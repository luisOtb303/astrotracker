#include "raw/RawDecoder.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {
int fail(const std::string& what, bool fatal = true)
{
    std::cerr << "FALLO: " << what << "\n";
    return fatal ? 1 : 0;
}

int checkOne(const std::string& path, int expectW, int expectH)
{
    std::cout << "[archivo] " << path << "\n";

    if (!RawDecoder::isRawFile(path))
        return fail("isRawFile(" + path + ")");

    int w = 0, h = 0;
    if (!RawDecoder::dimensions(path, w, h))
        return fail("dimensions(" + path + ")");
    std::cout << "  dimensions: " << w << "x" << h << "\n";
    if (expectW > 0 && (w != expectW || h != expectH))
        return fail("dimensions esperadas " + std::to_string(expectW) + "x" +
                        std::to_string(expectH),
                    true);

    cv::Mat thumb;
    if (!RawDecoder::thumbnail(path, thumb))
        return fail("thumbnail(" + path + ")");
    std::cout << "  thumbnail: " << thumb.cols << "x" << thumb.rows
              << " tipo=" << thumb.type() << "\n";
    if (thumb.empty() || thumb.depth() != CV_8U || thumb.channels() < 3)
        return fail("thumbnail no es BGR");

    cv::Mat full8;
    if (!RawDecoder::decode(path, full8, false, 1024))
        return fail("decode 8-bit(" + path + ")");
    std::cout << "  decode8: " << full8.cols << "x" << full8.rows
              << " tipo=" << full8.type() << "\n";
    if (full8.empty() || full8.depth() != CV_8U || full8.channels() != 3)
        return fail("decode8 no es BGR8");

    cv::Mat full16;
    if (!RawDecoder::decode(path, full16, true, 1024))
        return fail("decode 16-bit(" + path + ")");
    std::cout << "  decode16: " << full16.cols << "x" << full16.rows
              << " tipo=" << full16.type() << "\n";
    if (full16.empty() || full16.depth() != CV_16U || full16.channels() != 3)
        return fail("decode16 no es BGR16");

    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i)
        paths.emplace_back(argv[i]);

    if (paths.empty()) {
        std::cerr << "uso: test_raw <archivo1.raw> [archivo2.raw ...]\n";
        return 2;
    }

    int failures = 0;
    for (const auto& p : paths)
        failures += checkOne(p, 0, 0);

    if (failures) {
        std::cerr << failures << " archivo(s) con fallos\n";
        return 1;
    }
    std::cout << "OK: " << paths.size() << " RAW decodificados\n";
    return 0;
}