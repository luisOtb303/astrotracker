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

int getInt(const Exiv2::ExifData& exif, const char* key)
{
    const auto it = exif.findKey(Exiv2::ExifKey(key));
    if (it != exif.end() && it->count() > 0)
        return it->toInt64();
    return -1;
}

std::string lightSourceLabel(int v)
{
    switch (v) {
    case 1:  return "Luz de día";
    case 2:  return "Fluorescente";
    case 3:  return "Tungsteno";
    case 4:  return "Flash";
    case 9:  return "Buen tiempo";
    case 10: return "Nublado";
    case 11: return "Sombra";
    case 12: return "Fluorescente luz día";
    case 13: return "Fluorescente luz blanca";
    case 20: return "D55";
    case 21: return "D65";
    case 22: return "D75";
    case 23: return "D50";
    case 24: return "Tungsteno estudio";
    case 255: return "Otro";
    default:  return {};
    }
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

        // Balance de blancos
        const int wbVal = getInt(exif, "Exif.Photo.WhiteBalance");
        const int lsVal = getInt(exif, "Exif.Photo.LightSource");
        if (wbVal >= 0 || lsVal >= 0) {
            std::string label;
            if (lsVal >= 0)
                label = lightSourceLabel(lsVal);
            if (label.empty())
                label = (wbVal == 0) ? "Automático" : "Manual";
            info.whiteBalance = label;
        }
        // Kelvin de fabricante (best-effort)
        static const char* kTempKeys[] = {
            "Exif.CanonSi.ColorTemperature",
            "Exif.Nikon2.ColorTemperature",
            "Exif.NikonIi.ColorTemperature",
            nullptr
        };
        for (const char** k = kTempKeys; *k; ++k) {
            const int t = getInt(exif, *k);
            if (t > 0) {
                info.colorTempK = t;
                break;
            }
        }
    } catch (const Exiv2::Error&) {
        // Fichero sin metadatos legibles: se devuelve la info vacía.
    }

    return info;
}

void ExifReader::mergeWhiteBalance(const std::string& path, PhotoExifInfo& info)
{
    try {
        auto image = Exiv2::ImageFactory::open(path);
        if (!image)
            return;
        image->readMetadata();

        const Exiv2::ExifData& exif = image->exifData();
        const int wbVal = getInt(exif, "Exif.Photo.WhiteBalance");
        const int lsVal = getInt(exif, "Exif.Photo.LightSource");
        if (wbVal >= 0 || lsVal >= 0) {
            std::string label;
            if (lsVal >= 0)
                label = lightSourceLabel(lsVal);
            if (label.empty())
                label = (wbVal == 0) ? "Automático" : "Manual";
            info.whiteBalance = label;
        }
        static const char* kTempKeys[] = {
            "Exif.CanonSi.ColorTemperature",
            "Exif.Nikon2.ColorTemperature",
            "Exif.NikonIi.ColorTemperature",
            nullptr
        };
        for (const char** k = kTempKeys; *k; ++k) {
            const int t = getInt(exif, *k);
            if (t > 0) {
                info.colorTempK = t;
                break;
            }
        }
    } catch (const Exiv2::Error&) {
        // Fichero sin metadatos legibles.
    }
}