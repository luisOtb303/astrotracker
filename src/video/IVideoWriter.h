#pragma once

#include <opencv2/core.hpp>
#include <string>

// Interfaz de escritores de vídeo: reciben frames BGR8 y los codifican.
class IVideoWriter
{
public:
    virtual ~IVideoWriter() = default;

    virtual bool open(const std::string& path, int width, int height, double fps) = 0;
    virtual bool write(const cv::Mat& frameBgr) = 0;
    virtual void close() = 0;
};