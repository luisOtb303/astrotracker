#pragma once

#include "common/CircleF.h"
#include "stills/PhotoSequenceReader.h"
#include "tracking/DiscTracker.h"

#include <QStringList>
#include <QThread>
#include <QVector>
#include <atomic>

// Worker en hilo separado que sigue el disco foto a foto sobre una secuencia
// abierta localmente (no comparte el lector de la UI). Parte de la foto en la
// que se sembró el círculo y recorre la secuencia en ambas direcciones; el
// resultado por foto se empaqueta como QVector<double> con zancada 5:
// (cx, cy, radio, status, predicted). La UI no se bloquea.
class PhotoTrackWorker : public QThread
{
    Q_OBJECT

public:
    PhotoTrackWorker(const QStringList& paths, const CircleF& seed, int64_t seedIndex,
                     int analysisDim, const DiscTrackerParams& params,
                     QObject* parent = nullptr)
        : QThread(parent)
        , paths_(paths)
        , seed_(seed)
        , seedIndex_(seedIndex)
        , analysisDim_(analysisDim)
        , params_(params)
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
        res.fill(0.0, static_cast<int>(n) * 5);
        const int64_t start = std::min(std::max<int64_t>(seedIndex_, 0), n - 1);

        const auto trackOne = [this, &reader, &res](int64_t i, DiscTracker& tracker) {
            cv::Mat frame;
            if (reader.readAt(i, frame, analysisDim_)) {
                const DiscTrack t = tracker.track(frame);
                const int k = static_cast<int>(i) * 5;
                res[k] = t.center.x;
                res[k + 1] = t.center.y;
                res[k + 2] = t.radius;
                res[k + 3] = static_cast<double>(static_cast<int>(t.status));
                res[k + 4] = t.predicted ? 1.0 : 0.0;
            } else {
                const int k = static_cast<int>(i) * 5;
                res[k + 3] = static_cast<double>(static_cast<int>(TrackStatus::LOST));
                res[k + 4] = 1.0;
            }
        };

        int done = 0;
        auto emitProgress = [this, &done, n]() {
            ++done;
            emit progress(done, static_cast<int>(n));
        };

        // Pasada hacia delante desde la semilla.
        DiscTracker forward(params_);
        forward.init(seed_.center, seed_.radius);
        for (int64_t i = start; i < n; ++i) {
            if (stop_.load())
                break;
            trackOne(i, forward);
            emitProgress();
        }

        // Pasada hacia atrás desde la semilla.
        DiscTracker backward(params_);
        backward.init(seed_.center, seed_.radius);
        for (int64_t i = start - 1; i >= 0 && !stop_.load(); --i) {
            trackOne(i, backward);
            emitProgress();
        }

        emit finished(!stop_.load(), QString(), res);
    }

signals:
    void progress(int done, int total);
    void finished(bool ok, const QString& error, const QVector<double>& results);

private:
    QStringList paths_;
    CircleF seed_;
    int64_t seedIndex_ = 0;
    int analysisDim_ = 0;
    DiscTrackerParams params_;
    std::atomic<bool> stop_{false};
};