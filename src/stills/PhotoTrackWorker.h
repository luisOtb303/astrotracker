#pragma once

#include "common/AppLog.h"
#include "common/CircleF.h"
#include "stills/PhotoSequenceReader.h"
#include "tracking/DiscArcFit.h"
#include "tracking/DiscTracker.h"

#include <QStringList>
#include <QThread>
#include <QVector>
#include <atomic>
#include <map>
#include <opencv2/imgproc.hpp>

// Worker en hilo separado que sigue el disco foto a foto sobre una secuencia
// abierta localmente (no comparte el lector de la UI). Dos modos:
//
// 1. Foto a foto (photoByPhoto_=true, por defecto): ejecuta DiscArcFit en cada
//    foto de forma independiente. Si la detección falla, usa la foto anterior
//    como fallback. No necesita semilla (se auto-detecta).
//
// 2. Tracking secuencial (photoByPhoto_=false): usa DiscTracker con semilla,
//    pasada forward y backward. Modo legacy.
//
// El resultado por foto se empaqueta como QVector<double> con zancada 7:
// (cx, cy, radio, status, predicted, confianza, método).
class PhotoTrackWorker : public QThread
{
    Q_OBJECT

public:
    PhotoTrackWorker(const QStringList& paths, const CircleF& seed, int64_t seedIndex,
                     int analysisDim, const DiscTrackerParams& params,
                     const TrackingProfile& profile,
                     const std::map<int64_t, DiscMethod>& methodOverrides,
                     const std::vector<bool>& locked = {},
                     QObject* parent = nullptr,
                     bool photoByPhoto = true)
        : QThread(parent)
        , paths_(paths)
        , seed_(seed)
        , seedIndex_(seedIndex)
        , analysisDim_(analysisDim)
        , params_(params)
        , profile_(profile)
        , overrides_(methodOverrides)
        , locked_(locked)
        , photoByPhoto_(photoByPhoto)
    {
    }

    void requestStop() { stop_.store(true); }

    void run() override
    {
        std::vector<std::string> paths;
        paths.reserve(static_cast<size_t>(paths_.size()));
        for (const QString& p : paths_)
            paths.push_back(p.toStdString());

        PhotoSequenceReader reader;
        if (!reader.open(paths)) {
            emit finished(false, QStringLiteral("No se pudo abrir la secuencia"), QVector<double>());
            return;
        }

        const int64_t n = reader.count();
        if (n <= 0) {
            emit finished(false, QStringLiteral("Secuencia vacía"), QVector<double>());
            return;
        }

        QVector<double> res;
        res.fill(0.0, static_cast<int>(n) * 7);

        if (photoByPhoto_)
            runPhotoByPhoto(reader, res, n);
        else
            runSequential(reader, res, n);

        emit finished(!stop_.load(), QString(), res);
    }

signals:
    void progress(int done, int total);
    void finished(bool ok, const QString& error, const QVector<double>& results);
    void reacquired(int64_t index, int predictedBefore);
    void photoProcessed(int64_t index);

private:
    // ── Modo foto a foto: DiscArcFit independiente por foto ──────────────

    void runPhotoByPhoto(PhotoSequenceReader& reader, QVector<double>& res, int64_t n)
    {
        cv::Point2f prevCenter = seed_.center;
        float prevRadius = seed_.radius;
        bool hasPrev = (seed_.radius > 0.f);

        for (int64_t i = 0; i < n && !stop_.load(); ++i) {
            trackOnePhotoByPhoto(i, reader, res, n, prevCenter, prevRadius, hasPrev);
            emit progress(static_cast<int>(i + 1), static_cast<int>(n));
        }
    }

    void trackOnePhotoByPhoto(int64_t i, PhotoSequenceReader& reader,
                              QVector<double>& res, int64_t n,
                              cv::Point2f& prevCenter, float& prevRadius, bool& hasPrev)
    {
        const bool locked = locked_.size() > static_cast<size_t>(i) && locked_[i];
        emit photoProcessed(i);

        cv::Mat frame;
        if (!reader.readAt(i, frame, analysisDim_)) {
            if (!locked) {
                AppLog::warn(QStringLiteral("no se pudo leer la foto %1 · %2")
                                 .arg(i + 1)
                                 .arg(QString::fromStdString(reader.fileName(i))));
                const int k = static_cast<int>(i) * 7;
                res[k + 3] = static_cast<double>(static_cast<int>(TrackStatus::LOST));
                res[k + 4] = 1.0;
            }
            return;
        }

        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(0, 0), 1.2);

        const cv::Rect full(0, 0, gray.cols, gray.rows);
        const float guess = hasPrev ? prevRadius
                                    : std::max(20.f, 0.10f * static_cast<float>(gray.cols));
        const cv::Point2f prior = hasPrev
            ? prevCenter
            : cv::Point2f(gray.cols * 0.5f, gray.rows * 0.5f);

        const DiscArcEstimate e = DiscArcFit::fitDisc(gray, full, prior, guess, 3.f);

        DiscTrack t;
        if (e.ok && e.support >= 12 && e.spanDeg >= 50.f) {
            t.center = e.center;
            t.radius = hasPrev ? prevRadius : e.radius;
            t.status = TrackStatus::VALID;
            t.predicted = false;
            t.confidence = std::min(1.0f, static_cast<float>(e.support) / 50.0f);
            t.method = DiscMethod::ArcBlob;

            if (hasPrev && std::abs(e.radius - prevRadius) > 5.0f)
                t.status = TrackStatus::UNCERTAIN;

            prevCenter = e.center;
            prevRadius = e.radius;
            hasPrev = true;
        } else if (hasPrev) {
            t.center = prevCenter;
            t.radius = prevRadius;
            t.status = TrackStatus::UNCERTAIN;
            t.predicted = true;
            t.confidence = 0.0f;
            t.method = DiscMethod::Prediction;
        } else {
            t.center = cv::Point2f(0, 0);
            t.radius = 0;
            t.status = TrackStatus::LOST;
            t.predicted = true;
            t.confidence = 0.0f;
            t.method = DiscMethod::Prediction;
        }

