#pragma once

#include <QStringList>
#include <QThread>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <opencv2/core.hpp>

// Carga la foto pedida en un hilo separado para que la UI no se bloquee
// mientras se decodifica (los RAW tardan). Varias peticiones seguidas se
// coalescen: solo se entrega la última (el frameReady se emite para la foto
// más recientemente solicitada).
class PhotoFrameLoader : public QThread
{
    Q_OBJECT

public:
    PhotoFrameLoader(const QStringList& paths, int maxDim, QObject* parent = nullptr);

    void requestLoad(int64_t index);
    void shutdown();

protected:
    void run() override;

signals:
    void frameReady(int64_t index, const cv::Mat& frame);

private:
    QStringList paths_;
    int maxDim_ = 0;
    std::atomic<bool> stop_{false};
    std::atomic<int64_t> pending_{-1};
    std::mutex mutex_;
    std::condition_variable cv_;
};