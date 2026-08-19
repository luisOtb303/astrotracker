#include "tracking/DiscTracker.h"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

// Cielo oscuro con ruido suave y un disco brillante de radio r centrado en c.
cv::Mat makeDisc(const cv::Point2f& c, int r)
{
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(40, 40, 40));
    cv::RNG rng(42);
    cv::randn(frame, cv::Scalar(40), cv::Scalar(6));
    cv::circle(frame, c, r, cv::Scalar(245, 245, 245), cv::FILLED);
    return frame;
}

// Ocluye el disco con un rectángulo negro (nube/montaña): deja visible solo la
// parte superior del arco (creciente invertido, como el sol tras una montaña).
cv::Mat occludeBottom(const cv::Mat& src, const cv::Point2f& c, int r)
{
    cv::Mat out = src.clone();
    cv::rectangle(out, cv::Rect(0, static_cast<int>(c.y) - r / 2, 640,
                                static_cast<int>(out.rows - c.y + r / 2)),
                  cv::Scalar(0, 0, 0), cv::FILLED);
    return out;
}

// Eclipse parcial: un disco oscuro solapa parte del brillante (creciente).
cv::Mat makeCrescent(const cv::Point2f& c, int r)
{
    cv::Mat frame = makeDisc(c, r);
    const cv::Point2f umbra(c.x + r * 0.5f, c.y);
    cv::circle(frame, umbra, r, cv::Scalar(0, 0, 0), cv::FILLED);
    return frame;
}

// Nube total: solo cielo oscuro, sin disco (objeto completamente oculto).
cv::Mat makeCovered(const cv::Point2f&, int)
{
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(40, 40, 40));
    cv::RNG rng(11);
    cv::randn(frame, cv::Scalar(40), cv::Scalar(6));
    return frame;
}

void testFullDiscMoving()
{
    const int r = 30;
    const cv::Point2f vel(1.2f, 0.8f);
    cv::Point2f pos(320.f, 240.f);

    DiscTracker tracker;
    tracker.init(pos, static_cast<float>(r));

    bool anyValid = false;
    for (int i = 1; i < 40; ++i) {
        pos += vel;
        const DiscTrack t = tracker.track(makeDisc(pos, r));
        if (t.status == TrackStatus::VALID && !t.predicted) {
            anyValid = true;
            const double dist = cv::norm(t.center - pos);
            if (dist > 3.0) {
                std::printf("FAIL: disco lleno frame %d, dist=%f\n", i, dist);
                ++failures;
            }
        }
        expect(std::abs(t.radius - r) < 0.1f, "el radio fijo no debe cambiar");
    }
    expect(anyValid, "disco lleno: al menos una medida VALID");
    std::printf("full disc: OK\n");
}

void testCrescent()
{
    const int r = 30;
    const cv::Point2f pos(320.f, 240.f);
    DiscTracker tracker;
    tracker.init(pos, static_cast<float>(r));

    const DiscTrack t = tracker.track(makeCrescent(pos, r));
    const double dist = cv::norm(t.center - pos);
    expect(!t.predicted, "creciente: debe tener medición");
    expect(dist <= 4.0, "creciente: centro preciso");
    expect(t.status == TrackStatus::VALID, "creciente: arco amplio suficiente → VALID");
    std::printf("crescent: dist=%f\n", dist);
}

void testSunBehindMountain()
{
    const int r = 30;
    const cv::Point2f pos(320.f, 240.f);
    cv::Mat frame = occludeBottom(makeDisc(pos, r), pos, r);

    DiscTracker tracker;
    tracker.init(pos, static_cast<float>(r));
    const DiscTrack t = tracker.track(frame);
    const double dist = cv::norm(t.center - pos);
    expect(!t.predicted, "montaña: debe tener medición del arco visible");
    expect(dist <= 4.0, "montaña: centro preciso");
    std::printf("mountain: dist=%f ratioStatus=%d\n", dist, static_cast<int>(t.status));
}

void testFullOcclusionPredicts()
{
    const int r = 30;
    cv::Point2f pos(320.f, 240.f);
    DiscTracker tracker;
    tracker.init(pos, static_cast<float>(r));

    // Un frame válido y luego varios cubiertos: debe mantener la predicción.
    DiscTrack last = tracker.track(makeDisc(pos, r));
    for (int i = 0; i < 6; ++i) {
        pos += cv::Point2f(0.5f, 0.3f);
        last = tracker.track(makeCovered(pos, r));
    }
    expect(last.predicted, "nube total: posición supuesta (predicción)");
    expect(last.status == TrackStatus::LOST, "nube total: estado LOST tras varios misses");
    const double dist = cv::norm(last.center - pos);
    expect(dist < 8.0, "nube total: la predicción se mantiene cerca");
    std::printf("covered: dist=%f\n", dist);
}

void testReinitFromLaterFrame()
{
    const int r = 30;
    const cv::Point2f vel(0.6f, 0.4f);
    cv::Point2f pos(300.f, 240.f);

    // Simula el doble paso: hacia delante desde la semilla y hacia atrás.
    DiscTracker tracker;
    tracker.init(pos, static_cast<float>(r));
    for (int i = 1; i < 20; ++i) {
        pos += vel;
        const DiscTrack t = tracker.track(makeDisc(pos, r));
        expect(std::abs(t.radius - r) < 0.1f, "radio fijo en retorno");
    }
    std::printf("forward pass: OK\n");
}
} // namespace

int main()
{
    testFullDiscMoving();
    testCrescent();
    testSunBehindMountain();
    testFullOcclusionPredicts();
    testReinitFromLaterFrame();

    if (failures == 0) {
        std::printf("OK\n");
        return 0;
    }
    return 1;
}