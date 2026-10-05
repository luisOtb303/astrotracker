#include "common/ImageAdjust.h"

#include <algorithm>
#include <cmath>
#include <opencv2/photo.hpp>

namespace {

// Gains de WB relativos al punto neutro (la imagen tal como sale del
// decodificador). No hay Kelvin de por medio: warmth > 0 = más cálido (R↑, B↓).
// En 0 las ganancias son exactamente 1 (imagen intacta).
void wbGains(int warmth, float& rGain, float& bGain)
{
    if (warmth == 0) {
        rGain = 1.f;
        bGain = 1.f;
        return;
    }
    constexpr float kMaxShift = 0.45f; // tope de corrección al extremo del slider
    const float k = static_cast<float>(warmth) / 100.f * kMaxShift;
    rGain = 1.f + k;
    bGain = 1.f - k * 0.85f; // B baja algo menos de lo que sube R
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

// --- Píxeles calientes -----------------------------------------------------
//
// Un fotosito quemado del sensor lee siempre por encima de sus vecinos, en la
// misma posición, con el mismo valor alto. Eso lo distingue de una estrella,
// que se mueve. Aquí solo se hace la parte espacial (la temporal, que sí
// necesita varios fotogramas, vive en HotPixelMap).

// Mediana 3x3. Para un píxel defectuoso la mediana de los 9 valores es la de sus
// 8 vecinos: él es un extremo y no desplaza el centro de la distribución. Sirve
// tanto de referencia para comparar como de valor de sustitución.
cv::Mat localMedian(const cv::Mat& src)
{
    cv::Mat med;
    cv::medianBlur(src, med, 3);
    return med;
}

// Ruido de la toma, estimado del residuo de alta frecuencia |frame - mediana|.
//
// La desviación de esa diferencia es √2 veces la del ruido, de ahí la división.
// Es robusto frente a los propios píxeles calientes (son unos cuantos de entre
// millones) y evita tener que conocer el modelo de cámara o el ISO.
double noiseSigma(const cv::Mat& frame, const cv::Mat& med)
{
    cv::Mat residual;
    cv::absdiff(frame, med, residual);
    cv::Scalar mean, stddev;
    cv::meanStdDev(residual, mean, stddev);

    double s = 0.0;
    for (int c = 0; c < residual.channels(); ++c)
        s += stddev[c];
    return (s / residual.channels()) / 1.41421356;
}

// Píxeles que superan su entorno local en más de k·sigma.
//
// El umbral se evalúa por canal y se unse con OR: un fotosito quemado satura
// en el canal del Bayer que le toca, no necesariamente en los tres.
cv::Mat candidateMask(const cv::Mat& frame, const cv::Mat& med, double thr)
{
    if (thr < 0.2)
        thr = 0.2; // imagen plana: evita un umbral degenerado

    cv::Mat residual;
    cv::absdiff(frame, med, residual);
    residual.convertTo(residual, CV_32F);

    std::vector<cv::Mat> ch;
    cv::split(residual, ch);

    cv::Mat any;
    for (const auto& c : ch) {
        cv::Mat m;
        cv::compare(c, thr, m, cv::CMP_GT);
        any = any.empty() ? m : (any | m);
    }
    return any;
}

// Deja solo los candidatos que están realmente solos. Si alguno de los 8
// vecinos también es candidato, lo que hay ahí es una estrella con estructura o
// un grupo de ruido, no un fotosito individual.
//
// El vecindario excluye el centro a propósito: si no, cada candidato se
// anularía a sí mismo por ser su propio vecino.
cv::Mat isolate(const cv::Mat& mask)
{
    cv::Mat kernel = cv::Mat::ones(3, 3, CV_8U);
    kernel.at<uchar>(1, 1) = 0;

    cv::Mat neighbours;
    cv::dilate(mask, neighbours, kernel);
    cv::bitwise_not(neighbours, neighbours);

    cv::Mat isolated;
    cv::bitwise_and(mask, neighbours, isolated);
    return isolated;
}

} // namespace

namespace img {

cv::Mat detectHotPixels(const cv::Mat& frame, int sensitivity)
{
    if (frame.empty())
        return {};

    const cv::Mat med = localMedian(frame);
    const double k = ImageAdjustLimits::hotSensitivityToSigma(sensitivity);
    const double thr = k * noiseSigma(frame, med);

    return isolate(candidateMask(frame, med, thr));
}

cv::Mat correctHotPixels(const cv::Mat& frame, const cv::Mat& mask)
{
    if (frame.empty() || mask.empty() || mask.size() != frame.size())
        return frame;
    if (cv::countNonZero(mask) == 0)
        return frame;

    // La mediana local de un píxel defectuoso es la mediana de sus vecinos, así
    // que sustituye sin introducir gradientes nuevos.
    const cv::Mat med = localMedian(frame);

    cv::Mat out;
    frame.copyTo(out);
    med.copyTo(out, mask);
    return out;
}

} // namespace img

namespace img {

cv::Mat apply(const cv::Mat& bgr, const ImageAdjust& adj, const cv::Mat& hotMask)
{
    if (bgr.empty() || (adj.isDefault() && hotMask.empty()))
        return bgr.clone();

    // --- 1. Píxeles calientes (antes que nada: el denoise los extendería) ---
    //
    // Aquí SOLO se aplica una máscara explícita, que es el mapa temporal que
    // confirma las mismas posiciones a lo largo de varios fotogramas de vídeo.
    //
    // La detección espacial automática sobre la imagen ya demosaicada se ha
    // quitado a propósito: al debayer, el fotosito quemado reparte su carga
    // entre los píxeles vecinos y la mediana local sube con él, así que el
    // criterio "soy mucho más brillante que mis vecinos" deja de funcionar
    // (medido: de +6716 DN de exceso en el mosaico a +16 DN después de
    // demosaicar, con cientos de miles de falsos positivos por imagen). Para
    // fotos RAW el filtro se aplica antes del debayer, dentro de
    // RawDecoder::decode(). Ver docs/hotpixel-filter.md.
    cv::Mat work = bgr;
    if (!hotMask.empty())
        work = correctHotPixels(bgr, hotMask);

    // --- 2. Denoise (solo CV_8UC3, antes de float) ---
    if (adj.denoise > 0 && work.depth() == CV_8U && work.type() == CV_8UC3) {
        const int h = static_cast<int>(adj.denoise * 0.4f); // 0..40
        if (h > 0) {
            cv::fastNlMeansDenoisingColored(work, work, static_cast<float>(h),
                                             static_cast<float>(h * 0.75f),
                                             7, 21);
        }
    }

    // --- 3. Convertir a float [0, 1] ---
    cv::Mat f;
    work.convertTo(f, CV_32F, 1.0 / 255.0);

    std::vector<cv::Mat> ch;
    cv::split(f, ch);

    // --- 4. LP removal (sodio + mercurio) ---
    const float s = std::clamp(static_cast<float>(adj.lpSodium) / 100.f, 0.f, 1.f);
    const float m = std::clamp(static_cast<float>(adj.lpMercury) / 100.f, 0.f, 1.f);
    {
        float lrG, lgG, lbG;
        lpGains(s, m, lrG, lgG, lbG);
        ch[2] *= lrG; // R
        ch[1] *= lgG; // G
        ch[0] *= lbG; // B
    }

    // --- 5. WB (calidez relativa al neutro) ---
    if (adj.wbWarmth != 0) {
        float rWb, bWb;
        wbGains(adj.wbWarmth, rWb, bWb);
        ch[2] *= rWb; // R
        ch[0] *= bWb; // B
        // G se mantiene (ganancia 1)
    }

    // --- 6. Exposición (EV) ---
    if (adj.exposureEv != 0) {
        const float ev = static_cast<float>(adj.exposureEv) / 10.f;
        const float gain = std::pow(2.f, ev);
        ch[2] *= gain;
        ch[1] *= gain;
        ch[0] *= gain;
    }

    // --- 7. Contraste ---
    if (adj.contrast != 0) {
        const float c = static_cast<float>(adj.contrast) / 100.f;
        const float factor = (1.f + c) / (1.f - c + 0.001f);
        ch[2] = (ch[2] - 0.5f) * factor + 0.5f;
        ch[1] = (ch[1] - 0.5f) * factor + 0.5f;
        ch[0] = (ch[0] - 0.5f) * factor + 0.5f;
    }

    // --- 8. Brillo ---
    if (adj.brightness != 0) {
        const float b = static_cast<float>(adj.brightness) / 200.f;
        ch[2] += b;
        ch[1] += b;
        ch[0] += b;
    }

    // --- 9. Clamp + convertir ---
    cv::merge(ch, f);
    cv::Mat out;
    f.convertTo(out, bgr.depth(), 255.0);
    return out;
}

} // namespace img
