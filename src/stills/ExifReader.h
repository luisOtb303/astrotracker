#pragma once

#include "common/PhotoExifInfo.h"
#include <string>

// Extracts EXIF metadata from JPEG/PNG/TIFF files.
// Uses Qt's QImage for basic EXIF. When exiv2 is available, uses it for
// comprehensive EXIF extraction.
class ExifReader
{
public:
    static PhotoExifInfo readExif(const std::string& path);

private:
    static PhotoExifInfo readExifQImage(const std::string& path);
};
