#pragma once

#include "common/AppLog.h"
#include "common/CircleF.h"
#include "stills/PhotoSequenceReader.h"
#include "tracking/DiscTracker.h"

#include <QStringList>
#include <QThread>
#include <QVector>
#include <atomic>
#include <map>

// Worker en hilo separado que sigue el disco foto a foto sobre una secuencia
// abierta localmente (no comparte el lector de la UI). Parte de la foto en la
// que se sembró el círculo y recorre la secuencia en ambas direcciones; el
// resultado por foto se empaqueta como QVector<double> con zancada 7:
// (cx, cy, radio, status, predicted, confianza, método). La UI no se bloquea.
class PhotoTrackWorker : public QThread
{
    Q_OBJECT

public:
    PhotoTrackWorker(const QStringList& paths, const CircleF& seed, int64_t seedIndex,
                     int analysisDim, const DiscTrackerParams& params,
                     const TrackingProfile& profile,
                     const std::map<int64_t, DiscMethod>& methodOverrides,
                     const std::vector<bool>& locked = {}, QObject* parent = nullptr)
        : QThread(parent)
        , paths_(paths)
        , seed_(seed)
        , seedIndex_(seedIndex)
        , analysisDim_(analysisDim)
        , params_(params)
        , profile_(profile)
        , overrides_(methodOverrides)
        , locked_(locked)
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
        const int64_t start = std::min(std::max<int64_t>(seedIndex_, 0), n - 1);

        // Override por foto: usar SOLO ese método como fuente de candidatos.
        const auto methodFor = [this](int64_t i) {
            const auto it = overrides_.find(i);
            return it != overrides_.end() ? it->second : DiscMethod::Prediction;
        };

        const auto trackOne = [&](int64_t i, DiscTracker& tracker) {
            // Fotos bloqueadas: se siguen procesando (la cadena del tracker
            // avanza para mantener continuidad) pero no se sobrescribe su
            // resultado; la UI conserva el valor bloqueado.
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
                        .arg(QString::fromLatin1(methodName(t.method)))
                        .arg(std::lround(t.confidence * 100.0f)));
                if (t.reacquired) {
                    emit reacquired(i, t.predictedBefore);
                    AppLog::warn(QStringLiteral("re-adquirido en la foto %1 tras %2 supuestas")
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

        int done = 0;
        auto emitProgress = [this, &done, n]() {
            ++done;
            emit progress(done, static_cast<int>(n));
        };

        // Pasada hacia delante desde la semilla.
        DiscTracker forward(params_);
        forward.setProfile(profile_);
        forward.init(seed_.center, seed_.radius);
        for (int64_t i = start; i < n; ++i) {
            if (stop_.load())
                break;
            trackOne(i, forward);
            emitProgress();
        }

        // Pasada hacia atrás desde la semilla: el seguimiento completo recorre
        // toda la secuencia en ambas direcciones.
        {
            DiscTracker backward(params_);
            backward.setProfile(profile_);
            backward.init(seed_.center, seed_.radius);
            for (int64_t i = start - 1; i >= 0 && !stop_.load(); --i) {
                trackOne(i, backward);
                emitProgress();
            }
        }

        emit finished(!stop_.load(), QString(), res);
    }

signals:
    void progress(int done, int total);
    void finished(bool ok, const QString& error, const QVector<double>& results);
    // El disco volvió a confirmarse en la foto `index` tras `predictedBefore`
    // fotos "supuestas" (re-adquisición tras pérdida/salto).
    void reacquired(int64_t index, int predictedBefore);
    // La foto `index` acaba de procesarse (para mostrar el nombre en la UI).
    void photoProcessed(int64_t index);

private:
    QStringList paths_;
    CircleF seed_;
    int64_t seedIndex_ = 0;
    int analysisDim_ = 0;
    DiscTrackerParams params_;
    TrackingProfile profile_;
    std::map<int64_t, DiscMethod> overrides_;
    std::vector<bool> locked_;
    std::atomic<bool> stop_{false};
};
