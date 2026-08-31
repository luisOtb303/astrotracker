#include "stills/VideoExportWorker.h"

#include "common/AppLog.h"
#include "processing/BorderHandler.h"
#include "video/FFmpegVideoReader.h"
#include "video/FFmpegVideoWriter.h"

#include <QDir>
#include <QSize>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <memory>

namespace {

cv::Mat toBgr8(const cv::Mat& m)
{
    if (m.empty())
        return m;
    cv::Mat out;
    if (m.depth() != CV_8U) {
        const double alpha = (m.depth() == CV_16U) ? 1.0 / 257.0 : 1.0;
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

constexpr double kNormMaxGain = 3.0;

} // namespace

void VideoExportWorker::run()
{
    FFmpegVideoReader reader;
    if (!reader.open(inPath_.toStdString())) {
        emit finished(false, QStringLiteral("No se pudo abrir el vídeo"), 0);
        return;
    }

    const int64_t n = std::max<int64_t>(reader.frameCount(), 0);
    const bool toVideo = settings_.format == Format::Mp4;
    AppLog::info(QStringLiteral("Exportando vídeo (%1 fotogramas) como %2 → %3")
                     .arg(n)
                     .arg(toVideo ? QStringLiteral("MP4") : QStringLiteral("imágenes"))
                     .arg(toVideo ? settings_.outFile
                                  : QDir(settings_.outDir).filePath(
                                        QStringLiteral("frame_0000.%1")
                                            .arg(settings_.format == Format::Png
                                                     ? QStringLiteral("png")
                                                     : QStringLiteral("jpg")))));

    QSize canvas = canvasOf(settings_.resolution);
    if (toVideo && canvas.isEmpty())
        canvas = QSize(reader.width(), reader.height());

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
    const int interp = toVideo ? std::max(0, settings_.interp) : 0;
    const bool normBright = toVideo && settings_.normalizeBrightness;
    const double scale = (canvas.isEmpty())
                             ? 1.0
                             : std::min(static_cast<double>(canvas.width()) / reader.width(),
                                        static_cast<double>(canvas.height()) / reader.height());
    // Escala de los offsets (coordenadas de frame original a las del visor).
    const double offScale =
        (canvas.isEmpty()) ? 1.0
                           : std::min(static_cast<double>(canvas.width()) / reader.width(),
                                      static_cast<double>(canvas.height()) / reader.height());

    double refMean = 0.0;
    bool haveRef = false;
    cv::Mat prevOut;
    bool havePrev = false;
    int64_t frameIndex = 0;
    int written = 0;

    Frame frame;
    while (reader.readNext(frame)) {
        if (stop_.load())
            break;

        emit frameProcessed(frameIndex);

        AppLog::info(QStringLiteral("fotograma %1/%2%3")
                         .arg(frameIndex + 1)
                         .arg(n)
                         .arg(offsets_.empty() ? QStringLiteral(" → directo")
                                               : QStringLiteral(" → centrando")));

        cv::Mat work = toBgr8(frame.image);
        cv::Mat out;
        if (!offsets_.empty() && frameIndex < static_cast<int64_t>(offsets_.size())) {
            const cv::Point2f off = offsets_[static_cast<size_t>(frameIndex)] * offScale;
            out = BorderHandler::apply(work, off, mode);
        } else {
            out = work;
        }

        if (normBright) {
            const double m = centralMean(out);
            if (!haveRef) {
                refMean = m;
                haveRef = true;
            } else if (refMean > 5.0 && m > 1.0) {
                const double k = refMean / m;
                const double capped = std::clamp(k, 1.0 / kNormMaxGain, kNormMaxGain);
                if (std::abs(capped - 1.0) > 0.02)
                    out = out * capped;
            }
        }

        if (toVideo) {
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
        } else {
            out = fitToCanvas(out, canvas);
            const QString name = QStringLiteral("frame_%1.%2")
                                     .arg(static_cast<long long>(frameIndex), 5, 10,
                                          QLatin1Char('0'))
                                     .arg(settings_.format == Format::Png ? QStringLiteral("png")
                                                                          : QStringLiteral("jpg"));
            const QString path = QDir(settings_.outDir).filePath(name);
            const std::vector<int> params = settings_.format == Format::Png
                                                ? std::vector<int>{}
                                                : std::vector<int>{cv::IMWRITE_JPEG_QUALITY, 95};
            if (cv::imwrite(path.toStdString(), out, params))
                ++written;
        }

        prevOut = out;
        havePrev = true;

        ++frameIndex;
        emit progress(static_cast<int>(frameIndex), static_cast<int>(n));
    }

    if (toVideo) {
        writer->close();
        written = writer->frameCount();
    }

    const bool stopped = stop_.load();
    if (written > 0)
        AppLog::info(QStringLiteral("Exportación de vídeo finalizada: %1 elementos").arg(written));
    emit finished(!stopped, stopped ? QStringLiteral("Exportación detenida") : QString(),
                  written);
}
