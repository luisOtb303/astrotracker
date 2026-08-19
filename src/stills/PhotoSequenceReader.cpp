#include "stills/PhotoSequenceReader.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

bool PhotoSequenceReader::open(const std::vector<std::string>& paths)
{
    paths_.clear();
    width_ = 0;
    height_ = 0;

    for (const auto& p : paths) {
        if (isSupported(p))
            paths_.push_back(p);
    }

    std::sort(paths_.begin(), paths_.end(), [](const std::string& a, const std::string& b) {
        return compareNatural(baseName(a), baseName(b)) < 0;
    });

    if (paths_.empty())
        return false;
    return probeSize();
}

bool PhotoSequenceReader::openFolder(const std::string& dir)
{
    std::error_code ec;
    fs::directory_iterator it(dir, ec);
    if (ec)
        return false;

    std::vector<std::string> files;
    for (const auto& e : it) {
        if (e.is_regular_file())
            files.push_back(e.path().string());
    }
    return open(files);
}

void PhotoSequenceReader::close()
{
    paths_.clear();
    width_ = 0;
    height_ = 0;
}

bool PhotoSequenceReader::readAt(int64_t idx, cv::Mat& out, int maxDim) const
{
    cv::Mat raw;
    if (!readFullRes(idx, raw))
        return false;

    // Normaliza a BGR8 para vista previa/análisis.
    if (raw.depth() != CV_8U) {
        cv::Mat norm;
        cv::normalize(raw, norm, 0, 255, cv::NORM_MINMAX, CV_8U);
        raw = norm;
    }
    switch (raw.channels()) {
    case 1:
        cv::cvtColor(raw, raw, cv::COLOR_GRAY2BGR);
        break;
    case 4:
        cv::cvtColor(raw, raw, cv::COLOR_BGRA2BGR);
        break;
    default:
        break;
    }

    if (maxDim > 0) {
        const int longest = std::max(raw.cols, raw.rows);
        if (longest > maxDim) {
            const double scale = static_cast<double>(maxDim) / longest;
            cv::resize(raw, raw, cv::Size(), scale, scale, cv::INTER_AREA);
        }
    }

    out = raw;
    return true;
}

bool PhotoSequenceReader::readFullRes(int64_t idx, cv::Mat& out) const
{
    if (idx < 0 || idx >= count())
        return false;
    out = cv::imread(paths_[static_cast<size_t>(idx)], cv::IMREAD_UNCHANGED);
    return !out.empty();
}

std::string PhotoSequenceReader::fileName(int64_t idx) const
{
    if (idx < 0 || idx >= count())
        return {};
    return baseName(paths_[static_cast<size_t>(idx)]);
}

std::string PhotoSequenceReader::filePath(int64_t idx) const
{
    if (idx < 0 || idx >= count())
        return {};
    return paths_[static_cast<size_t>(idx)];
}

bool PhotoSequenceReader::probeSize()
{
    cv::Mat first;
    if (!readFullRes(0, first))
        return false;
    width_ = first.cols;
    height_ = first.rows;
    return true;
}

bool PhotoSequenceReader::isSupported(const std::string& path)
{
    static const std::set<std::string> kExt = {
        ".jpg", ".jpeg", ".png", ".tif", ".tiff", ".bmp"
    };
    const fs::path p(path);
    if (p.extension().empty())
        return false;
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return kExt.count(ext) != 0;
}

std::string PhotoSequenceReader::baseName(const std::string& path)
{
    return fs::path(path).filename().string();
}

// Compara dos nombres teniendo en cuenta los números de forma natural.
int PhotoSequenceReader::compareNatural(const std::string& a, const std::string& b)
{
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[j])));

        if (std::isdigit(static_cast<unsigned char>(ca)) &&
            std::isdigit(static_cast<unsigned char>(cb))) {
            size_t ia = i, ib = j;
            while (ia < a.size() && std::isdigit(static_cast<unsigned char>(a[ia])))
                ++ia;
            while (ib < b.size() && std::isdigit(static_cast<unsigned char>(b[ib])))
                ++ib;

            const size_t za = a.find_first_not_of('0', i);
            const std::string na = (za == std::string::npos || za >= ia) ? "0"
                                                                         : a.substr(za, ia - za);
            const size_t zb = b.find_first_not_of('0', j);
            const std::string nb = (zb == std::string::npos || zb >= ib) ? "0"
                                                                         : b.substr(zb, ib - zb);

            int cmp = 0;
            if (na.size() != nb.size())
                cmp = na.size() < nb.size() ? -1 : 1;
            else
                cmp = na.compare(nb);
            if (cmp != 0)
                return cmp;
            i = ia;
            j = ib;
        } else {
            if (ca != cb)
                return ca < cb ? -1 : 1;
            ++i;
            ++j;
        }
    }
    if (i < a.size())
        return 1;
    if (j < b.size())
        return -1;
    return 0;
}