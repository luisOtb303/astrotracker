#pragma once

#include "common/PhotoExifInfo.h"
#include <string>

// Extracts EXIF metadata from JPEG/PNG/TIFF files using the vendored exiv2
// library. RAW files are handled by RawExifReader (LibRaw).
class ExifReader
{
public:
    static PhotoExifInfo readExif(const std::string& path);
};