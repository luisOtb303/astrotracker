#include "common/ImageAdjust.h"

#include <opencv2/core.hpp>
#include <cmath>
#include <cstdio>

namespace {
int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

cv::Mat makeTestImage()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    return img;
}

void testDefaultIdentity()
{
    const cv::Mat img = makeTestImage();
    const cv::Mat out = img::apply(img, ImageAdjust{});
    const cv::Scalar mean = cv::mean(out);
    expect(std::abs(mean[0] - 128.0) < 2.0, "default: B ~128");
    expect(std::abs(mean[1] - 128.0) < 2.0, "default: G ~128");
    expect(std::abs(mean[2] - 128.0) < 2.0, "default: R ~128");
}

void testSodiumLp()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    ImageAdjust adj;
    adj.lpSodium = 100;
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[2] < 100.0, "sodium LP: R reduced");
    expect(mean[0] > 128.0, "sodium LP: B boosted");
}

void testMercuryLp()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    ImageAdjust adj;
    adj.lpMercury = 100;
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[1] < 128.0, "mercury LP: G reduced");
    expect(mean[0] < 128.0, "mercury LP: B reduced");
}

void testExposureIncrease()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(50, 50, 50));
    ImageAdjust adj;
    adj.exposureEv = 20; // +2 EV
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[0] > 150.0, "exposure +2EV: channels brightened");
}

void testExposureDecrease()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(200, 200, 200));
    ImageAdjust adj;
    adj.exposureEv = -20; // -2 EV
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[0] < 100.0, "exposure -2EV: channels darkened");
}

void testBrightnessIncrease()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(100, 100, 100));
    ImageAdjust adj;
    adj.brightness = 50;
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[0] > 140.0, "brightness +50: channels brighter");
}

void testContrastIncrease()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    // High contrast around midpoint: no change at 128, but helps test pipeline
    ImageAdjust adj;
    adj.contrast = 80;
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar mean = cv::mean(out);
    expect(std::abs(mean[0] - 128.0) < 10.0, "contrast: mid-gray ~128");
}

void testStackingLpWbEv()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    ImageAdjust adj;
    adj.lpSodium = 50;
    adj.wbWarmth = 50;
    adj.exposureEv = 10;
    const cv::Mat out = img::apply(img, adj);
    expect(!out.empty(), "stacking: output not empty");
    expect(out.rows == 100 && out.cols == 100, "stacking: dimensions preserved");
}

void testMonotonicityExposure()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    ImageAdjust a1, a2;
    a1.exposureEv = 0;
    a2.exposureEv = 20;
    const cv::Scalar m1 = cv::mean(img::apply(img, a1));
    const cv::Scalar m2 = cv::mean(img::apply(img, a2));
    expect(m2[0] > m1[0], "monotonicity: higher EV = brighter B");
}

void testEmptyInput()
{
    const cv::Mat empty;
    const cv::Mat out = img::apply(empty, ImageAdjust{});
    expect(out.empty(), "empty input returns empty");
}

// --- Píxeles calientes -----------------------------------------------------

// Fondo plano con ruido gaussiano suave: es lo que hace que el umbral se
// base en sigma y no en un valor absoluto.
cv::Mat makeNoisyBackground(int w = 200, int h = 200, unsigned seed = 7)
{
    cv::RNG rng(seed);
    cv::Mat img(h, w, CV_8UC3);
    rng.fill(img, cv::RNG::NORMAL, 60.0, 2.0);
    return img;
}

void testSpatialDetectsIsolatedBrightPixel()
{
    cv::Mat img = makeNoisyBackground();
    img.at<cv::Vec3b>(100, 100) = cv::Vec3b(250, 250, 250);

    const cv::Mat mask = img::detectHotPixels(img, 50);
    expect(!mask.empty(), "spatial: mask not empty");
    expect(mask.at<uchar>(100, 100) == 255, "spatial: hot pixel is flagged");
    // Los vecinos no deben marcarse: un defecto es un punto, no una mancha.
    expect(mask.at<uchar>(100, 99) == 0, "spatial: left neighbour clean");
    expect(mask.at<uchar>(99, 100) == 0, "spatial: top neighbour clean");
    expect(mask.at<uchar>(101, 101) == 0, "spatial: diagonal neighbour clean");
}

