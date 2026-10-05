#include "processing/HotPixelMap.h"

#include "common/ImageAdjust.h"
#include "video/IVideoReader.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <algorithm>
#include <opencv2/imgproc.hpp>

HotPixelMap::Result HotPixelMap::fromVideo(IVideoReader& reader, const Options& opts)
{
    Result res;
    if (!reader.isOpen()) {
        res.error = QStringLiteral("no hay ningun video abierto");
        return res;
    }

    // Un vídeo de un solo fotograma no tiene contra qué comparar: el mapa
    // temporal se basa en repetir en la misma posición, así que sin al menos dos
    // tomas el resultado no significa nada. Sin esta guarda, un lector que
    // entrega el mismo frame en cada seek daría un mapa falso y lleno de estrellas.
    if (reader.frameCount() > 0 && reader.frameCount() < 2) {
        res.error = QStringLiteral("el video tiene un solo fotograma");
        return res;
    }

    const int64_t duration = reader.durationUs();
    const int64_t total = reader.frameCount();
    const int want = std::max(2, opts.sampleCount);
    const int n =
        static_cast<int>(std::max<int64_t>(2, total > 0 ? std::min<int64_t>(total, want) : want));

    // Aciertos por píxel: se cuenta una vez por fotograma (ya viene unificado
    // entre canales desde img::detectHotPixels), no por canal.
    cv::Mat hits;
    int read = 0;

    for (int i = 0; i < n; ++i) {
        if (opts.cancel && opts.cancel->load()) {
            res.cancelled = true;
            break;
        }

        // Las muestras se reparten por toda la duración, extremos incluidos:
        // un defecto del sensor no se va con el tiempo.
        const int64_t us =
            (n > 1 && duration > 0)
                ? static_cast<int64_t>(static_cast<double>(i) / (n - 1) * duration)
                : 0;

        if (i > 0 && !reader.seekToUs(us))
            break;

        Frame frame;
        if (!reader.readNext(frame) || frame.image.empty())
            break;

        cv::Mat cand = img::detectHotPixels(frame.image, opts.sensitivity);
        if (cand.empty())
            continue; // profundidad no soportada: no cuenta como acierto

        if (hits.empty() || hits.size() != cand.size())
            hits = cv::Mat::zeros(cand.size(), CV_32S);

        // OJO: cand es CV_8U con 255 en los aciertos, y sumarlo tal cual a un
        // CV_32S acumula 255 POR FOTOGRAMA, no 1. Con minHitRatio = 0.7 el
        // umbral queda en 0.7 * 255 * n, o sea ~178*n, y TODOS los candidatos
        // pasan el filtro en el primer fotograma: el mapa acaba marcando la
        // estrella que se mueve igual que los quemados, que es justo lo que
        // este detector existe para no hacer. Hay que sumar 1 por acierto, y
        // comparar contra 0 no basta porque el resultado sigue siendo 0/255.
        cv::Mat mark;
        cv::compare(cand, 0, mark, cv::CMP_NE);
        cv::Mat ones;
        mark.convertTo(ones, CV_32S, 1.0 / 255.0);
        hits += ones;
        ++read;

        if (opts.progress)
            opts.progress(read, n);

        // Cede el hilo de vez en cuando para que la ventana siga respondiendo.
        if ((i % 4) == 3)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }

    res.sampled = read;
    // Cancelado significa cancelado: si el usuario pulsó cancelar o se cambió de
    // pestaña, un mapa "a medias" sería peor que no tener mapa, porque se
    // aplicaría a la exportación creyendo que está respaldado por todos los
    // fotogramas. El mapa parcial se descarta y se devuelve el error.
    if (res.cancelled) {
        res.error = QStringLiteral("analisis cancelado");
        return res;
    }
    if (read < 2) {
        if (res.error.isEmpty())
            res.error = QStringLiteral("no se han podido leer fotogramas suficientes");
        return res;
    }

    // Solo los píxeles que se repiten en casi todos los fotogramas son defecto
    // fijo. Una estrella cae en un sitio distinto en cada toma (seeing, deriva
    // del trípode, registro) y no llega al umbral de aciertos.
    const double need = opts.minHitRatio * read;
    cv::Mat hot;
    cv::compare(hits, need, hot, cv::CMP_GE);

    res.hotCount = cv::countNonZero(hot);
    if (res.hotCount > 0 && opts.dilate > 0) {
        const cv::Mat k = cv::getStructuringElement(
            cv::MORPH_RECT, cv::Size(2 * opts.dilate + 1, 2 * opts.dilate + 1));
        cv::dilate(hot, hot, k);
    }

    res.mask = hot;
    res.ok = true;
    return res;
}

cv::Mat HotPixelMap::correct(const cv::Mat& frame, const Mask& mask)
{
    return img::correctHotPixels(frame, mask);
}

HotPixelMap::Mask HotPixelMap::detectSpatial(const cv::Mat& frame, int sensitivity)
{
    return img::detectHotPixels(frame, sensitivity);
}