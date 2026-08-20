#include "stills/PhotoFrameLoader.h"

#include "stills/PhotoSequenceReader.h"

#include <QMetaType>

#include <vector>

PhotoFrameLoader::PhotoFrameLoader(const QStringList& paths, int maxDim, QObject* parent)
    : QThread(parent)
    , paths_(paths)
    , maxDim_(maxDim)
{
    qRegisterMetaType<cv::Mat>("cv::Mat");
}

void PhotoFrameLoader::requestLoad(int64_t index)
{
    pending_.store(index);
    cv_.notify_all();
}

void PhotoFrameLoader::shutdown()
{
    stop_.store(true);
    cv_.notify_all();
    wait();
}

void PhotoFrameLoader::run()
{
    std::vector<std::string> paths;
    paths.reserve(static_cast<size_t>(paths_.size()));
    for (const QString& p : paths_)
        paths.push_back(p.toStdString());

    PhotoSequenceReader reader;
    if (!reader.open(paths))
        return;

    int64_t last = -1;
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
        cv_.wait(lock, [&] { return stop_.load() || pending_.load() != last; });
        if (stop_.load())
            break;
        last = pending_.load();
        lock.unlock();
        cv::Mat frame;
        if (reader.readAt(last, frame, maxDim_) && !frame.empty())
            emit frameReady(last, frame);
        lock.lock();
    }
}