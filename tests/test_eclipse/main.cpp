#include "stills/PhotoSequenceReader.h"
#include "tracking/DiscArcFit.h"
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
// La semilla y la referencia de cada foto se obtienen con DiscArcFit: el
// CENTRO del círculo que forma el arco visible del disco (fase parcial,
// creciente, corona), no el punto más brillante. La referencia es verde, el
// círculo seguido es rojo y el informe mide el error del seguimiento contra
// esa referencia. El juicio definitivo lo hace el usuario en la app o mirando
// los overlays (este test NO falla).

int main(int argc, char** argv)
{
    int failures = 0;
    const char* folder = (argc > 1) ? argv[1] : TESTDATA_ECLIPSE;

    PhotoSequenceReader reader;
    if (!reader.openFolder(folder) || reader.count() == 0) {
        std::printf("SKIP: no hay fotos en %s\n", folder);
        return 0;
    }
    const int64_t n = reader.count();
    std::printf("secuencia: %lld fotos CR2\n", static_cast<long long>(n));

    const int dim = 1600;
    std::vector<cv::Mat> frames(n);
    std::vector<cv::Mat> gray(n);
    for (int64_t i = 0; i < n; ++i) {
        if (!reader.readAt(i, frames[i], dim)) {
            std::printf("SKIP: no se pudo leer la foto %lld\n", static_cast<long long>(i));
            return 0;
        }
        cv::cvtColor(frames[i], gray[i], cv::COLOR_BGR2GRAY);
    }

    // Regresión de la caché de análisis: las lecturas anteriores (RAW con
    // maxDim) deben haber dejado entradas JPG en _astrotracker_cache/.
    {
        std::string cacheDir = folder;
        cacheDir += "/_astrotracker_cache";
        int cached = 0;
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(cacheDir, ec))
            if (!ec && e.is_regular_file() &&
                e.path().extension() == ".jpg")
                ++cached;
        if (cached < 1) {
            std::printf("FAIL: la caché de análisis está vacía (%s)\n",
                        cacheDir.c_str());
            ++failures;
        } else {
            std::printf("caché de análisis: %d entradas en %s\n", cached,
                        cacheDir.c_str());
        }
    }

    // Semilla: círculo real del disco en la foto 0 (barrido de radio fijo).
    const cv::Rect full(0, 0, frames[0].cols, frames[0].rows);
    const float radiusGuess = std::max(20.f, 0.10f * static_cast<float>(frames[0].cols));
    const DiscArcEstimate seed =
        DiscArcFit::fitDisc(gray[0], full, cv::Point2f(frames[0].cols * 0.5f,
                                                       frames[0].rows * 0.5f),
                            radiusGuess, 3.f);
    if (!seed.ok) {
        std::printf("SKIP: no se pudo ajustar el círculo del disco en la foto 0\n");
        return 0;
    }
    std::printf("semilla foto 0: centro=(%5.1f,%5.1f) r=%5.1f  arco=%.0f° (soporte %d/%d)\n",
                seed.center.x, seed.center.y, seed.radius, seed.spanDeg, seed.support,
                seed.contourCount);

    // Referencia por foto: centro del círculo de radio fijo que forma el arco
    // visible (criterio del usuario para fases parciales/crecientes/corona).
    std::vector<cv::Point2f> ref(n);
    std::vector<float> refSpan(n);
    std::vector<bool> refOk(n, false);
    ref[0] = seed.center;
    refSpan[0] = seed.spanDeg;
    refOk[0] = true;
    for (int64_t i = 1; i < n; ++i) {
        const DiscArcEstimate e =
            DiscArcFit::fitFixedRadius(gray[i], full, ref[i - 1], seed.radius, 3.f);
        refOk[i] = e.ok;
        if (e.ok) {
            ref[i] = e.center;
            refSpan[i] = e.spanDeg;
        } else {
            ref[i] = ref[i - 1];
        }
    }

    // Seguimiento normal secuencial desde la semilla detectada en la foto 0.
    std::vector<DiscTrack> tracks(n);
    {
        DiscTracker tracker;
        tracker.init(seed.center, seed.radius);
        printf("--- seguimiento (verde=referencia del arco, rojo=seguido) ---\n");
        for (int64_t i = 0; i < n; ++i) {
            tracks[i] = tracker.track(frames[i]);
            const double err = cv::norm(tracks[i].center - ref[i]);
            printf("foto %lld: %s estado=%d pred=%d | ref=(%5.1f,%5.1f) arco=%.0f°%s | "
                   "track=(%5.1f,%5.1f) | err(ref)=%6.2f px\n",
                   static_cast<long long>(i), reader.fileName(i).c_str(),
                   static_cast<int>(tracks[i].status), tracks[i].predicted ? 1 : 0,
                   ref[i].x, ref[i].y, refSpan[i], refOk[i] ? "" : " (ref débil)",
                   tracks[i].center.x, tracks[i].center.y, err);
        }
    }

    // Overlays: círculo de referencia (verde) + círculo seguido (rojo).
    {
        std::string dir = folder;
        dir += "/_props";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        for (int64_t i = 0; i < n; ++i) {
            cv::Mat vis = frames[i].clone();
            cv::circle(vis, ref[i], cvRound(seed.radius), cv::Scalar(0, 255, 0), 2);
            cv::circle(vis, ref[i], 4, cv::Scalar(0, 255, 0), cv::FILLED);
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
        printf("overlays (verde=centro del disco por arco, rojo=seguido) en %s\n",
               dir.c_str());
    }

    printf("Informe generado (sin fallo): revisa los overlays y/o la app para juzgar.\n");
    return failures == 0 ? 0 : 1;
}