void testSpatialIgnoresCleanBackground()
{
    const cv::Mat img = makeNoisyBackground();
    const cv::Mat mask = img::detectHotPixels(img, 50);
    // El ruido de fondo no debe producir detecciones masivas.
    expect(cv::countNonZero(mask) < 20, "spatial: clean background stays clean");
}

void testSpatialIgnoresCluster()
{
    // Una "estrella" de 3x3 saturada: tiene estructura alrededor, así que el
    // aislamiento por vecindario la descarta. Un fotosito quemado es de 1 píxel.
    cv::Mat img = makeNoisyBackground();
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            img.at<cv::Vec3b>(100 + dy, 100 + dx) = cv::Vec3b(252, 252, 252);

    const cv::Mat mask = img::detectHotPixels(img, 50);
    expect(mask.at<uchar>(100, 100) == 0, "spatial: 3x3 cluster is not a hot pixel");
}

// El deslizador tiene que reducir monótonamente lo detectado. Lo que NO se
// afirma aquí es que una amplitud concreta quede o no filtrada: noiseSigma()
// estima el ruido a partir del residuo |frame - mediana|, que ya viene
// suavizado por la mediana y por tanto sale sesgado a la baja, de modo que k=30
// no es "30 sigmas del ruido real" sino bastante menos. Como esta función ya no
// está en el camino de corrección (el filtro va antes del debayer, sobre el
// mosaico), solo se usa como descriptor por fotograma para acumular el mapa
// temporal, donde importa el orden y no la escala.
void testSpatialSensitivityGatesDetection()
{
    cv::Mat img = makeNoisyBackground();
    img.at<cv::Vec3b>(100, 100) = cv::Vec3b(90, 90, 90);

    const int loose = cv::countNonZero(img::detectHotPixels(img, 0));
    const int tight = cv::countNonZero(img::detectHotPixels(img, 100));

    expect(loose > 0, "spatial: sensibilidad 0 detecta al menos el outlier");
    expect(tight <= loose, "spatial: mas sensibilidad no detecta mas que menos");
}

// apply() NO hace detección espacial sobre la imagen demosaicada: al debayer el
// fotosito quemado reparte su carga entre los píxeles vecinos y la mediana local
// sube con él, así que el criterio local deja de poder separarlo de una estrella
// (de +6716 DN de exceso en el mosaico a +16 DN después de demosaicar). Para
// fotos RAW el filtro va antes del debayer, en RawDecoder::decode(); por eso
// activar el check aquí NO debe tocar la imagen.
void testNoSpatialFallbackOnDemosaicedImage()
{
    cv::Mat img = makeNoisyBackground();
    img.at<cv::Vec3b>(100, 100) = cv::Vec3b(255, 255, 255);

    ImageAdjust adj;
    adj.hotPixels = true;
    const cv::Mat out = img::apply(img, adj);

    expect(out.at<cv::Vec3b>(100, 100) == cv::Vec3b(255, 255, 255),
           "no spatial: apply() no toca la imagen demosaicada por su cuenta");
    expect(cv::norm(img, out, cv::NORM_INF) == 0.0,
           "no spatial: la imagen queda intacta byte a byte");
}

void testOffByDefault()
{
    cv::Mat img = makeNoisyBackground();
    img.at<cv::Vec3b>(100, 100) = cv::Vec3b(255, 255, 255);

    ImageAdjust adj;
    adj.hotSensitivity = 0; // el valor más agresivo, pero con el filtro apagado
    expect(adj.isDefault(), "off: identity even with extreme sensitivity");
    const cv::Mat out = img::apply(img, adj);
    expect(out.at<cv::Vec3b>(100, 100) == img.at<cv::Vec3b>(100, 100),
           "off: hot pixel survives when filter is disabled");
}

// El mapa explícito es el único camino de píxeles calientes por aquí: son
// posiciones ya confirmadas fotograma a fotograma (vídeo), así que no hay
// umbral que ajustar ni estrella que tocar.
void testExplicitMaskAppliesWithFilterOff()
{
    cv::Mat img = makeNoisyBackground();
    img.at<cv::Vec3b>(100, 100) = cv::Vec3b(255, 255, 255);

    cv::Mat mask = cv::Mat::zeros(img.size(), CV_8U);
    mask.at<uchar>(100, 100) = 255;

    const cv::Mat out = img::apply(img, ImageAdjust{}, mask);
    expect(out.at<cv::Vec3b>(100, 100) != cv::Vec3b(255, 255, 255),
           "explicit mask: hot pixel replaced with filter off");
}

