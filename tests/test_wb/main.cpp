#include "common/WhiteBalance.h"

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

void testZeroWarmth()
{
    const cv::Mat img = makeTestImage();
    const cv::Mat out = wb::apply(img, 0);
    expect(out.rows == img.rows && out.cols == img.cols,
           "warmth=0: dimensions preserved");
    const cv::Scalar mean = cv::mean(out);
    expect(std::abs(mean[0] - 128.0) < 1.0, "warmth=0: B unchanged");
    expect(std::abs(mean[2] - 128.0) < 1.0, "warmth=0: R unchanged");
}

void testWarmPositive()
{
    const cv::Mat img = makeTestImage();
    const cv::Mat out = wb::apply(img, 100);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[2] > 190.0, "warmth=+100: R increased");
    expect(mean[0] < 65.0, "warmth=+100: B decreased");
}

void testWarmNegative()
{
    const cv::Mat img = makeTestImage();
    const cv::Mat out = wb::apply(img, -100);
    const cv::Scalar mean = cv::mean(out);
    expect(mean[0] > 190.0, "warmth=-100: B increased");
    expect(mean[2] < 65.0, "warmth=-100: R decreased");
}

void testGreenUnchanged()
{
    const cv::Mat img = makeTestImage();
    const cv::Mat out = wb::apply(img, 100);
    const cv::Scalar mean = cv::mean(out);
    expect(std::abs(mean[1] - 128.0) < 1.0, "green channel unchanged at +100");
}

void test16Bit()
{
    const cv::Mat img = makeTestImage16();
    const cv::Mat out = wb::apply(img, 100);
    expect(out.depth() == CV_16U, "16-bit: depth preserved");
    const cv::Scalar mean = cv::mean(out);
    expect(mean[2] > 40000.0, "16-bit: R increased");
    expect(mean[0] < 20000.0, "16-bit: B decreased");
}

void testEmptyInput()
{
    const cv::Mat empty;
    const cv::Mat out = wb::apply(empty, 50);
    expect(out.empty(), "empty input returns empty");
}

} // namespace

int main()
{
    testZeroWarmth();
    testWarmPositive();
    testWarmNegative();
    testGreenUnchanged();
    test16Bit();
    testEmptyInput();

    if (failures == 0)
        std::printf("ALL TESTS PASSED\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);
    return failures;
}
