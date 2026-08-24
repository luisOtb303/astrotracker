#pragma once

#include "tracking/TrackingProfile.h"

#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <vector>

// Estado persistible de un proyecto AstroTracker (.atracker): el trabajo del
// modo Fotos (secuencia abierta, semilla, resultados del seguimiento y marcas
// por foto) y/o el del modo Vídeo (ruta, ROI y ajustes del pipeline). Solo
// depende de QtCore para poder testearlo sin GUI.
struct PhotoProjectPhotoResult {
    QString file;               // nombre del archivo (coincidencia por nombre)
    float x = 0.f;              // círculo en píxeles de la imagen de análisis
    float y = 0.f;
    float radius = 0.f;
    int status = 2;             // TrackStatus (0 válida, 1 incierta, 2 perdida)
    bool predicted = false;     // círculo "supuesto" (por predicción)
    bool locked = false;        // bloqueada por el usuario
    bool manualFixed = false;   // corregida a mano
    bool exportSelected = true; // casilla "exportar" del filmstrip
    float confidence = 0.f;     // confianza de la medición (0..1)
    DiscMethod method = DiscMethod::Prediction; // método que la localizó
};

// Override por foto: método principal fijado por el usuario para esa foto
// concreta (Prediction = sin override).
struct PhotoProjectOverride {
    int index = -1;
    DiscMethod method = DiscMethod::Prediction;
};

struct PhotoProjectPhotos {
    bool active = false;
    int originType = 0;         // 0 = carpeta, 1 = lista de archivos
    QString originFolder;
    QStringList originFiles;
    int analysisMaxDim = 1600;  // espacio de coordenadas de los círculos
    int currentIndex = 0;
    ObjectProfile profile = ObjectProfile::Auto; // perfil de seguimiento del proyecto
    std::vector<PhotoProjectOverride> overrides; // método principal por foto
    bool hasSeed = false;
    int seedIndex = 0;
    float seedX = 0.f;
    float seedY = 0.f;
    float seedRadius = 0.f;
    std::vector<PhotoProjectPhotoResult> results;

    // Índice del resultado cuyo archivo coincide con name (-1 si no hay).
    int indexOfResult(const QString& name) const;
};

struct PhotoProjectVideo {
    bool active = false;
    QString path;
    bool hasRoi = false;
    int roiX = 0;
    int roiY = 0;
    int roiW = 0;
    int roiH = 0;
    qint64 startUs = 0;         // inicio del análisis (donde se eligió la ROI)
    qint64 positionUs = 0;      // posición de reproducción al guardar
    int tracker = 0;            // 0 = Template, 1 = Centroid
    double smoothingAlpha = 0.3;
    int borderMode = 0;         // 0 = borde negro, 1 = réplica
};

struct PhotoProject {
    static constexpr int kFormat = 1;

    int format = kFormat;
    QString savedAt;            // fecha/hora ISO al guardar
    PhotoProjectPhotos photos;
    PhotoProjectVideo video;
};

// Serialización JSON (UTF-8, con claves en español para que el archivo sea
// legible). decode rechaza documentos corruptos o escritos por una versión
// posterior del formato.
namespace photo_project {

QByteArray encode(const PhotoProject& project);
bool decode(const QByteArray& json, PhotoProject& out);

} // namespace photo_project
