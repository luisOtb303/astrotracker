#include "common/ImageAdjust.h"

#include <opencv2/core.hpp>
#include <cmath>
#include <cstdio>

// El balance de blancos es RELATIVO: 0 = la foto tal como sale del decodificador,
// + = más cálido, - = más frío. No hay Kelvin de referencia porque no se puede
// saber con qué luz se tomó la foto.

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
    return cv::Mat(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
}

cv::Mat makeTestImage16()
{
    return cv::Mat(100, 100, CV_16UC3, cv::Scalar(32768, 32768, 32768));
}

// 0 debe devolver la imagen EXACTAMENTE igual: es la garantía de que poner el
// control a neutro restaura el original.
void testWarmthZeroIsIdentity()
{
    const cv::Mat img = makeTestImage();
    const cv::Mat out = img::apply(img, ImageAdjust{});
    expect(cv::norm(img, out, cv::NORM_INF) == 0.0, "warmth=0: pixel-identico al original");
}

void testWarmDirection()
{
    const cv::Mat img = makeTestImage();
    ImageAdjust adj;
    adj.wbWarmth = 60;
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar m = cv::mean(out);
    expect(m[2] > m[0], "warmth>0: R > B (mas calido)");
}

void testCoolDirection()
{
    const cv::Mat img = makeTestImage();
    ImageAdjust adj;
    adj.wbWarmth = -60;
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar m = cv::mean(out);
    expect(m[0] > m[2], "warmth<0: B > R (mas frio)");
}

void testGreenUnchanged()
{
    const cv::Mat img = makeTestImage();
    ImageAdjust adj;
    adj.wbWarmth = 80;
    const cv::Mat out = img::apply(img, adj);
    const cv::Scalar m = cv::mean(out);
    expect(std::abs(m[1] - 128.0) < 1.0, "canal G intacto");
}

// El signo tiene que ser coherente en todo el recorrido: subir calienta siempre.
void testMonotonic()
{
    const cv::Mat img = makeTestImage();
    float prevDiff = -1e9f;
    for (int w = -100; w <= 100; w += 10) {
        ImageAdjust adj;
        adj.wbWarmth = w;
        const cv::Scalar m = cv::mean(img::apply(img, adj));
        const float diff = static_cast<float>(m[2] - m[0]);
        if (w > -100)
            expect(diff > prevDiff, "monotono: mas warmth = mas calido");
        prevDiff = diff;
    }
}

void testExtremes()
{
    const cv::Mat img = makeTestImage();
    ImageAdjust warm;
    warm.wbWarmth = ImageAdjustLimits::kMaxWarmth;
    ImageAdjust cool;
    cool.wbWarmth = ImageAdjustLimits::kMinWarmth;
    const cv::Scalar mw = cv::mean(img::apply(img, warm));
    const cv::Scalar mc = cv::mean(img::apply(img, cool));
    expect(mw[2] > 128.0f, "extremo +: R sube");
    expect(mc[2] < 128.0f, "extremo -: R baja");
    expect(mw[2] <= 255.0f && mc[0] <= 255.0f, "extremos: sin desbordar");
}

void test16Bit()
{
    const cv::Mat img = makeTestImage16();
    ImageAdjust adj;
    adj.wbWarmth = 60;
    const cv::Mat out = img::apply(img, adj);
    expect(out.depth() == CV_16U, "16-bit: profundidad preservada");
    const cv::Scalar m = cv::mean(out);
    expect(m[2] > 33000.0, "16-bit: R sube");
    expect(m[0] < 32000.0, "16-bit: B baja");
}

void testEmptyInput()
{
    const cv::Mat empty;
    ImageAdjust adj;
    adj.wbWarmth = 50;
    expect(img::apply(empty, adj).empty(), "entrada vacia devuelve vacio");
}

} // namespace

int main()
{
    testWarmthZeroIsIdentity();
    testWarmDirection();
    testCoolDirection();
    testGreenUnchanged();
    testMonotonic();
    testExtremes();
    test16Bit();
    testEmptyInput();

    if (failures == 0)
        std::printf("ALL TESTS PASSED\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);
    return failures;
}