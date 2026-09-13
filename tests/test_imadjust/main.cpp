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
    const cv::Mat out = img::apply(img, ImageAdjust{}, 6500);
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
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[2] < 100.0, "sodium LP: R reduced");
    expect(mean[0] > 128.0, "sodium LP: B boosted");
}

void testMercuryLp()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    ImageAdjust adj;
    adj.lpMercury = 100;
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[1] < 128.0, "mercury LP: G reduced");
    expect(mean[0] < 128.0, "mercury LP: B reduced");
}

void testExposureIncrease()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(50, 50, 50));
    ImageAdjust adj;
    adj.exposureEv = 20; // +2 EV
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[0] > 150.0, "exposure +2EV: channels brightened");
}

void testExposureDecrease()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(200, 200, 200));
    ImageAdjust adj;
    adj.exposureEv = -20; // -2 EV
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[0] < 100.0, "exposure -2EV: channels darkened");
}

void testBrightnessIncrease()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(100, 100, 100));
    ImageAdjust adj;
    adj.brightness = 50;
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[0] > 140.0, "brightness +50: channels brighter");
}

void testContrastIncrease()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    // High contrast around midpoint: no change at 128, but helps test pipeline
    ImageAdjust adj;
    adj.contrast = 80;
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(std::abs(mean[0] - 128.0) < 10.0, "contrast: mid-gray ~128");
}

void testStackingLpWbEv()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    ImageAdjust adj;
    adj.lpSodium = 50;
    adj.wbKelvin = 10000;
    adj.exposureEv = 10;
    const cv::Mat out = img::apply(img, adj, 6500);
    expect(!out.empty(), "stacking: output not empty");
    expect(out.rows == 100 && out.cols == 100, "stacking: dimensions preserved");
}

void testMonotonicityExposure()
{
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    ImageAdjust a1, a2;
    a1.exposureEv = 0;
    a2.exposureEv = 20;
    const cv::Scalar m1 = cv::mean(img::apply(img, a1, 6500));
    const cv::Scalar m2 = cv::mean(img::apply(img, a2, 6500));
    expect(m2[0] > m1[0], "monotonicity: higher EV = brighter B");
}

void testEmptyInput()
{
    const cv::Mat empty;
    const cv::Mat out = img::apply(empty, ImageAdjust{}, 6500);
    expect(out.empty(), "empty input returns empty");
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

    if (failures == 0)
        std::printf("ALL TESTS PASSED\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);
    return failures;
}
