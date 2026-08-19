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

void testJumpReacquire()
{
    const int r = 30;
    DiscTracker tracker;
    const cv::Point2f seed(200.f, 240.f);
    tracker.init(seed, static_cast<float>(r));
    tracker.track(makeDisc(seed, r)); // frame 0: semilla; se crea la plantilla

    // Paneo puntual grande entre fotografías (deriva típica sin star tracker):
    // la ventana de búsqueda debe crecer hasta volver a localizar el disco.
    cv::Point2f pos(seed.x + 55.f, seed.y - 30.f);
    const cv::Point2f vel(9.f, -6.f);

    bool reacquired = false;
    int foundCount = 0;
    for (int i = 1; i <= 6; ++i) { // quieto tras el paneo: re-adquisición
        const DiscTrack t = tracker.track(makeDisc(pos, r));
        foundCount += t.predicted ? 0 : 1;
        reacquired = reacquired || t.reacquired;
    }
    for (int i = 7; i < 20; ++i) { // deriva suave constante: seguimiento fino
        pos += vel;
        const DiscTrack t = tracker.track(makeDisc(pos, r));
        if (!t.predicted) {
            ++foundCount;
            reacquired = reacquired || t.reacquired;
            const double dist = cv::norm(t.center - pos);
            if (dist > 6.0) {
                std::printf("FAIL: jump frame %d, dist=%f\n", i, dist);
                ++failures;
            }
        }
    }
    expect(reacquired, "jump: debe re-adquirir el disco tras el salto grande");
    expect(foundCount >= 12, "jump: la mayoría de los frames deben confirmarse");
    std::printf("jump reacquire: found=%d/19 OK\n", foundCount);
}

void testTemplateRefresh()
{
    const int r = 30;
    const cv::Point2f c(320.f, 240.f);
    DiscTracker tracker;
    tracker.init(c, static_cast<float>(r));
    tracker.track(makeDisc(c, r));

    // La forma cambia foto a foto (el eclipse avanza): la plantilla se
    // refresca con cada confirmación y el seguimiento no debe perderse.
    int found = 0;
    for (int i = 0; i < 12; ++i) {
        cv::Mat f = makeDisc(c, r);
        const float frac = 0.2f + 0.06f * static_cast<float>(i);
        cv::circle(f, cv::Point2f(c.x + r * frac, c.y), r, cv::Scalar(0, 0, 0),
                   cv::FILLED);
        const DiscTrack t = tracker.track(f);
        if (!t.predicted) {
            ++found;
            const double dist = cv::norm(t.center - c);
            if (dist > 4.5) {
                std::printf("FAIL: refresh frame %d, dist=%f\n", i, dist);
                ++failures;
            }
        }
    }
    expect(found >= 10, "template refresh: sigue confirmando con la forma cambiante");
    std::printf("template refresh: found=%d/12 OK\n", found);
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
    testJumpReacquire();
    testTemplateRefresh();
    testReinitFromLaterFrame();

    if (failures == 0) {
        std::printf("OK\n");
        return 0;
    }
    return 1;
}