        const QString state = t.predicted
            ? QStringLiteral("supuesta")
            : (t.status == TrackStatus::VALID
                ? QStringLiteral("válida")
                : QStringLiteral("dudosa"));
        AppLog::info(
            QStringLiteral("foto %1/%2 · %3 → %4 (%5,%6,r%7) · %8 %9%")
                .arg(i + 1)
                .arg(n)
                .arg(QString::fromStdString(reader.fileName(i)))
                .arg(state)
                .arg(t.center.x, 0, 'f', 0)
                .arg(t.center.y, 0, 'f', 0)
                .arg(t.radius, 0, 'f', 0)
                .arg(QString::fromUtf8(methodName(t.method)))
                .arg(std::lround(t.confidence * 100.0f)));

        if (locked)
            return;

        const int k = static_cast<int>(i) * 7;
        res[k] = t.center.x;
        res[k + 1] = t.center.y;
        res[k + 2] = t.radius;
        res[k + 3] = static_cast<double>(static_cast<int>(t.status));
        res[k + 4] = t.predicted ? 1.0 : 0.0;
        res[k + 5] = t.confidence;
        res[k + 6] = static_cast<double>(static_cast<int>(t.method));
    }

    // ── Modo secuencial legacy: DiscTracker con semilla ──────────────────

    void runSequential(PhotoSequenceReader& reader, QVector<double>& res, int64_t n)
    {
        const int64_t start = std::min(std::max<int64_t>(seedIndex_, 0), n - 1);

        const auto methodFor = [this](int64_t i) {
            const auto it = overrides_.find(i);
            return it != overrides_.end() ? it->second : DiscMethod::Prediction;
        };

        const auto trackOne = [&](int64_t i, DiscTracker& tracker) {
            const bool locked = locked_.size() > static_cast<size_t>(i) && locked_[i];
            emit photoProcessed(i);
            cv::Mat frame;
            if (reader.readAt(i, frame, analysisDim_)) {
                tracker.setSingleMethod(methodFor(i));
                const DiscTrack t = tracker.track(frame);
                const QString state = t.predicted
                    ? QStringLiteral("supuesta")
                    : (t.status == TrackStatus::VALID
                        ? QStringLiteral("válida")
                        : QStringLiteral("dudosa"));
                AppLog::info(
                    QStringLiteral("foto %1/%2 · %3 → %4 (%5,%6,r%7) · %8 %9%")
                        .arg(i + 1)
                        .arg(n)
                        .arg(QString::fromStdString(reader.fileName(i)))
                        .arg(state)
                        .arg(t.center.x, 0, 'f', 0)
                        .arg(t.center.y, 0, 'f', 0)
                        .arg(t.radius, 0, 'f', 0)
                        .arg(QString::fromUtf8(methodName(t.method)))
                        .arg(std::lround(t.confidence * 100.0f)));
                if (t.reacquired) {
                    emit reacquired(i, t.predictedBefore);
                    AppLog::warn(
                        QStringLiteral("re-adquirido en la foto %1 tras %2 supuestas")
                            .arg(i + 1)
                            .arg(t.predictedBefore));
                }
                if (locked)
                    return;
                const int k = static_cast<int>(i) * 7;
                res[k] = t.center.x;
                res[k + 1] = t.center.y;
                res[k + 2] = t.radius;
                res[k + 3] = static_cast<double>(static_cast<int>(t.status));
                res[k + 4] = t.predicted ? 1.0 : 0.0;
                res[k + 5] = t.confidence;
                res[k + 6] = static_cast<double>(static_cast<int>(t.method));
            } else {
                if (locked)
                    return;
                AppLog::warn(QStringLiteral("no se pudo leer la foto %1 · %2")
                                 .arg(i + 1)
                                 .arg(QString::fromStdString(reader.fileName(i))));
                const int k = static_cast<int>(i) * 7;
                res[k + 3] = static_cast<double>(static_cast<int>(TrackStatus::LOST));
                res[k + 4] = 1.0;
            }
        };

        DiscTracker forward(params_);
        forward.setProfile(profile_);
        forward.init(seed_.center, seed_.radius);
        for (int64_t i = start; i < n && !stop_.load(); ++i) {
            trackOne(i, forward);
            emit progress(static_cast<int>(i - start + 1), static_cast<int>(n));
        }

        {
            DiscTracker backward(params_);
            backward.setProfile(profile_);
            backward.init(seed_.center, seed_.radius);
            for (int64_t i = start - 1; i >= 0 && !stop_.load(); --i) {
                trackOne(i, backward);
                emit progress(static_cast<int>(start - i + (n - start)), static_cast<int>(n));
            }
        }
    }

    // ── Miembros ─────────────────────────────────────────────────────────

    QStringList paths_;
    CircleF seed_;
    int64_t seedIndex_ = 0;
    int analysisDim_ = 0;
    DiscTrackerParams params_;
    TrackingProfile profile_;
    std::map<int64_t, DiscMethod> overrides_;
    std::vector<bool> locked_;
    bool photoByPhoto_ = true;
    std::atomic<bool> stop_{false};
};
