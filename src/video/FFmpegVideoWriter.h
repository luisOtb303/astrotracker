#pragma once

#include "video/IVideoWriter.h"

#include <memory>

// FFmpegVideoWriter: codifica frames BGR8 a H.264 (libx264) en un contenedor
// MP4/MOV usando libavformat/libavcodec/libswscale.
class FFmpegVideoWriter final : public IVideoWriter
{
public:
    FFmpegVideoWriter();
    ~FFmpegVideoWriter() override;

    bool open(const std::string& path, int width, int height, double fps) override;
    bool write(const cv::Mat& frameBgr) override;
    void close() override;

    bool isOpen() const;
    int frameCount() const { return framesWritten_; }

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    int64_t framesWritten_ = 0;
};