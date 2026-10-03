#include "processing/AdjustWorker.h"

#include <QDeadlineTimer>
#include <QMetaType>

AdjustWorker::AdjustWorker(QObject* parent)
    : QThread(parent)
{
    qRegisterMetaType<cv::Mat>("cv::Mat");
}

void AdjustWorker::setSource(const cv::Mat& src)
{
    QMutexLocker lock(&mutex_);
    source_ = src.empty() ? cv::Mat() : src.clone();
}

bool AdjustWorker::request(const ImageAdjust& adj)
{
    if (adj.isDefault()) {
        // Identidad no necesita pasar por el hilo: se entrega el crudo tal cual.
        reset();
        return false;
    }
    QMutexLocker lock(&mutex_);
    requested_ = adj;
    hasRequested_ = true;
    ++requestSeq_;
    cond_.wakeAll();
    return true;
}

void AdjustWorker::reset()
{
    QMutexLocker lock(&mutex_);
    hasRequested_ = false;
    requested_ = ImageAdjust{};
    cond_.wakeAll();
}

void AdjustWorker::shutdown()
{
    {
        QMutexLocker lock(&mutex_);
        stop_.store(true);
        cond_.wakeAll();
    }
    wait();
}

bool AdjustWorker::waitIdle(int msecs)
{
    QDeadlineTimer deadline(msecs);
    QMutexLocker lock(&mutex_);
    // QWaitCondition no admite predicado: sondea con reevaluacion del mutex.
    while (hasRequested_ || requestSeq_ != appliedSeq_ || busy_) {
        if (deadline.hasExpired())
            return false;
        cond_.wait(&mutex_, 10);
    }
    return true;
}

void AdjustWorker::run()
{
    QMutexLocker lock(&mutex_);
    while (true) {
        while (!stop_.load() && requestSeq_ == appliedSeq_)
            cond_.wait(&mutex_, 20);
        if (stop_.load())
            break;

        // Coalescing: si se pedimos algo mas mientras trabajabamos, hasRequested_
        // lo recoge el siguiente ciclo y solo se procesa la ultima peticion.
        appliedSeq_ = requestSeq_;
        const ImageAdjust adj = requested_;
        const quint64 seq = appliedSeq_;
        const cv::Mat src = source_;
        hasRequested_ = false;
        busy_ = true;
        lock.unlock();

        cv::Mat out;
        if (!src.empty())
            out = img::apply(src, adj);

        lock.relock();
        busy_ = false;
        cond_.wakeAll();

        if (!out.empty())
            emit adjusted(out, seq);
    }
}