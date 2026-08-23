#include "stills/PhotoSequenceReader.h"

#include "raw/RawDecoder.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <vector>

namespace fs = std::filesystem;

// Conversión fiel a BGR8: los 16 bits se dividen por 257 (65535 → 255), sin
// estirar el histograma. Determinista entre fotos: una foto oscura se ve
// oscura tal cual es, sin amplificar ruido ni alterar el brillo relativo de la
// secuencia. Es el mismo mapeo que usa el exportador.
void toBgr8Faithful(cv::Mat& m)
{
    if (!m.empty() && m.depth() == CV_16U) {
        cv::Mat eight;
        m.convertTo(eight, CV_8U, 1.0 / 257.0);
        m = eight;
    }
    switch (m.empty() ? 0 : m.channels()) {
    case 1:
        cv::cvtColor(m, m, cv::COLOR_GRAY2BGR);
        break;
    case 4:
        cv::cvtColor(m, m, cv::COLOR_BGRA2BGR);
        break;
    default:
        break;
    }
}

bool PhotoSequenceReader::open(const std::vector<std::string>& paths)
{
    paths_.clear();
    width_ = 0;
    height_ = 0;

    for (const auto& p : paths) {
        if (!isSupported(p))
            continue;
        if (isRawExt(p)) {
            // Valida el RAW sin decodificarlo (solo cabecera).
            if (RawDecoder::isRawFile(p))
                paths_.push_back(p);
        } else {
            paths_.push_back(p);
        }
    }

    std::sort(paths_.begin(), paths_.end(), [](const std::string& a, const std::string& b) {
        return compareNatural(baseName(a), baseName(b)) < 0;
    });

    if (paths_.empty())
        return false;
    initAnalysisCache();
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
    analysisCacheDir_.clear();
}

bool PhotoSequenceReader::readAt(int64_t idx, cv::Mat& out, int maxDim) const
{
    if (idx < 0 || idx >= count())
        return false;
    const std::string& path = paths_[static_cast<size_t>(idx)];

    // RAW: decodificación media (rápida) y caché de análisis en disco; la
    // primera lectura genera el JPG pequeño y las siguientes salen de él.
    if (isRawExt(path)) {
        if (maxDim > 0 && loadCachedAnalysis(path, maxDim, out))
            return true;
        cv::Mat m;
        if (!RawDecoder::decode(path, m, true, maxDim))
            return false;
        toBgr8Faithful(m);
        if (maxDim > 0)
            storeCachedAnalysis(path, maxDim, m);
        out = m;
        return true;
    }

    out = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (out.empty())
        return false;
    toBgr8Faithful(out);

    if (maxDim > 0) {
        const int longest = std::max(out.cols, out.rows);
        if (longest > maxDim) {
            const double scale = static_cast<double>(maxDim) / longest;
            cv::resize(out, out, cv::Size(), scale, scale, cv::INTER_AREA);
        }
    }
    return true;
}

bool PhotoSequenceReader::readFullRes(int64_t idx, cv::Mat& out) const
{
    if (idx < 0 || idx >= count())
        return false;
    const std::string& path = paths_[static_cast<size_t>(idx)];
    if (isRawExt(path))
        return RawDecoder::decode(path, out, true);
    out = cv::imread(path, cv::IMREAD_UNCHANGED);
    return !out.empty();
}

bool PhotoSequenceReader::thumbnail(int64_t idx, cv::Mat& out, int maxDim) const
{
    if (idx < 0 || idx >= count())
        return false;
    const std::string& path = paths_[static_cast<size_t>(idx)];
    if (isRawExt(path))
        return RawDecoder::thumbnail(path, out, maxDim);
    return readAt(idx, out, maxDim);
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
    if (paths_.empty())
        return false;
    const std::string& first = paths_.front();
    if (isRawExt(first))
        return RawDecoder::dimensions(first, width_, height_);
    cv::Mat firstImg;
    if (!readFullRes(0, firstImg))
        return false;
    width_ = firstImg.cols;
    height_ = firstImg.rows;
    return true;
}

void PhotoSequenceReader::initAnalysisCache()
{
    analysisCacheDir_.clear();
    if (paths_.empty())
        return;
    const fs::path parent = fs::path(paths_.front()).parent_path();
    if (parent.empty())
        return;
    std::error_code ec;
    const fs::path dir = parent / "_astrotracker_cache";
    fs::create_directories(dir, ec);
    if (!ec)
        analysisCacheDir_ = dir.string();
}

// Nombre de entrada de caché auto-invalidable: incluye mtime y tamaño del
// original; si la foto cambia, cambia el nombre y la entrada vieja se olvida.
// El prefijo de versión invalida en bloque las entradas generadas con un
// procesado RAW distinto (p. ej. antes de desactivar el auto-brillo).
std::string PhotoSequenceReader::cacheEntryPath(const std::string& srcPath,
                                                int maxDim) const
{
    const fs::path p(srcPath);
    std::error_code ec;
    const auto t = fs::last_write_time(p, ec);
    const unsigned long long mtime =
        ec ? 0ULL
           : static_cast<unsigned long long>(t.time_since_epoch().count());
    const unsigned long long size =
        ec ? 0ULL : static_cast<unsigned long long>(fs::file_size(p, ec));
    std::string ext = p.extension().string();
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    fs::path entry =
        fs::path(analysisCacheDir_) /
        ("v2." + p.stem().string() + ext + "." + std::to_string(mtime) +
         "." + std::to_string(size) + "." + std::to_string(maxDim) + ".jpg");
    return entry.string();
}

bool PhotoSequenceReader::loadCachedAnalysis(const std::string& srcPath,
                                             int maxDim, cv::Mat& out) const
{
    if (analysisCacheDir_.empty())
        return false;
    cv::Mat img = cv::imread(cacheEntryPath(srcPath, maxDim), cv::IMREAD_COLOR);
    if (img.empty())
        return false;
    out = img;
    return true;
}

void PhotoSequenceReader::storeCachedAnalysis(const std::string& srcPath,
                                              int maxDim, const cv::Mat& img) const
{
    if (analysisCacheDir_.empty() || img.empty())
        return;
    try {
        // Codificación a memoria y volcado directo a la entrada final:
        // cv::imwrite elige el codificador por la extensión del nombre, así
        // que un temporal ".tmp" fallaba siempre en silencio.
        std::vector<unsigned char> encoded;
        const std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, 92};
        if (!cv::imencode(".jpg", img, encoded, params))
            return;
        std::lock_guard<std::mutex> lock(cacheMutex_);
        const std::string entry = cacheEntryPath(srcPath, maxDim);
        std::ofstream file(entry, std::ios::binary | std::ios::trunc);
        if (!file)
            return;
        file.write(reinterpret_cast<const char*>(encoded.data()),
                   static_cast<std::streamsize>(encoded.size()));
    } catch (const std::exception&) {
        // La caché es una optimización: cualquier otro fallo se ignora.
    }
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
    return kExt.count(ext) != 0 || isRawExt(path);
}

bool PhotoSequenceReader::isRawExt(const std::string& path)
{
    static const std::set<std::string> kRawExt = {
        ".cr2", ".cr3", ".dng", ".nef", ".arw", ".orf", ".raf", ".rw2", ".pef", ".srw", ".raw"
    };
    const fs::path p(path);
    if (p.extension().empty())
        return false;
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return kRawExt.count(ext) != 0;
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