void testMismatchMaskIsIgnored()
{
    cv::Mat img = makeNoisyBackground(64, 64);
    img.at<cv::Vec3b>(10, 10) = cv::Vec3b(255, 255, 255);

    // Máscara de otro tamaño: se ignora entera. Antes caía al filtro espacial;
    // ahora no hay nada a lo que caerse, y es mejor no tocar nada.
    cv::Mat mask = cv::Mat::zeros(32, 32, CV_8U);
    mask.at<uchar>(10, 10) = 255;

    ImageAdjust adj;
    adj.hotPixels = true;
    const cv::Mat out = img::apply(img, adj, mask);
    expect(out.size() == img.size(), "mismatched mask: size preserved");
    expect(out.at<cv::Vec3b>(10, 10) == img.at<cv::Vec3b>(10, 10),
           "mismatched mask: se ignora y no modifica nada");
}

void test16bitSupport()
{
    cv::Mat img(64, 64, CV_16UC3, cv::Scalar(2000, 2000, 2000));
    cv::RNG rng(3);
    rng.fill(img, cv::RNG::NORMAL, 2000.0, 40.0);
    img.at<cv::Vec3w>(32, 32) = cv::Vec3w(60000, 60000, 60000);

    cv::Mat mask = cv::Mat::zeros(img.size(), CV_8U);
    mask.at<uchar>(32, 32) = 255;

    const cv::Mat out = img::apply(img, ImageAdjust{}, mask);
    expect(out.depth() == CV_16U, "16bit: depth preserved");
    expect(out.at<cv::Vec3w>(32, 32)[0] < 60000, "16bit: hot pixel replaced");
}

void testSensitivityMapping()
{
    expect(ImageAdjustLimits::hotSensitivityToSigma(0) == ImageAdjustLimits::kHotSigmaMin,
           "mapping: 0 -> sigma min");
    expect(ImageAdjustLimits::hotSensitivityToSigma(100) == ImageAdjustLimits::kHotSigmaMax,
           "mapping: 100 -> sigma max");
    expect(ImageAdjustLimits::hotSensitivityToSigma(50) > ImageAdjustLimits::kHotSigmaMin
               && ImageAdjustLimits::hotSensitivityToSigma(50)
                      < ImageAdjustLimits::kHotSigmaMax,
           "mapping: 50 -> between");
    // Fuera de rango debe saturar, no desbordar.
    expect(ImageAdjustLimits::hotSensitivityToSigma(-10) == ImageAdjustLimits::kHotSigmaMin,
           "mapping: below range clamps");
    expect(ImageAdjustLimits::hotSensitivityToSigma(999) == ImageAdjustLimits::kHotSigmaMax,
           "mapping: above range clamps");

    // El recorrido tiene que quedarse dentro de la banda medida. Con el rango
    // antiguo (3..30) el valor 0 daba 3 sigma, que sobre los RAW de referencia
    // produce 1540 falsos positivos por Mpx, y el default caía en una zona sin
    // ninguna decisión que tomar.
    expect(ImageAdjustLimits::kHotSigmaMin >= 10.0,
           "mapping: el extremo agresivo no baja de 10 sigma");
    expect(ImageAdjustLimits::hotSensitivityToSigma(
               ImageAdjustLimits::kDefaultHotSensitivity) == 20.0,
           "mapping: el default cae en k=20, la meseta segura");
}

} // namespace

int main()
{
    testDefaultIdentity();
    testSodiumLp();
    testMercuryLp();
    testExposureIncrease();
    testExposureDecrease();
    testBrightnessIncrease();
    testContrastIncrease();
    testStackingLpWbEv();
    testMonotonicityExposure();
    testEmptyInput();
    testSpatialDetectsIsolatedBrightPixel();
    testSpatialIgnoresCleanBackground();
    testSpatialIgnoresCluster();
    testSpatialSensitivityGatesDetection();
    testNoSpatialFallbackOnDemosaicedImage();
    testOffByDefault();
    testExplicitMaskAppliesWithFilterOff();
    testMismatchMaskIsIgnored();
    test16bitSupport();
    testSensitivityMapping();

    if (failures == 0)
        std::printf("ALL TESTS PASSED\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);
    return failures;
}
