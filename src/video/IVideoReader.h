#pragma once

#include "common/Frame.h"
#include <string>

// Interfaz de lectura de vídeo. La implementación FFmpeg es sustituible.
class IVideoReader
{
public:
    virtual ~IVideoReader() = default;

    virtual bool open(const std::string& path) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Decodifica el siguiente frame de vídeo. Devuelve false al llegar a EOF.
    virtual bool readNext(Frame& out) = 0;

    // Salta al instante dado (microsegundos) y deja listo el siguiente frame
    // con pts >= us.
    virtual bool seekToUs(int64_t us) = 0;

    virtual int64_t durationUs() const = 0;
    virtual int64_t frameCount() const = 0;
    virtual double fps() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};