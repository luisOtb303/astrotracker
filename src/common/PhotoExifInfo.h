#pragma once

#include <cmath>
#include <cstdio>
#include <string>

// Universal EXIF data extracted from any image (RAW, JPEG, PNG, TIFF).
struct PhotoExifInfo
{
    std::string camera;      // "Canon EOS R6"
    std::string lens;        // "RF 800mm f/11"
    float focalMm = 0;      // 800.0
    float aperture = 0;     // 11.0
    float shutterSec = 0;   // 0.002 (1/500)
    int iso = 0;            // 3200
    std::string date;        // "2026-03-15T22:47:00"
    int width = 0;          // 5472
    int height = 0;         // 3648

    bool hasFocal() const { return focalMm > 0.0f; }
    bool hasAperture() const { return aperture > 0.0f; }
    bool hasShutter() const { return shutterSec > 0.0f; }
    bool hasIso() const { return iso > 0; }
    bool hasCamera() const { return !camera.empty(); }
    bool hasLens() const { return !lens.empty(); }
    bool hasDate() const { return !date.empty(); }

    bool isEmpty() const
    {
        return !hasCamera() && !hasLens() && !hasFocal() &&
               !hasAperture() && !hasShutter() && !hasIso();
    }

    // Shutter as human-readable fraction: "1/500s" or "0.5s"
    std::string shutterString() const
    {
        if (shutterSec <= 0.0f) return {};
        if (shutterSec < 1.0f) {
            const int denom = static_cast<int>(std::round(1.0 / shutterSec));
            return "1/" + std::to_string(denom) + "s";
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1fs", shutterSec);
        return buf;
    }

    // Aperture as "f/2.8"
    std::string apertureString() const
    {
        if (aperture <= 0.0f) return {};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "f/%.1f", aperture);
        return buf;
    }

    // Focal as "800mm"
    std::string focalString() const
    {
        if (focalMm <= 0.0f) return {};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0fmm", focalMm);
        return buf;
    }

    // Combined: "800mm · f/11 · 1/500s"
    std::string combinedString() const
    {
        std::string s;
        if (hasFocal()) s += focalString();
        if (hasAperture()) {
            if (!s.empty()) s += " · ";
            s += apertureString();
        }
        if (hasShutter()) {
            if (!s.empty()) s += " · ";
            s += shutterString();
        }
        return s;
    }
};
