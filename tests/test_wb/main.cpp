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

cv::Mat makeTestImage16()
{
    cv::Mat img(100, 100, CV_16UC3, cv::Scalar(32768, 32768, 32768));
    return img;
}

void testKelvinZeroIdentity()
{
    const cv::Mat img = makeTestImage();
    ImageAdjust adj;
    adj.wbKelvin = 0; // auto = identity
    const cv::Mat out = img::apply(img, adj, 6500);
    expect(out.rows == img.rows && out.cols == img.cols,
           "kelvin=0: dimensions preserved");
    const cv::Scalar mean = cv::mean(out);
    expect(std::abs(mean[0] - 128.0) < 2.0, "kelvin=0: B ~128");
    expect(std::abs(mean[2] - 128.0) < 2.0, "kelvin=0: R ~128");
}

void testWarmDirection()
{
    const cv::Mat img = makeTestImage();
    ImageAdjust adj;
    adj.wbKelvin = 10000;
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[2] > mean[0], "10000K: R > B (warm)");
}

void testCoolDirection()
{
    const cv::Mat img = makeTestImage();
    ImageAdjust adj;
    adj.wbKelvin = 3000;
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[0] > mean[2], "3000K: B > R (cool)");
}

void testGreenUnchanged()
{
    const cv::Mat img = makeTestImage();
    ImageAdjust adj;
    adj.wbKelvin = 10000;
    const cv::Mat out = img::apply(img, adj, 6500);
    const cv::Scalar mean = cv::mean(out);
    expect(std::abs(mean[1] - 128.0) < 2.0, "green channel ~128 at 10000K");
}

void test16Bit()
{
    const cv::Mat img = makeTestImage16();
    ImageAdjust adj;
    adj.wbKelvin = 10000;
    const cv::Mat out = img::apply(img, adj, 6500);
    expect(out.depth() == CV_16U, "16-bit: depth preserved");
    const cv::Scalar mean = cv::mean(out);
    expect(mean[2] > 33000.0, "16-bit: R increased");
    expect(mean[0] < 32000.0, "16-bit: B decreased");
}

void testEmptyInput()
{
    const cv::Mat empty;
    ImageAdjust adj;
    adj.wbKelvin = 5000;
    const cv::Mat out = img::apply(empty, adj, 6500);
    expect(out.empty(), "empty input returns empty");
}

} // namespace

int main()
{
    testKelvinZeroIdentity();
    testWarmDirection();
    testCoolDirection();
    testGreenUnchanged();
    test16Bit();
    testEmptyInput();

    if (failures == 0)
        std::printf("ALL TESTS PASSED\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);
    return failures;
}
