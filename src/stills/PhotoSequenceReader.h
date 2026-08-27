#pragma once

#include "common/PhotoExifInfo.h"
#include <opencv2/core.hpp>
#include <mutex>
#include <string>
#include <vector>

// Lee una secuencia de fotos estáticas (JPG/PNG/TIFF/BMP y RAW CR2/CR3 vía
// LibRaw). El orden de trabajo es el de los nombres de archivo ordenados
// naturalmente (IMG_2, IMG_10 en lugar de IMG_10, IMG_2).
// Cada foto se decodifica bajo demanda; no se retiene nada en memoria.
// Los RAW usan además una caché de análisis en disco (_astrotracker_cache/
// junto a las fotos): la primera lectura genera un JPG pequeño y las demás
// salen de él (navegación, miniaturas y cálculo mucho más rápidos).
class PhotoSequenceReader
{
public:
    PhotoSequenceReader() = default;
    ~PhotoSequenceReader() = default;

    // Abre una lista de rutas, descartando las que no sean imágenes soportadas.
    bool open(const std::vector<std::string>& paths);
    // Abre todos los archivos de imagen de un directorio (orden natural).
    bool openFolder(const std::string& dir);
    void close();
    bool isOpen() const { return !paths_.empty(); }

    int64_t count() const { return static_cast<int64_t>(paths_.size()); }

    // Imagen de trabajo (BGR8). Si maxDim > 0 se redimensiona para que el lado
    // mayor quepa en maxDim (vista previa / análisis).
    bool readAt(int64_t idx, cv::Mat& out, int maxDim = 0) const;

    // Decodificación sin normalizar (cualquier profundidad/canales), para
    // exportación a máxima calidad. Los RAW se devuelven a 16 bits.
    bool readFullRes(int64_t idx, cv::Mat& out) const;

    // Miniatura rápida para filmstrips (RAW: miniatura embebida del archivo).
    bool thumbnail(int64_t idx, cv::Mat& out, int maxDim = 0) const;

    std::string fileName(int64_t idx) const;
    std::string filePath(int64_t idx) const;
    int width() const { return width_; }
    int height() const { return height_; }

    // EXIF metadata for a specific photo (cached).
    PhotoExifInfo exifInfo(int64_t idx) const;

private:
    static bool isSupported(const std::string& path);
    static bool isRawExt(const std::string& path);
    static std::string baseName(const std::string& path);
    static int compareNatural(const std::string& a, const std::string& b);
    bool probeSize();

    void initAnalysisCache();
    std::string cacheEntryPath(const std::string& srcPath, int maxDim) const;
    bool loadCachedAnalysis(const std::string& srcPath, int maxDim, cv::Mat& out) const;
    void storeCachedAnalysis(const std::string& srcPath, int maxDim,
                             const cv::Mat& img) const;

    std::vector<std::string> paths_;
    int width_ = 0;
    int height_ = 0;
    // Caché de análisis de RAW (vacío = desactivada).
    std::string analysisCacheDir_;
    mutable std::mutex cacheMutex_;
};