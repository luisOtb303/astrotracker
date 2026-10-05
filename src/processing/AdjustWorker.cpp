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
    // El mapa de píxeles calientes pertenece al frame de referencia: al
    // cambiar de fotograma ya no encaja en tamaño y no debe aplicarse.
    hotMask_ = cv::Mat();
}

void AdjustWorker::setHotMask(const cv::Mat& mask)
{
    QMutexLocker lock(&mutex_);
    hotMask_ = mask.empty() ? cv::Mat() : mask.clone();
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
    // También avanza la secuencia: si había un resultado en vuelo, hay que
    // invalidarlo o se pintaría un ajuste que ya no está vigente.
    ++requestSeq_;
    cond_.wakeAll();
}

quint64 AdjustWorker::currentSeq()
{
    QMutexLocker lock(&mutex_);
    return requestSeq_;
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

        // reset() (petición identidad incluida) también avanza requestSeq_, así
        // que puede haber trabajo pendiente sin nada que aplicar: en ese caso
        // solo hay que sincronizar appliedSeq_ para no reprocesar.
        if (!hasRequested_) {
            appliedSeq_ = requestSeq_;
            continue;
        }

        // Coalescing: si se pedimos algo mas mientras trabajabamos, hasRequested_
        // lo recoge el siguiente ciclo y solo se procesa la ultima peticion.
        appliedSeq_ = requestSeq_;
        const ImageAdjust adj = requested_;
        const quint64 seq = appliedSeq_;
        const cv::Mat src = source_;
        const cv::Mat mask = hotMask_;
        hasRequested_ = false;
        busy_ = true;
        lock.unlock();

        cv::Mat out;
        if (!src.empty())
            out = img::apply(src, adj, mask);

        lock.relock();
        busy_ = false;
        cond_.wakeAll();

        if (!out.empty())
            emit adjusted(out, seq);
    }
}