#include "stabilization/Stabilizer.h"
#include "stabilization/SmoothingFilter.h"

#include <opencv2/imgproc.hpp>
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
} // namespace

int main()
{
    // SmoothingFilter: converge hacia el valor de entrada y reduce oscilación.
    SmoothingFilter sf(0.5f);
    sf.reset({100.f, 100.f});
    const cv::Point2f v1 = sf.push({120.f, 100.f});
    expect(std::abs(v1.x - 110.f) < 0.01f, "EMA step 1 (110)");
    const cv::Point2f v2 = sf.push({120.f, 100.f});
    expect(std::abs(v2.x - 115.f) < 0.01f, "EMA step 2 (115)");

    // Stabilizer: con centro exacto, el offset centra el objeto.
    const cv::Size frameSize(640, 480);
    const cv::Point2f obj(200.f, 150.f);
    Stabilizer st;
    st.reset(frameSize, obj);
    // Primer offset: centra el objeto inmediatamente (target = centro del frame).
    expect(std::abs(st.offset().x - (320.f - 200.f)) < 0.5f, "offset x inicial");
    expect(std::abs(st.offset().y - (240.f - 150.f)) < 0.5f, "offset y inicial");

    // Con objeto ya centrado (corregido), el offset tiende a anularse conforme
    // el suavizado converge.
    const cv::Point2f center = st.target().point;
    for (int i = 0; i < 30; ++i)
        st.update(center);
    expect(cv::norm(st.offset()) < 0.5f, "offset nulo con objeto centrado");

    // apply(): el objeto aparece centrado en el frame estabilizado.
    cv::Mat frame(frameSize, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::circle(frame, obj, 15, cv::Scalar(255, 255, 255), cv::FILLED);
    const cv::Point2f off = st.target().point - obj;
    const cv::Mat stab = Stabilizer::apply(frame, off);
    // El centro del disco blanco debe estar en el centro del frame estabilizado.
    cv::Mat gray;
    cv::cvtColor(stab, gray, cv::COLOR_BGR2GRAY);
    cv::Moments m = cv::moments(gray, true);
    const cv::Point2f centroid(static_cast<float>(m.m10 / m.m00),
                               static_cast<float>(m.m01 / m.m00));
    expect(cv::norm(centroid - center) < 2.0f, "disco centrado tras apply");

    // Smoothing reduce jitter: centro estimado entre las muestras ruidosas.
    Stabilizer st2;
    st2.reset(frameSize, {320.f, 240.f});
    cv::Point2f noisy(320.f, 240.f);
    for (int i = 0; i < 40; ++i) {
        noisy.x = 320.f + (i % 2 ? 10.f : -10.f);
        const cv::Point2f o = st2.update(noisy);
        if (i >= 10) {
            expect(std::abs(st2.offset().x) < 9.f, "jitter atenuado por suavizado");
        }
    }

    if (failures == 0) {
        std::printf("OK\n");
        return 0;
    }
    return 1;
}