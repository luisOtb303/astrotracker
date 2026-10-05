#include "raw/RawDecoder.h"

#include "libraw/libraw.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <vector>

bool RawDecoder::isRawFile(const std::string& path)
{
    LibRaw raw;
    const int rc = raw.open_file(path.c_str());
    raw.recycle();
    return rc == LIBRAW_SUCCESS;
}

bool RawDecoder::dimensions(const std::string& path, int& w, int& h)
{
    LibRaw raw;
    if (raw.open_file(path.c_str()) != LIBRAW_SUCCESS)
        return false;
    w = raw.imgdata.sizes.width;
    h = raw.imgdata.sizes.height;
    raw.recycle();
    return w > 0 && h > 0;
}

// Aplica el filtro de píxeles calientes sobre el mosaico YA DESEMPAQUETADO,
// antes de que dcraw_process() lo debayere. Devuelve true si hizo algo.
//
// El mosaico de LibRaw es un buffer plano de uint16 con paso raw_width, y el
// área activa son los rangos [top_margin, top_margin+height) x
// [left_margin, left_margin+width). Se envuelve en una cv::Mat SIN copiar: el
// filtro recorre el mosaico con ptr<uint16_t>(fila), que ya respeta el paso, así
// que corregir a través de la vista es idéntico a hacerlo sobre una copia y
// evita mover 36 MB por foto solo para poder escribir.
static bool applyHotPixelFilter(LibRaw& raw, const RawDecoder::HotSettings& hot)
{
    if (!hot.enabled || raw.imgdata.rawdata.raw_image == nullptr)
        return false;

    // X-Trans (filters == 9) y Foveon no son un Bayer 2x2: el reparto en cuatro
    // clases de paridad no significaría nada y se omite en lugar de aplicarse a
    // ciegas.
    if (raw.imgdata.idata.filters == 9 || raw.imgdata.idata.is_foveon)
        return false;
    if (raw.imgdata.idata.colors != 3)
        return false;

    const libraw_image_sizes_t& s = raw.imgdata.sizes;
    if (s.width == 0 || s.height == 0)
        return false;
    if (s.left_margin + s.width > s.raw_width || s.top_margin + s.height > s.raw_height)
        return false;

    // Deriva el id de color de cada clase de paridad con FC(), que es pública
    // (libraw.h) y devuelve el código CFA 0..3. Solo hacen falta las cuatro
    // esquinas: el patrón se repite cada 2.
    //
    // OJO con FC(): indexa `filters` con
    //     (filters >> ((((row << 1) & 14) | (col & 1)) << 1)) & 3
    // así que el desplazamiento máximo es 30 y `filters` es un entero de 32 bits.
    // Pedirle una fila del borde de la imagen (top_margin puede ser ~2000) se sale
    // del patrón por arriba, el >> se sale del entero y devuelve basura. Aquí se
    // vio como cuatro colores distintos, que es justo lo que el validador de
    // mosaico rechaza, con el filtro entero desactivado sin error visible. Se le
    // pide la misma posición (módulo 8 en fila, módulo 2 en columna), que devuelve
    // el mismo patrón sin desbordar.
    //
    // La fase concreta es irrelevante para el detector, que solo agrupa por
    // paridad; esto es un chequeo de que lo que llega es de verdad un Bayer 2x2.
    const int r0 = s.top_margin & 7;
    const int c0 = s.left_margin & 1;
    int phase[2][2];
    for (int py = 0; py < 2; ++py) {
        for (int px = 0; px < 2; ++px)
            phase[py][px] = raw.FC(r0 + py, c0 + px);
    }

    const size_t step = static_cast<size_t>(s.raw_width) * sizeof(uint16_t);
    cv::Mat mosaic(
        s.height, s.width, CV_16UC1,
        reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(raw.imgdata.rawdata.raw_image) +
            static_cast<size_t>(s.top_margin) * step +
            static_cast<size_t>(s.left_margin) * sizeof(uint16_t)),
        step);

    return raw::removeHotPixels(mosaic, phase, hot.params).corrected > 0;
}

