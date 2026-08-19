#include "stills/PhotoSequenceReader.h"
#include "tracking/CircleEstimator.h"
#include "tracking/DiscTracker.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

// Test local opcional: verifica el seguimiento del disco (DiscTracker) con
// CR2 reales de un eclipse (testdata/eclipse/, que no está en el repo). Si la
// carpeta no existe, el test se omite sin fallar.
//
// El disco se detecta automáticamente (Otsu + blob + minEnclosingCircle) solo
// para la semilla de la foto 0; con eso se siembra y se sigue la secuencia.
// Esa detección es una aproximación y puede fallar en escenas reales (halo,
// glare, eclipse parcial). Por eso este test NO falla: genera un informe por
// foto y overlays PNG (círculo de la semilla/user-trut en verde, círculo
// seguido en rojo) en <carpeta>/_props para su inspección visual. El juicio
// definitivo lo hace el usuario en la app o mirando los overlays.

namespace {

// Círculo del disco más brillante de la imagen (Otsu + mayor blob conexo).
bool detectDisc(const cv::Mat& bgr, cv::Point2f& center, float& radius)
{
    if (bgr.empty())
        return false;
    cv::Mat g;
    cv::cvtColor(bgr, g, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(g, g, cv::Size(0, 0), 1.5);
    cv::Mat bin;
    cv::threshold(g, bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::Mat labels, stats, centroids;
    const int n = cv::connectedComponentsWithStats(bin, labels, stats, centroids);
    if (n < 2)
        return false;
    int bestIdx = -1;
    int bestArea = 0;
    for (int i = 1; i < n; ++i) {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > bestArea) {
            bestArea = area;
            bestIdx = i;
        }
    }
    if (bestIdx < 0)
        return false;
    center = cv::Point2f(static_cast<float>(centroids.at<double>(bestIdx, 0)),
                         static_cast<float>(centroids.at<double>(bestIdx, 1)));
    const int l = stats.at<int>(bestIdx, cv::CC_STAT_LEFT);
    const int t = stats.at<int>(bestIdx, cv::CC_STAT_TOP);
    const int w = stats.at<int>(bestIdx, cv::CC_STAT_WIDTH);
    const int h = stats.at<int>(bestIdx, cv::CC_STAT_HEIGHT);
    radius = static_cast<float>(std::max(w, h)) * 0.65f;
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    const char* folder = (argc > 1) ? argv[1] : TESTDATA_ECLIPSE;

    PhotoSequenceReader reader;
    if (!reader.openFolder(folder) || reader.count() == 0) {
        std::printf("SKIP: no hay fotos en %s\n", folder);
        return 0;
    }
    const int64_t n = reader.count();
    std::printf("secuencia: %lld fotos CR2\n", static_cast<long long>(n));

    const int dim = 1600;
    std::vector<cv::Point2f> truth(n);
    std::vector<float> truthR(n);
    std::vector<cv::Mat> frames(n);
    for (int64_t i = 0; i < n; ++i) {
        cv::Mat fr;
        if (!reader.readAt(i, fr, dim)) {
            std::printf("SKIP: no se pudo leer la foto %lld\n", static_cast<long long>(i));
            return 0;
        }
        frames[i] = fr;
        detectDisc(fr, truth[i], truthR[i]);
        if (i > 0)
            std::printf("  foto %lld: r=%5.1f  desplazamiento Otsu desde la anterior=%6.2f px\n",
                        static_cast<long long>(i), truthR[i], cv::norm(truth[i] - truth[i - 1]));
        else
            std::printf("  foto %lld: r=%5.1f\n", static_cast<long long>(i), truthR[i]);
    }

    // Seguimiento normal secuencial desde la semilla detectada en la foto 0.
    std::vector<DiscTrack> tracks(n);
    {
        DiscTracker tracker;
        tracker.init(truth[0], truthR[0]);
        printf("--- seguimiento normal ---\n");
        for (int64_t i = 0; i < n; ++i) {
            tracks[i] = tracker.track(frames[i]);
            const double err = cv::norm(tracks[i].center - truth[i]);
            printf("  foto %lld: %s estado=%d pred=%d | Otsu=(%5.1f,%5.1f) r=%5.1f | track=(%5.1f,%5.1f) | err(Otsu)=%6.2f px\n",
                   static_cast<long long>(i),
                   reader.fileName(i).c_str(), static_cast<int>(tracks[i].status),
                   tracks[i].predicted ? 1 : 0,
                   truth[i].x, truth[i].y, truthR[i], tracks[i].center.x, tracks[i].center.y, err);
        }
    }

    // Overlays: círculo detectado (verde) + círculo seguido (rojo).
    {
        std::string dir = folder;
        dir += "/_props";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        for (int64_t i = 0; i < n; ++i) {
            cv::Mat vis = frames[i].clone();
            cv::circle(vis, truth[i], cvRound(truthR[i]), cv::Scalar(0, 255, 0), 2);
            cv::circle(vis, truth[i], 4, cv::Scalar(0, 255, 0), cv::FILLED);
            const DiscTrack& t = tracks[i];
            if (t.radius > 0.f) {
                cv::circle(vis, t.center, cvRound(t.radius), cv::Scalar(0, 0, 255), 2);
                cv::circle(vis, t.center, 4, cv::Scalar(0, 0, 255), t.predicted ? 0 : -1);
            }
            char fname[128];
            std::snprintf(fname, sizeof fname, "%s/%03lld_overlay.png", dir.c_str(),
                          static_cast<long long>(i));
            cv::imwrite(fname, vis);
        }
        printf("overlays (verde=Otsu/semilla, rojo=seguido) en %s\n", dir.c_str());
    }

    printf("Informe generado (sin fallo): revisa los overlays y/o la app para juzgar.\n");
    return 0;
}