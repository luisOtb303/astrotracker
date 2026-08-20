#include "stills/PhotoExportWorker.h"

#include "common/AppLog.h"
#include "processing/BorderHandler.h"
#include "video/FFmpegVideoWriter.h"

#include <QDir>
#include <QSize>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <memory>

namespace {

// Los RAW devueltos por readFullRes pueden ser de 16 bits; se convierten a
// BGR8 para el centrado y la codificación.
cv::Mat toBgr8(const cv::Mat& m)
{
    if (m.empty())
        return m;
    cv::Mat out;
    if (m.depth() != CV_8U) {
        const double alpha = (m.depth() == CV_16U) ? 1.0 / 256.0 : 1.0;
        m.convertTo(out, CV_8U, alpha);
    } else {
        out = m;
    }
    if (out.channels() == 1)
        cv::cvtColor(out, out, cv::COLOR_GRAY2BGR);
    return out;
}

QSize canvasOf(PhotoExportWorker::Resolution r)
{
    switch (r) {
    case PhotoExportWorker::Resolution::HD:
        return QSize(1280, 720);
    case PhotoExportWorker::Resolution::FHD:
        return QSize(1920, 1080);
    case PhotoExportWorker::Resolution::QHD:
        return QSize(2560, 1440);
    case PhotoExportWorker::Resolution::UHD:
        return QSize(3840, 2160);
    default:
        return QSize();
    }
}

// Escala `src` para que quepa en el lienzo y rellena con negro hasta el tamaño
// exacto del lienzo (mantiene la relación de aspecto). Los vídeos exigen un
// tamaño fijo para todos los frames.
cv::Mat fitToCanvas(const cv::Mat& src, const QSize& canvas)
{
    if (canvas.isEmpty())
        return src.clone();
    if (src.cols == canvas.width() && src.rows == canvas.height())
        return src.clone();
    const double s = std::min(static_cast<double>(canvas.width()) / src.cols,
                              static_cast<double>(canvas.height()) / src.rows);
    cv::Mat scaled;
    cv::resize(src, scaled, cv::Size(), s, s, cv::INTER_AREA);
    cv::Mat out(canvas.height(), canvas.width(), src.type(), cv::Scalar::all(0));
    scaled.copyTo(out(cv::Rect((out.cols - scaled.cols) / 2,
                               (out.rows - scaled.rows) / 2,
                               scaled.cols, scaled.rows)));
    return out;
}

// Brillo medio de la zona central (donde está el disco al centrar). Se usa
// como referencia para normalizar el brillo y evitar el parpadeo entre fotos.
double centralMean(const cv::Mat& bgr)
{
    cv::Mat g;
    cv::cvtColor(bgr, g, cv::COLOR_BGR2GRAY);
    const int x0 = g.cols * 3 / 10;
    const int y0 = g.rows * 3 / 10;
    const int w = g.cols * 4 / 10;
    const int h = g.rows * 4 / 10;
    return cv::mean(g(cv::Rect(x0, y0, w, h)))[0];
}

} // namespace

