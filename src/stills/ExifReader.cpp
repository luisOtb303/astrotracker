#include "stills/ExifReader.h"

#include <exiv2/exiv2.hpp>

#include <cstdlib>
#include <string>

namespace
{

std::string getString(const Exiv2::ExifData& exif, const char* key)
{
    const auto it = exif.findKey(Exiv2::ExifKey(key));
    if (it != exif.end() && it->count() > 0)
        return it->toString();
    return {};
}

float getRational(const Exiv2::ExifData& exif, const char* key)
{
    const auto it = exif.findKey(Exiv2::ExifKey(key));
    if (it == exif.end())
        return 0.0f;
    const Exiv2::Rational r = it->toRational();
    if (r.second == 0)
        return 0.0f;
    return static_cast<float>(static_cast<double>(r.first) / r.second);
}

} // namespace

PhotoExifInfo ExifReader::readExif(const std::string& path)
{
    PhotoExifInfo info;

    try {
        auto image = Exiv2::ImageFactory::open(path);
        if (!image)
            return info;
        image->readMetadata();

        const Exiv2::ExifData& exif = image->exifData();
        if (image->pixelWidth() > 0)
            info.width = static_cast<int>(image->pixelWidth());
        if (image->pixelHeight() > 0)
            info.height = static_cast<int>(image->pixelHeight());

        const std::string make = getString(exif, "Exif.Image.Make");
        const std::string model = getString(exif, "Exif.Image.Model");
        info.camera = make;
        if (!info.camera.empty() && !model.empty())
            info.camera += " ";
        info.camera += model;

        std::string lens = getString(exif, "Exif.Photo.LensModel");
        if (lens.empty())
            lens = getString(exif, "Exif.Photo.Lens");
        info.lens = lens;

        info.focalMm = getRational(exif, "Exif.Photo.FocalLength");
        info.aperture = getRational(exif, "Exif.Photo.FNumber");
        info.shutterSec = getRational(exif, "Exif.Photo.ExposureTime");

        const std::string iso = getString(exif, "Exif.Photo.ISOSpeedRatings");
        if (!iso.empty())
            info.iso = std::atoi(iso.c_str());

        std::string date = getString(exif, "Exif.Photo.DateTimeOriginal");
        if (date.empty())
            date = getString(exif, "Exif.Image.DateTime");
        info.date = date;
    } catch (const Exiv2::Error&) {
        // Fichero sin metadatos legibles: se devuelve la info vacía.
    }

    return info;
}