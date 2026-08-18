#include "processing/Pipeline.h"
#include "video/FFmpegVideoWriter.h"
#include "video/FFmpegVideoReader.h"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Genera un vídeo de prueba y lo estabiliza con el pipeline de dos pasadas,
// verificando que (a) no se descarta ningún frame y (b) el objeto queda
// centrado en el vídeo de salida.
namespace {
int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

bool generateInput(const std::string& path)
{
    FFmpegVideoWriter w;
    if (!w.open(path, 640, 360, 25.0))
        return false;
    cv::Point2f c(150.f, 90.f);
    const cv::Point2f vel(1.0f, 1.0f); // deriva lenta realista: el lag del suavizado (~2.3·|v|) es despreciable
    for (int i = 0; i < 40; ++i) {
        cv::Mat frame(360, 640, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::circle(frame, c, 20, cv::Scalar(255, 255, 255), cv::FILLED);
        if (!w.write(frame))
            return false;
        c += vel;
    }
    w.close();
    return true;
}

cv::Point2f centroid(const cv::Mat& bgr)
{
    cv::Mat gray, mask;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, mask, 100, 255, cv::THRESH_BINARY);
    cv::Moments m = cv::moments(mask, true);
    return {static_cast<float>(m.m10 / m.m00), static_cast<float>(m.m01 / m.m00)};
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::printf("uso: test_pipeline <in.mp4> <out.mp4>\n");
        return 2;
    }
    const std::string inPath = argv[1];
    const std::string outPath = argv[2];

    if (!generateInput(inPath)) {
        std::printf("FAIL: no se pudo generar el vídeo de entrada\n");
        return 1;
    }

    const cv::Rect2f roi(130.f, 70.f, 40.f, 40.f); // centrado en el disco inicial (150,90)
    PipelineStats stats;
    Pipeline pipeline;
    if (!pipeline.run(inPath, outPath, roi, BorderMode::Black, &stats)) {
        std::printf("FAIL: el pipeline no completó\n");
        return 1;
    }

    expect(stats.frames == 40, "se procesaron 40 frames");
    expect(stats.valid == 40, "tracking válido en los 40 frames");

    // Releer la salida: mismo número de frames y objeto centrado en todos.
    FFmpegVideoReader reader;
    if (!reader.open(outPath)) {
        std::printf("FAIL: no se pudo releer la salida\n");
        return 1;
    }

    const cv::Point2f expectedCenter(320.f, 180.f);
    Frame f;
    int n = 0;
    int maxDev = 0;
    while (reader.readNext(f)) {
        const cv::Point2f c = centroid(f.image);
        const int dev = static_cast<int>(std::lround(cv::norm(c - expectedCenter)));
        if (dev > maxDev)
            maxDev = dev;
        ++n;
    }
    expect(n == 40, "la salida conserva los 40 frames (nunca se descartan)");
    expect(maxDev <= 5, "objeto centrado en todos los frames de salida");
    std::printf("maxDesviación=%dpx\n", maxDev);

    std::remove(outPath.c_str());
    std::remove(inPath.c_str());

    if (failures == 0) {
        std::printf("OK\n");
        return 0;
    }
    return 1;
}