void PhotoExportWorker::run()
{
    std::vector<std::string> paths;
    paths.reserve(static_cast<size_t>(paths_.size()));
    for (const QString& p : paths_)
        paths.push_back(p.toStdString());

    PhotoSequenceReader reader;
    if (!reader.open(paths)) {
        emit finished(false, QStringLiteral("No se pudo abrir la secuencia"), 0);
        return;
    }
    const int64_t n = reader.count();
    if (n <= 0) {
        emit finished(false, QStringLiteral("Secuencia vacía"), 0);
        return;
    }

    const bool toVideo = settings_.format == Format::Mp4;
    const QString outDesc = toVideo ? settings_.outFile
                                    : QDir(settings_.outDir).filePath(
                                          QStringLiteral("centrada_0000.%1")
                                              .arg(settings_.format == Format::Png ? QStringLiteral("png")
                                                                                   : QStringLiteral("jpg")));
    AppLog::info(QStringLiteral("Exportando %1 fotos como %2 → %3")
                     .arg(n)
                     .arg(toVideo ? QStringLiteral("MP4") : QStringLiteral("imágenes"))
                     .arg(outDesc));
    QSize canvas = canvasOf(settings_.resolution);
    if (toVideo && canvas.isEmpty()) {
        // Vídeo sin lienzo estándar: se fija el tamaño de la primera foto para
        // que todos los frames tengan el mismo tamaño.
        cv::Mat f0;
        if (settings_.resolution == Resolution::Original)
            reader.readFullRes(0, f0);
        else
            reader.readAt(0, f0, analysisDim_);
        canvas = QSize(f0.cols, f0.rows);
    }

    std::unique_ptr<FFmpegVideoWriter> writer;
    if (toVideo) {
        writer = std::make_unique<FFmpegVideoWriter>();
        if (!writer->open(settings_.outFile.toStdString(), canvas.width(), canvas.height(),
                          settings_.fps)) {
            emit finished(false, QStringLiteral("No se pudo crear el vídeo MP4"), 0);
            return;
        }
    } else {
        QDir().mkpath(settings_.outDir);
    }

    const BorderMode mode = settings_.borderMode == 1 ? BorderMode::Replicate : BorderMode::Black;
    const int total = static_cast<int>(n);
    const int interp = toVideo ? std::max(0, settings_.interp) : 0;
    const bool normBright = toVideo && settings_.normalizeBrightness;
    double refMean = 0.0;
    bool haveRef = false;
    cv::Mat prevOut;
    bool havePrev = false;
    int done = 0;
    int written = 0;

    for (int64_t i = 0; i < n; ++i) {
        if (stop_.load())
            break;

        emit photoProcessed(i);
        const bool selected =
            selection_.empty() || (i < static_cast<int64_t>(selection_.size()) &&
                                   selection_[static_cast<size_t>(i)]);
        if (!selected)
            continue;
        AppLog::info(QStringLiteral("procesando %1 → centrando")
                         .arg(QString::fromStdString(reader.fileName(i))));

        cv::Mat work;
        double scale = 1.0;
        if (settings_.resolution == Resolution::Original) {
            if (!reader.readFullRes(i, work)) {
                AppLog::warn(QStringLiteral("no se pudo leer la foto %1").arg(i + 1));
                continue;
            }
            work = toBgr8(work);
            const double nativeMax = std::max(work.cols, work.rows);
            scale = (analysisDim_ > 0 && nativeMax > 0) ? nativeMax / analysisDim_ : 1.0;
        } else {
            if (!reader.readAt(i, work, analysisDim_)) {
                AppLog::warn(QStringLiteral("no se pudo leer la foto %1").arg(i + 1));
                continue;
            }
        }

        // Centrado igual que el visor; las fotos sin resultado válido se
        // exportan sin desplazar (regla: nunca descartar frames).
        cv::Point2f center(0.f, 0.f);
        float radius = 0.f;
        if (i < static_cast<int64_t>(tracks_.size()) &&
            tracks_[static_cast<size_t>(i)].radius > 0.f) {
            center = tracks_[static_cast<size_t>(i)].center * static_cast<float>(scale);
            radius = tracks_[static_cast<size_t>(i)].radius * static_cast<float>(scale);
        }
        const cv::Point2f offset(work.cols / 2.0f - center.x, work.rows / 2.0f - center.y);
        cv::Mat out = BorderHandler::apply(work, offset, mode);

        // Normalización de brillo: todas las fotos al brillo medio de la
        // primera, para que la transición no "parpadee".
        if (normBright) {
            const double m = centralMean(out);
            if (!haveRef) {
                refMean = m;
                haveRef = true;
            } else if (refMean > 5.0 && m > 1.0) {
                const double k = refMean / m;
                if (std::abs(k - 1.0) > 0.02)
                    out = out * k;
            }
        }

        if (toVideo) {
            // Transiciones suavizadas: fotogramas intermedios entre esta foto y
            // la anterior, como fundido cruzado de los dos frames ya centrados
            // (el disco queda en el centro en todo momento; solo el fondo se
            // desliza).
            if (havePrev && interp > 0) {
                for (int j = 1; j <= interp; ++j) {
                    const double t = static_cast<double>(j) / (interp + 1);
                    const double u = t * t * (3.0 - 2.0 * t); // smoothstep
                    cv::Mat f;
                    cv::addWeighted(prevOut, 1.0 - u, out, u, 0.0, f);
                    f = fitToCanvas(f, canvas);
                    if (writer->write(f))
                        ++written;
                }
            }
            cv::Mat base = fitToCanvas(out, canvas);
            if (writer->write(base))
                ++written;
            AppLog::info(QStringLiteral("  frame %1/%2 → vídeo").arg(i + 1).arg(n));
        } else {
            out = fitToCanvas(out, canvas);
            const QString name = QStringLiteral("centrada_%1.%2")
                                     .arg(static_cast<long long>(i), 4, 10, QLatin1Char('0'))
                                     .arg(settings_.format == Format::Png ? QStringLiteral("png")
                                                                          : QStringLiteral("jpg"));
            const QString path = QDir(settings_.outDir).filePath(name);
            const std::vector<int> params = settings_.format == Format::Png
                                                ? std::vector<int>{}
                                                : std::vector<int>{cv::IMWRITE_JPEG_QUALITY, 95};
            if (cv::imwrite(path.toStdString(), out, params))
                ++written;
            AppLog::info(QStringLiteral("  escrito %1").arg(path));
        }

        prevOut = out;
        havePrev = true;

        ++done;
        emit progress(done, total);
    }

    if (toVideo) {
        writer->close();
        written = writer->frameCount();
    }

    const bool stopped = stop_.load();
    if (written > 0)
        AppLog::info(QStringLiteral("Exportación finalizada: %1 fotos").arg(written));
    emit finished(!stopped, stopped ? QStringLiteral("Exportación detenida") : QString(),
                  written);
}