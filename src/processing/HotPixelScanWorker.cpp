#include "processing/HotPixelScanWorker.h"

#include "video/FFmpegVideoReader.h"

#include <QMetaType>

HotPixelScanWorker::HotPixelScanWorker(QObject* parent)
    : QThread(parent)
{
    qRegisterMetaType<HotPixelMap::Result>("HotPixelMap::Result");
}

HotPixelScanWorker::~HotPixelScanWorker()
{
    shutdown();
}

bool HotPixelScanWorker::startScan(const QString& path, const HotPixelMap::Options& opts)
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return false; // ya hay un análisis en curso

    path_ = path;
    opts_ = opts;
    opts_.cancel = &cancel_;
    cancel_.store(false);
    opts_.progress = [this](int sampled, int planned) {
        emit progress(sampled, planned);
    };

    start();
    return true;
}

void HotPixelScanWorker::cancel()
{
    cancel_.store(true);
}

void HotPixelScanWorker::shutdown()
{
    cancel_.store(true);
    if (isRunning())
        wait();
}

void HotPixelScanWorker::run()
{
    HotPixelMap::Result result;

    // Lector propio: el del visor está positioned en el frame que el usuario
    // está viendo y no se debe tocar. Al terminar se cierra y se queda solo.
    FFmpegVideoReader reader;
    if (!reader.open(std::string(path_.toUtf8().constData()))) {
        result.error = QStringLiteral("no se ha podido abrir el video");
    } else {
        result = HotPixelMap::fromVideo(reader, opts_);
    }

    running_.store(false);
    emit finished(result);
}