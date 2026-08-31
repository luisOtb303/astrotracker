#include "stills/PhotoSequenceReader.h"
#include "stills/ExifReader.h"

#include <exiv2/exiv2.hpp>

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

    // readAt convierte la 16-bit a BGR8 con mapeo fiel (dividir por 257) y
    // mantiene geometría: sin estirado por foto, una imagen oscura sigue oscura.
    cv::Mat img;
    CHECK(reader.readAt(0, img));
    CHECK(!img.empty());
    CHECK(img.type() == CV_8UC3);
    CHECK(img.cols == 30 && img.rows == 20);
    // Valor máximo de deep16 = 29*1000 = 29000 → 29000/257 ≈ 112 (antes el
    // estirado min-max lo llevaba a 255).
    const cv::Vec3b px = img.at<cv::Vec3b>(10, 29);
    CHECK(px[0] >= 100 && px[0] <= 125 && px[0] == px[1] && px[1] == px[2]);

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

    // EXIF real: se escribe metadatos con exiv2 en un JPEG y se leen con
    // ExifReader (el mismo camino que usa PhotoSequenceReader::exifInfo).
    {
        using namespace Exiv2;
        const fs::path exifJpg = dir / "exif_foto.jpg";
        cv::imwrite(exifJpg.string(), makeFrame(320, 240, 200));

        auto img = ImageFactory::open(exifJpg.string());
        ExifData& ed = img->exifData();
        ed["Exif.Image.Make"] = "AstroCam";
        ed["Exif.Image.Model"] = "Test MK-1";
        ed["Exif.Photo.LensModel"] = "RF 800mm f/11";
        ed["Exif.Photo.FocalLength"] = Rational(800, 1);
        ed["Exif.Photo.FNumber"] = Rational(11, 1);
        ed["Exif.Photo.ExposureTime"] = Rational(1, 500);
        ed["Exif.Photo.ISOSpeedRatings"] = 3200;
        ed["Exif.Photo.DateTimeOriginal"] = "2026:03:15 22:47:00";
        img->setExifData(ed);
        img->writeMetadata();

        const PhotoExifInfo ex = ExifReader::readExif(exifJpg.string());
        CHECK(ex.camera == "AstroCam Test MK-1");
        CHECK(ex.lens == "RF 800mm f/11");
        CHECK(ex.hasFocal() && static_cast<int>(ex.focalMm + 0.5f) == 800);
        CHECK(ex.hasAperture() && static_cast<int>(ex.aperture + 0.5f) == 11);
        CHECK(ex.shutterSec > 0.0018f && ex.shutterSec < 0.0022f);
        CHECK(ex.iso == 3200);
        CHECK(!ex.date.empty());

        // Una imagen sin metadatos no debe reportar datos fantasma.
        CHECK(ExifReader::readExif((dir / "extra.bmp").string()).camera.empty());
    }

    if (g_failures == 0) {
        std::cout << "test_stills: OK\n";
        return 0;
    }
    std::cerr << "test_stills: " << g_failures << " fallo(s)\n";
    return 1;
}