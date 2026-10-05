#include "common/ImageAdjust.h"
#include "raw/RawDecoder.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {
int fail(const std::string& what, bool fatal = true)
{
    std::cerr << "FALLO: " << what << "\n";
    return fatal ? 1 : 0;
}

// El filtro de píxeles calientes cambia la imagen, pero muy poco: con los
// valores por defecto corrige del orden de ~150 fotositos en un sensor de 18 Mpx
// (medido sobre los RAW de referencia), lo que tras demosaicar son unos pocos
// cientos de píxeles de los 18 millones. El test fija las dos cosas: que hace
// algo, y que no toca la foto.
// `expectCorrections` distingue las dos cosas que se comprueban aquí, porque no
// valen lo mismo para todos los RAW:
//
//   - que el filtro NO rompa la foto: universal. Si no es un Bayer 2x2, o si el
//     sensor no tiene pixeles quemados detectables, lo correcto es 0 cambios,
//     y eso no es un fallo. Un 40D en sRAW2, por ejemplo, llega con filters
//     0xB4E1E2D4 (Bayer valido) pero sin pixeles calientes detectables.
//   - que el filtro CORRIJA los pixeles quemados: solo se exige en las muestras
//     de testdata/hotpixel, que sí los tienen (11k y 2k correcciones).
int checkHotPixels(const std::string& path, bool expectCorrections = false)
{
    cv::Mat off, on;
    if (!RawDecoder::decode(path, off, true))
        return fail("decode sin hot(" + path + ")");

    RawDecoder::HotSettings hot;
    hot.enabled = true;
    hot.params.k = ImageAdjustLimits::hotSensitivityToSigma(50); // el default
    if (!RawDecoder::decode(path, on, true, 0, hot))
        return fail("decode con hot(" + path + ")");

    if (off.size() != on.size() || off.type() != on.type())
        return fail("el filtro cambia la geometria de la imagen");

    cv::Mat diff;
    cv::absdiff(off, on, diff);
    std::vector<cv::Mat> ch;
    cv::split(diff, ch);
    int changed = 0;
    for (const auto& c : ch)
        changed += cv::countNonZero(c);

    const long long total =
        static_cast<long long>(off.cols) * off.rows * off.channels();
    const double ppm = 100.0 * changed / static_cast<double>(total);

    std::cout << "  hot pixels: " << changed << " pixeles cambiados (" << ppm
              << " ppm del frame)\n";

    if (expectCorrections && changed == 0)
        return fail("el filtro no corrige nada en un RAW con defectos");
    // >1 ppm (18000 px en 18 Mpx) significaria que esta limpiando de mas.
    if (ppm > 1.0)
        return fail("el filtro corrige demasiado: " + std::to_string(changed) +
                    " pixeles no parecen un hot pixel filter");

    // Un umbral mas alto tiene que corregir menos o lo mismo.
    RawDecoder::HotSettings stricter = hot;
    stricter.params.k = ImageAdjustLimits::hotSensitivityToSigma(100);
    cv::Mat onStrict;
    if (!RawDecoder::decode(path, onStrict, true, 0, stricter))
        return fail("decode con hot k=30(" + path + ")");
    cv::Mat diff2;
    cv::absdiff(off, onStrict, diff2);
    std::vector<cv::Mat> ch2;
    cv::split(diff2, ch2);
    int changedStrict = 0;
    for (const auto& c : ch2)
        changedStrict += cv::countNonZero(c);
    std::cout << "  hot pixels (k=30): " << changedStrict << "\n";
    if (changedStrict > changed)
        return fail("un umbral mas alto corrige mas pixeles que uno mas bajo");

    return 0;
}

int checkOne(const std::string& path, int expectW, int expectH)
{
    std::cout << "[archivo] " << path << "\n";

    if (!RawDecoder::isRawFile(path))
        return fail("isRawFile(" + path + ")");

    int w = 0, h = 0;
    if (!RawDecoder::dimensions(path, w, h))
        return fail("dimensions(" + path + ")");
    std::cout << "  dimensions: " << w << "x" << h << "\n";
    if (expectW > 0 && (w != expectW || h != expectH))
        return fail("dimensions esperadas " + std::to_string(expectW) + "x" +
                        std::to_string(expectH),
                    true);

    cv::Mat thumb;
    if (!RawDecoder::thumbnail(path, thumb))
        return fail("thumbnail(" + path + ")");
    std::cout << "  thumbnail: " << thumb.cols << "x" << thumb.rows
              << " tipo=" << thumb.type() << "\n";
    if (thumb.empty() || thumb.depth() != CV_8U || thumb.channels() < 3)
        return fail("thumbnail no es BGR");

    cv::Mat full8;
    if (!RawDecoder::decode(path, full8, false, 1024))
        return fail("decode 8-bit(" + path + ")");
    std::cout << "  decode8: " << full8.cols << "x" << full8.rows
              << " tipo=" << full8.type() << "\n";
    if (full8.empty() || full8.depth() != CV_8U || full8.channels() != 3)
        return fail("decode8 no es BGR8");

    cv::Mat full16;
    if (!RawDecoder::decode(path, full16, true, 1024))
        return fail("decode 16-bit(" + path + ")");
    std::cout << "  decode16: " << full16.cols << "x" << full16.rows
              << " tipo=" << full16.type() << "\n";
    if (full16.empty() || full16.depth() != CV_16U || full16.channels() != 3)
        return fail("decode16 no es BGR16");

    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i)
        paths.emplace_back(argv[i]);

    if (paths.empty()) {
        std::cerr << "uso: test_raw <archivo1.raw> [archivo2.raw ...]\n";
        return 2;
    }

    int failures = 0;
    for (const auto& p : paths) {
        failures += checkOne(p, 0, 0);
        // Solo las muestras de testdata/hotpixel tienen los pixeles quemados que
        // hay que corregir; el resto solo tiene que salir intacto.
        const bool expectCorrections = p.find("hotpixel") != std::string::npos;
        failures += checkHotPixels(p, expectCorrections);
    }

    if (failures) {
        std::cerr << failures << " archivo(s) con fallos\n";
        return 1;
    }
    std::cout << "OK: " << paths.size() << " RAW decodificados\n";
    return 0;
}