#include "common/ImageAdjust.h"

#include <algorithm>
#include <cmath>
#include <opencv2/photo.hpp>

namespace {

// Gains de WB basados en temperatura Kelvin relativos a una referencia.
// t = targetKelvin, d = detectedKelvin (referencia).
// Más Kelvin = más cálido (R↑, B↓); identidad en t == d.
void wbGains(float t, float d, float& rGain, float& bGain)
{
    if (d <= 0.f)
        d = static_cast<float>(ImageAdjustLimits::kDefaultKelvin);
    if (std::abs(t - d) < 0.5f) {
        rGain = 1.f;
        bGain = 1.f;
        return;
    }
    constexpr float kPower = 0.65f;
    rGain = std::pow(t / d, kPower);
    bGain = std::pow(d / t, kPower);
}

// Ganancias LP: reduce canales donde inciden sodio/mercurio.
void lpGains(float sodium, float mercury, float& rG, float& gG, float& bG)
{
    rG = 1.f;
    gG = 1.f;
    bG = 1.f;

    if (sodium > 0.f) {
        // Sodio 589nm (naranja): atenúa R y G, compensa ligeramente B
        rG *= (1.f - 0.50f * sodium);
        gG *= (1.f - 0.25f * sodium);
        bG *= (1.f + 0.10f * sodium);
    }
    if (mercury > 0.f) {
        // Mercurio 436/546/578nm (violeta-verde): atenúa G y B
        gG *= (1.f - 0.40f * mercury);
        bG *= (1.f - 0.15f * mercury);
    }
}

} // namespace

namespace img {

cv::Mat apply(const cv::Mat& bgr, const ImageAdjust& adj, int detectedKelvin)
{
    if (bgr.empty() || adj.isDefault())
        return bgr.clone();

    // --- 1. Denoise (solo CV_8UC3, antes de float) ---
    cv::Mat work = bgr;
    if (adj.denoise > 0 && bgr.depth() == CV_8U && bgr.type() == CV_8UC3) {
        const int h = static_cast<int>(adj.denoise * 0.4f); // 0..40
        if (h > 0) {
            cv::fastNlMeansDenoisingColored(bgr, work, static_cast<float>(h),
                                             static_cast<float>(h * 0.75f),
                                             7, 21);
        }
    }

    // --- 2. Convertir a float [0, 1] ---
    cv::Mat f;
    work.convertTo(f, CV_32F, 1.0 / 255.0);

    std::vector<cv::Mat> ch;
    cv::split(f, ch);

    // --- 3. LP removal (sodio + mercurio) ---
    const float s = std::clamp(static_cast<float>(adj.lpSodium) / 100.f, 0.f, 1.f);
    const float m = std::clamp(static_cast<float>(adj.lpMercury) / 100.f, 0.f, 1.f);
    {
        float lrG, lgG, lbG;
        lpGains(s, m, lrG, lgG, lbG);
        ch[2] *= lrG; // R
        ch[1] *= lgG; // G
        ch[0] *= lbG; // B
    }

    // --- 4. WB Kelvin ---
    if (adj.wbKelvin != 0) {
        const float t = static_cast<float>(adj.wbKelvin);
        const float d = detectedKelvin > 0
                            ? static_cast<float>(detectedKelvin)
                            : static_cast<float>(ImageAdjustLimits::kDefaultKelvin);
        float rWb, bWb;
        wbGains(t, d, rWb, bWb);
        ch[2] *= rWb; // R
        ch[0] *= bWb; // B
        // G se mantiene (ganancia 1)
    }

    // --- 5. Exposición (EV) ---
    if (adj.exposureEv != 0) {
        const float ev = static_cast<float>(adj.exposureEv) / 10.f;
        const float gain = std::pow(2.f, ev);
        ch[2] *= gain;
        ch[1] *= gain;
        ch[0] *= gain;
    }

    // --- 6. Contraste ---
    if (adj.contrast != 0) {
        const float c = static_cast<float>(adj.contrast) / 100.f;
        const float factor = (1.f + c) / (1.f - c + 0.001f);
        ch[2] = (ch[2] - 0.5f) * factor + 0.5f;
        ch[1] = (ch[1] - 0.5f) * factor + 0.5f;
        ch[0] = (ch[0] - 0.5f) * factor + 0.5f;
    }

    // --- 7. Brillo ---
    if (adj.brightness != 0) {
        const float b = static_cast<float>(adj.brightness) / 200.f;
        ch[2] += b;
        ch[1] += b;
        ch[0] += b;
    }

    // --- 8. Clamp + convertir ---
    cv::merge(ch, f);
    cv::Mat out;
    f.convertTo(out, bgr.depth(), 255.0);
    return out;
}

} // namespace img
