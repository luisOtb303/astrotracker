#include "video/FFmpegVideoWriter.h"
#include "video/FFmpegVideoReader.h"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

cv::Mat makeFrame(const cv::Point2f& c, int r)
{
    cv::Mat frame(360, 640, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::circle(frame, c, r, cv::Scalar(255, 255, 255), cv::FILLED);
    return frame;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("uso: test_writer <out.mp4>\n");
        return 2;
    }
    const std::string outPath = argv[1];

    const int n = 30;
    const cv::Point2f center0(320.f, 180.f);

    FFmpegVideoWriter writer;
    if (!writer.open(outPath, 640, 360, 25.0)) {
        std::printf("FAIL: no se pudo abrir el writer\n");
        return 1;
    }
    for (int i = 0; i < n; ++i) {
        cv::Mat f = makeFrame(center0 + cv::Point2f(1.f, 0.f) * i, 15);
        if (!writer.write(f)) {
            std::printf("FAIL: write frame %d\n", i);
            return 1;
        }
    }
    writer.close();
    expect(writer.frameCount() == n, "frames escritos == n");

    // Re-lectura del archivo generado.
    FFmpegVideoReader reader;
    if (!reader.open(outPath)) {
        std::printf("FAIL: no se pudo releer %s\n", outPath.c_str());
        return 1;
    }
    expect(reader.width() == 640 && reader.height() == 360, "dimensiones ok");
    expect(std::abs(reader.fps() - 25.0) < 0.5, "fps ok");

    Frame f;
    int read = 0;
    while (reader.readNext(f)) {
        cv::Mat gray, mask;
        cv::cvtColor(f.image, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, mask, 100, 255, cv::THRESH_BINARY);
        cv::Moments m = cv::moments(mask, true);
        if (m.m00 < 1.0) {
            expect(false, "frame vacío en lectura");
            break;
        }
        const cv::Point2f c(static_cast<float>(m.m10 / m.m00),
                            static_cast<float>(m.m01 / m.m00));
        const cv::Point2f expected = center0 + cv::Point2f(1.f, 0.f) * read;
        if (cv::norm(c - expected) > 4.0) {
            std::printf("FAIL: frame %d centro %f,%f (esperado %f,%f)\n",
                        read, c.x, c.y, expected.x, expected.y);
            ++failures;
        }
        ++read;
    }
    expect(read == n, "frames releídos == n");

    if (failures == 0) {
        std::printf("OK: %d frames ida y vuelta\n", n);
        return 0;
    }
    return 1;
}