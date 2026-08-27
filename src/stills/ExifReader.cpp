#include "stills/ExifReader.h"

#include <QImage>
#include <QByteArray>
#include <QString>
#include <cmath>
#include <cstdlib>

PhotoExifInfo ExifReader::readExif(const std::string& path)
{
    return readExifQImage(path);
}

PhotoExifInfo ExifReader::readExifQImage(const std::string& path)
{
    PhotoExifInfo info;

    QImage img(QString::fromStdString(path));
    if (img.isNull())
        return info;

    info.width = img.width();
    info.height = img.height();

    // Qt exposes some EXIF as image text keys for JPEG
    const QString make = img.text("Make");
    const QString model = img.text("Model");
    if (!make.isEmpty() || !model.isEmpty()) {
        info.camera = make.toStdString();
        if (!info.camera.empty() && !model.isEmpty())
            info.camera += " ";
        info.camera += model.toStdString();
    }

    const QString lensMake = img.text("LensMake");
    const QString lensModel = img.text("LensModel");
    if (!lensModel.isEmpty()) {
        info.lens = lensModel.toStdString();
    } else if (!lensMake.isEmpty()) {
        info.lens = lensMake.toStdString();
    }

    // Focal length
    const QString focalStr = img.text("FocalLength");
    if (!focalStr.isEmpty()) {
        // Format: "800/1" or "50/1"
        const QStringList parts = focalStr.split('/');
        if (parts.size() == 2) {
            const double num = parts[0].toDouble();
            const double den = parts[1].toDouble();
            if (den > 0)
                info.focalMm = static_cast<float>(num / den);
        }
    }

    // Aperture
    const QString apertureStr = img.text("FNumber");
    if (!apertureStr.isEmpty()) {
        const QStringList parts = apertureStr.split('/');
        if (parts.size() == 2) {
            const double num = parts[0].toDouble();
            const double den = parts[1].toDouble();
            if (den > 0)
                info.aperture = static_cast<float>(num / den);
        }
    }

    // Exposure time
    const QString exposureStr = img.text("ExposureTime");
    if (!exposureStr.isEmpty()) {
        const QStringList parts = exposureStr.split('/');
        if (parts.size() == 2) {
            const double num = parts[0].toDouble();
            const double den = parts[1].toDouble();
            if (den > 0)
                info.shutterSec = static_cast<float>(num / den);
        }
    }

    // ISO
    const QString isoStr = img.text("ISOSpeedRatings");
    if (!isoStr.isEmpty())
        info.iso = isoStr.toInt();

    // Date
    const QString dateStr = img.text("DateTimeOriginal");
    if (!dateStr.isEmpty())
        info.date = dateStr.toStdString();

    return info;
}
