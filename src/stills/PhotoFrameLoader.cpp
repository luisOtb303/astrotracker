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
    requestSeq_.fetch_add(1);
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

    int64_t lastSeq = 0;
    int64_t cachedIndex = -1;
    cv::Mat cached;
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
        cv_.wait(lock, [&] { return stop_.load() || requestSeq_.load() != lastSeq; });
        if (stop_.load())
            break;
        lastSeq = requestSeq_.load();
        const int64_t idx = pending_.load();
        lock.unlock();
        if (idx != cachedIndex || cached.empty()) {
            cv::Mat frame;
            if (reader.readAt(idx, frame, maxDim_) && !frame.empty()) {
                cached = frame;
                cachedIndex = idx;
            }
        }
        if (idx == cachedIndex && !cached.empty())
            emit frameReady(idx, cached);
        lock.lock();
    }
}