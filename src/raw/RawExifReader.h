#pragma once

#include "common/PhotoExifInfo.h"
#include <string>

// Extracts EXIF metadata from RAW files (CR2, CR3, NEF, ARW, etc.) via LibRaw.
class RawExifReader
{
public:
    // Reads EXIF from a RAW file. Returns empty info on failure.
    static PhotoExifInfo readExif(const std::string& path);
};
