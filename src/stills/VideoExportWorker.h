#pragma once

#include "stills/PhotoExportWorker.h"

#include <QString>
#include <QThread>
#include <atomic>
#include <vector>

// Worker en hilo separado que exporta un vídeo como MP4 (H.264) o como
// secuencia de imágenes PNG/JPG (una imagen por fotograma del vídeo). Si se
// pasan offsets del análisis, cada frame se desplaza para dejar el objeto
// centrado; si el vector está vacío se exporta el vídeo tal cual (TL directo).
class VideoExportWorker : public QThread
{
    Q_OBJECT

public:
    using Settings = PhotoExportWorker::Settings;
    using Resolution = PhotoExportWorker::Resolution;
    using Format = PhotoExportWorker::Format;

    VideoExportWorker(const QString& inPath, const std::vector<cv::Point2f>& offsets,
                      const Settings& settings, QObject* parent = nullptr)
        : QThread(parent)
        , inPath_(inPath)
        , offsets_(offsets)
        , settings_(settings)
    {
    }

    void requestStop() { stop_.store(true); }

    void run() override;

signals:
    void progress(int done, int total);
    void finished(bool ok, const QString& error, int frames);
    void frameProcessed(int64_t index);

private:
    QString inPath_;
    std::vector<cv::Point2f> offsets_;
    Settings settings_;
    std::atomic<bool> stop_{false};
};