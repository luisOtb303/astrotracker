#pragma once

#include "video/IVideoReader.h"
#include <memory>

// Lector de vídeo basado en libavformat/libavcodec/libswscale (FFmpeg).
// PIMPL para no exponer headers de FFmpeg a la UI.
class FFmpegVideoReader : public IVideoReader
{
public:
    FFmpegVideoReader();
    ~FFmpegVideoReader() override;

    bool open(const std::string& path) override;
    void close() override;
    bool isOpen() const override;

    bool readNext(Frame& out) override;
    bool seekToUs(int64_t us) override;

    int64_t durationUs() const override;
    int64_t frameCount() const override;
    double fps() const override;
    int width() const override;
    int height() const override;

private:
    bool decodeLoop(Frame& out);
    void convertFrame(void* srcFrame, cv::Mat& dst);

    struct Impl;
    std::unique_ptr<Impl> d;
};