bool RawDecoder::decode(const std::string& path, cv::Mat& out, bool want16, int maxDim,
                        const HotSettings& hot)
{
    LibRaw raw;
    if (raw.open_file(path.c_str()) != LIBRAW_SUCCESS)
        return false;
    if (raw.unpack() != LIBRAW_SUCCESS) {
        raw.recycle();
        return false;
    }

    // Sobre el mosaico, antes de demosaicar. Es el único punto donde el defecto
    // conserva su amplitud; después el debayer lo reparte sobre 4 píxeles y el
    // criterio local deja de poder separarlo de una estrella.
    applyHotPixelFilter(raw, hot);

    // Procesado determinista y fiel: sin el auto-brillo de dcraw, que ajustaba
    // la ganancia según el histograma de CADA foto (las oscuras salían
    // amplificadas, con el ruido de croma convertido en moteado naranja) y con
    // demosaicing completo (half_size se lo saltaba y dejaba artefactos de
    // color). El coste extra lo absorbe la caché de análisis.
    raw.imgdata.params.output_bps = want16 ? 16 : 8;
    raw.imgdata.params.no_auto_bright = 1;

    if (raw.dcraw_process() != LIBRAW_SUCCESS) {
        raw.recycle();
        return false;
    }

    int err = LIBRAW_SUCCESS;
    libraw_processed_image_t* img = raw.dcraw_make_mem_image(&err);
    if (!img || err != LIBRAW_SUCCESS) {
        if (img)
            raw.dcraw_clear_mem(img);
        raw.recycle();
        return false;
    }

    const int cn = (img->colors == 4) ? 4 : 3;
    const int depth = (img->bits == 16) ? CV_16U : CV_8U;
    cv::Mat m(img->height, img->width, CV_MAKETYPE(depth, cn), img->data);
    m = m.clone();

    raw.dcraw_clear_mem(img);
    raw.recycle();

    if (cn == 4)
        cv::cvtColor(m, m, cv::COLOR_RGBA2BGR);
    else if (cn == 3)
        cv::cvtColor(m, m, cv::COLOR_RGB2BGR);

    if (maxDim > 0) {
        const int longest = std::max(m.cols, m.rows);
        if (longest > maxDim) {
            const double scale = static_cast<double>(maxDim) / longest;
            cv::resize(m, m, cv::Size(), scale, scale, cv::INTER_AREA);
        }
    }

    out = m;
    return true;
}

bool RawDecoder::thumbnail(const std::string& path, cv::Mat& out, int maxDim)
{
    LibRaw raw;
    if (raw.open_file(path.c_str()) != LIBRAW_SUCCESS)
        return false;
    if (raw.unpack_thumb() != LIBRAW_SUCCESS) {
        raw.recycle();
        return false;
    }

    const libraw_thumbnail_t& t = raw.imgdata.thumbnail;
    cv::Mat thumb;
    if (t.tformat == LIBRAW_THUMBNAIL_JPEG && t.thumb && t.tlength > 0) {
        const cv::Mat buf(1, static_cast<int>(t.tlength), CV_8U,
                          static_cast<void*>(t.thumb));
        thumb = cv::imdecode(buf, cv::IMREAD_COLOR);
    } else if (t.tformat == LIBRAW_THUMBNAIL_BITMAP && t.thumb && t.twidth > 0) {
        thumb = cv::Mat(t.theight, t.twidth, CV_8UC3, static_cast<void*>(t.thumb))
                    .clone();
    } else {
        raw.recycle();
        return false;
    }

    raw.recycle();
    if (thumb.empty())
        return false;

    if (maxDim > 0) {
        const int longest = std::max(thumb.cols, thumb.rows);
        if (longest > maxDim) {
            const double scale = static_cast<double>(maxDim) / longest;
            cv::resize(thumb, thumb, cv::Size(), scale, scale, cv::INTER_AREA);
        }
    }

    out = thumb;
    return true;
}