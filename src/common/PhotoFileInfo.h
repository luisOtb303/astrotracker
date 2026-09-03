#pragma once

#include <cstdint>
#include <string>

// Datos del fichero e imagen de una foto de la secuencia (modo Fotos): el
// archivo en disco (nombre, ruta, tamaño, fecha de modificación) y las
// dimensiones de píxeles. El EXIF va por separado en PhotoExifInfo.
struct PhotoFileInfo
{
    std::string name;          // "IMG_1234.CR2"
    std::string path;          // ruta absoluta
    std::int64_t sizeBytes = 0;
    std::string type;          // "Canon Raw (CR2)" / "JPEG" ...
    std::string modifyDate;    // fecha de modificación del fichero
    int width = 0;
    int height = 0;

    bool isEmpty() const { return name.empty(); }
};
