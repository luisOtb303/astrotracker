#include "stills/PhotoSequenceReader.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_failures = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ++g_failures;                                                            \
            std::cerr << "FALLO en linea " << __LINE__ << ": " #cond << "\n";        \
        }                                                                            \
    } while (0)

static cv::Mat makeFrame(int w, int h, uchar value)
{
    return cv::Mat(h, w, CV_8UC3, cv::Scalar(value, value, value));
}

int main()
{
#ifdef WIN32
    const fs::path dir = fs::temp_directory_path() / "astrotracker_test_stills";
#else
    const fs::path dir = fs::current_path() / "test_stills_data";
#endif
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);

    cv::imwrite((dir / "extra.bmp").string(), makeFrame(80, 40, 90));
    cv::imwrite((dir / "foto.tif").string(), makeFrame(64, 48, 150));
    cv::imwrite((dir / "IMG_0001.png").string(), makeFrame(100, 50, 30));
    cv::imwrite((dir / "IMG_0002.png").string(), makeFrame(100, 50, 60));
    cv::imwrite((dir / "IMG_0010.png").string(), makeFrame(100, 50, 120));

    cv::Mat deep(20, 30, CV_16UC1, cv::Scalar(0));
    for (int y = 0; y < deep.rows; ++y)
        for (int x = 0; x < deep.cols; ++x)
            deep.at<ushort>(y, x) = static_cast<ushort>(x * 1000);
    cv::imwrite((dir / "deep16.tif").string(), deep);

    std::ofstream((dir / "nota.txt").string()) << "no es una imagen\n";
    std::ofstream((dir / "escondido.cr2").string()) << "cr2 de momento ignorado\n";

    // openFolder: descarta el .txt y el .cr2; orden natural.
    PhotoSequenceReader reader;
    CHECK(reader.openFolder(dir.string()));
    CHECK(reader.isOpen());
    CHECK(reader.count() == 6);

    // deep16.tif < extra.bmp < foto.tif < IMG_0001 < IMG_0002 < IMG_0010
    CHECK(reader.fileName(0) == "deep16.tif");
    CHECK(reader.fileName(1) == "extra.bmp");
    CHECK(reader.fileName(2) == "foto.tif");
    CHECK(reader.fileName(3) == "IMG_0001.png");
    CHECK(reader.fileName(4) == "IMG_0002.png");
    CHECK(reader.fileName(5) == "IMG_0010.png");

    CHECK(reader.width() == 30);
    CHECK(reader.height() == 20);

    // readAt normaliza la 16-bit a BGR8 y mantiene geometría.
    cv::Mat img;
    CHECK(reader.readAt(0, img));
    CHECK(!img.empty());
    CHECK(img.type() == CV_8UC3);
    CHECK(img.cols == 30 && img.rows == 20);

    // Redimensión por maxDim.
    CHECK(reader.readAt(3, img, 50));
    CHECK(!img.empty());
    CHECK(std::max(img.cols, img.rows) <= 50);
    CHECK(img.cols == 50 && img.rows == 25);

    // Sin maxDim: geometría original.
    CHECK(reader.readAt(3, img));
    CHECK(img.cols == 100 && img.rows == 50);

    // readFullRes conserva la profundidad original.
    cv::Mat full;
    CHECK(reader.readFullRes(0, full));
    CHECK(!full.empty());
    CHECK(full.depth() == CV_16U);

    CHECK(!reader.readAt(99, img));
    CHECK(reader.fileName(99).empty());

    // open() con lista mixta: solo cuenta las imágenes válidas.
    std::vector<std::string> list = {
        (dir / "IMG_0001.png").string(),
        (dir / "nota.txt").string(),
        (dir / "IMG_0010.png").string(),
    };
    PhotoSequenceReader r2;
    CHECK(r2.open(list));
    CHECK(r2.count() == 2);
    CHECK(r2.fileName(0) == "IMG_0001.png");
    CHECK(r2.fileName(1) == "IMG_0010.png");

    PhotoSequenceReader r3;
    CHECK(!r3.openFolder((fs::temp_directory_path() / "no_existe_astrotracker_xyz").string()));

    if (g_failures == 0) {
        std::cout << "test_stills: OK\n";
        return 0;
    }
    std::cerr << "test_stills: " << g_failures << " fallo(s)\n";
    return 1;
}