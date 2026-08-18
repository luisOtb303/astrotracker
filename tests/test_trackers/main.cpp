#include "tracking/TemplateTracker.h"
#include "tracking/CentroidTracker.h"

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

// Frame sintético: disco brillante (radio r) sobre fondo negro, centrado en c.
cv::Mat makeFrame(const cv::Point2f& c, int r)
{
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::circle(frame, c, r, cv::Scalar(255, 255, 255), cv::FILLED);
    return frame;
}

void runTracker(ITracker& tracker, const char* name)
{
    const int r = 20;
    cv::Point2f pos(320.f, 240.f);
    const cv::Point2f vel(1.2f, 0.8f);

    cv::Mat f0 = makeFrame(pos, r);
    cv::Rect2f roi(pos.x - r - 5, pos.y - r - 5, 2 * (r + 5), 2 * (r + 5));
    if (!tracker.init(f0, roi)) {
        expect(false, (std::string("init falla (") + name + ")").c_str());
        return;
    }

    bool anyFound = false;
    for (int i = 1; i < 30; ++i) {
        pos += vel;
        cv::Mat f = makeFrame(pos, r);
        const TrackResult res = tracker.track(f);
        if (res.found) {
            anyFound = true;
            const cv::Point2f center = res.rect.tl() + cv::Point2f(res.rect.width, res.rect.height) * 0.5f;
            const double dist = cv::norm(center - pos);
            if (dist > 3.0) {
                std::printf("FAIL: %s frame %d, dist=%f\n", name, i, dist);
                ++failures;
            }
        }
    }
    expect(anyFound, (std::string("sin mediciones (") + name + ")").c_str());
}

void runCentroidPartialOcclusion()
{
    CentroidTracker tracker(2.5f, 0.15f);
    const int r = 20;
    cv::Point2f pos(320.f, 240.f);
    cv::Mat f0 = makeFrame(pos, r);
    cv::Rect2f roi(pos.x - r - 5, pos.y - r - 5, 2 * (r + 5), 2 * (r + 5));
    if (!tracker.init(f0, roi)) {
        expect(false, "init falla (centroid oclusión)");
        return;
    }

    // Oclusión parcial: solo la mitad del disco es visible.
    cv::Mat f = makeFrame(pos, r);
    cv::rectangle(f, cv::Rect(0, 0, 640, 245), cv::Scalar(0, 0, 0), cv::FILLED);
    const TrackResult res = tracker.track(f);
    expect(res.found, "centroid debe encontrar el disco semi-oculto");
}
} // namespace

int main()
{
    TemplateTracker tmpl;
    runTracker(tmpl, "template");

    CentroidTracker centroid;
    runTracker(centroid, "centroid");

    runCentroidPartialOcclusion();

    if (failures == 0) {
        std::printf("OK\n");
        return 0;
    }
    return 1;
}