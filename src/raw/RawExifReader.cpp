#include "raw/RawExifReader.h"

#include "libraw/libraw.h"

#include <cstdio>

PhotoExifInfo RawExifReader::readExif(const std::string& path)
{
    PhotoExifInfo info;

    LibRaw raw;
    if (raw.open_file(path.c_str()) != LIBRAW_SUCCESS)
        return info;

    // Extract from imgdata.other (available after open_file)
    const auto& other = raw.imgdata.other;
    info.focalMm = other.focal_len;
    info.aperture = other.aperture;
    info.shutterSec = other.shutter;
    info.iso = static_cast<int>(other.iso_speed);

    // Camera make/model from imgdata.idata
    const auto& idata = raw.imgdata.idata;
    if (idata.make[0] != '\0')
        info.camera = idata.make;
    if (idata.model[0] != '\0') {
        if (!info.camera.empty())
            info.camera += " ";
        info.camera += idata.model;
    }

    // Image dimensions
    info.width = raw.imgdata.sizes.width;
    info.height = raw.imgdata.sizes.height;

    // Timestamp
    if (other.timestamp > 0) {
        char buf[64];
        std::time_t t = static_cast<std::time_t>(other.timestamp);
        struct tm tmBuf;
#ifdef _WIN32
        localtime_s(&tmBuf, &t);
#else
        localtime_r(&t, &tmBuf);
#endif
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmBuf);
        info.date = buf;
    }

    raw.recycle();
    return info;
}
