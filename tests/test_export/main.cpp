// Test de regresión del centrado en la exportación de fotos.
//
// Genera fotos sintéticas con un disco brillante en posiciones conocidas y
// bien separadas del centro, ejecuta el PhotoExportWorker real (imágenes JPG a
// resolución Visor y vídeo MP4 FHD con fotogramas intermedios) y comprueba que
// el centroide del disco queda en el centro del frame en TODAS las salidas,
// incluidos los fotogramas interpolados.

#include "common/Frame.h"
#include "stills/PhotoExportWorker.h"
#include "video/FFmpegVideoReader.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_failures = 0;

static void fail(const char* file, int line, const std::string& msg)
{
    std::fprintf(stderr, "FALLO %s:%d: %s\n", file, line, msg.c_str());
    ++g_failures;
}

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond))                                                    \
            fail(__FILE__, __LINE__, #cond);                            \
    } while (0)

// Centroide de los píxeles brillantes (disco) en una imagen BGR8.
static cv::Point2f centroid(const cv::Mat& bgr, int threshold = 150)
{
    cv::Mat g;
    cv::cvtColor(bgr, g, cv::COLOR_BGR2GRAY);
    cv::Mat mask;
    cv::threshold(g, mask, threshold, 255, cv::THRESH_BINARY);
    const cv::Moments m = cv::moments(mask, true);
    if (m.m00 <= 0.0)
        return {-1.f, -1.f};
    return {static_cast<float>(m.m10 / m.m00), static_cast<float>(m.m01 / m.m00)};
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const fs::path root = fs::temp_directory_path() / "astrotracker_test_export";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    // 10 fotos sintéticas de 2000x1500 con un disco de radio 60 en posiciones
    // conocidas y bien separadas del centro del encuadre.
    constexpr int W = 2000;
    constexpr int H = 1500;
    constexpr int R = 60;
    constexpr int N = 10;
    const cv::Point2f srcCenter[N] = {
        {300.f, 250.f},  {600.f, 420.f},  {950.f, 650.f},  {1350.f, 920.f},
        {1750.f, 1120.f}, {1500.f, 1300.f}, {1050.f, 1400.f}, {700.f, 1250.f},
        {400.f, 950.f},  {250.f, 620.f}};

    std::vector<std::string> paths;
    for (int i = 0; i < N; ++i) {
        cv::Mat img(H, W, CV_8UC3, cv::Scalar::all(8));
        cv::circle(img, srcCenter[i], R, cv::Scalar(255, 255, 255), cv::FILLED);
        char name[32];
        std::snprintf(name, sizeof name, "img_%02d.jpg", i + 1);
        const std::string path = (root / name).string();
        if (!cv::imwrite(path, img)) {
            std::fprintf(stderr, "no se pudo escribir %s\n", path.c_str());
            return 1;
        }
        paths.push_back(path);
    }

    // Tracks en el espacio de análisis: la exportación a Visor lee a maxDim 1600
    // (escala 1600/2000 = 0.8). Los centros conocidos se escalan igual.
    const float scale = 1600.f / W;
    std::vector<DiscTrack> tracks;
    tracks.reserve(N);
    for (int i = 0; i < N; ++i) {
        DiscTrack t;
        t.center = {srcCenter[i].x * scale, srcCenter[i].y * scale};
        t.radius = R * scale;
        t.status = TrackStatus::VALID;
        tracks.push_back(t);
    }

    QStringList pathsQt;
    for (const std::string& p : paths)
        pathsQt << QString::fromStdString(p);

    // --- Exportación a JPG (resolución Visor): salida 1600x1200, disco en (800,600).
    {
        const fs::path outDir = root / "jpg";
        fs::create_directories(outDir, ec);

        PhotoExportWorker::Settings st;
        st.format = PhotoExportWorker::Format::Jpg;
        st.resolution = PhotoExportWorker::Resolution::Visor;
        st.outDir = QString::fromStdString(outDir.string());

        PhotoExportWorker worker(pathsQt, tracks, 1600, st);
        worker.run();

        for (int i = 0; i < N; ++i) {
            char name[32];
            std::snprintf(name, sizeof name, "centrada_%04d.jpg", i);
            const cv::Mat out = cv::imread((outDir / name).string());
            if (out.empty()) {
                fail(__FILE__, __LINE__, "no se leyó el JPG exportado " + std::string(name));
                continue;
            }
            const cv::Point2f c = centroid(out);
            if (std::abs(c.x - out.cols / 2.0f) >= 3.f ||
                std::abs(c.y - out.rows / 2.0f) >= 3.f) {
                char msg[128];
                std::snprintf(msg, sizeof msg, "JPG %d: disco en (%.1f, %.1f), centro (%.1f, %.1f)",
                              i, c.x, c.y, out.cols / 2.0, out.rows / 2.0);
                fail(__FILE__, __LINE__, msg);
            }
        }
    }

    // --- Exportación a MP4 (FHD, 2 fotogramas intermedios): disco en (960,540)
    // en TODOS los frames, incluidos los intermedios.
    {
        const std::string mp4Path = (root / "export.mp4").string();

        PhotoExportWorker::Settings st;
        st.format = PhotoExportWorker::Format::Mp4;
        st.resolution = PhotoExportWorker::Resolution::FHD;
        st.fps = 25.0;
        st.outFile = QString::fromStdString(mp4Path);
        st.interp = 2;
        st.normalizeBrightness = false;

        PhotoExportWorker worker(pathsQt, tracks, 1600, st);
        worker.run();

        FFmpegVideoReader reader;
        if (!reader.open(mp4Path)) {
            fail(__FILE__, __LINE__, "no se pudo abrir el MP4 exportado");
            return 1;
        }
        int frames = 0;
        Frame f;
        while (reader.readNext(f)) {
            if (f.image.empty()) {
                fail(__FILE__, __LINE__, "frame vacío en el MP4");
                continue;
            }
            const cv::Point2f c = centroid(f.image);
            if (std::abs(c.x - f.image.cols / 2.0f) >= 3.f ||
                std::abs(c.y - f.image.rows / 2.0f) >= 3.f) {
                char msg[128];
                std::snprintf(msg, sizeof msg, "MP4 frame %d: disco en (%.1f, %.1f), centro (%.1f, %.1f)",
                              frames, c.x, c.y, f.image.cols / 2.0, f.image.rows / 2.0);
                fail(__FILE__, __LINE__, msg);
            }
            ++frames;
        }
        const int expected = N + (N - 1) * 2; // 10 base + 9*2 intermedios
        if (frames != expected) {
            char msg[64];
            std::snprintf(msg, sizeof msg, "MP4: %d frames, se esperaban %d", frames, expected);
            fail(__FILE__, __LINE__, msg);
        }
    }

    fs::remove_all(root, ec);

    if (g_failures == 0) {
        std::cout << "test_export: OK\n";
        return 0;
    }
    std::cerr << "test_export: " << g_failures << " fallo(s)\n";
    return 1;
}