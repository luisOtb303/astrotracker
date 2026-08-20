#pragma once

#include "stills/PhotoSequenceReader.h"
#include "tracking/DiscTracker.h"

#include <QString>
#include <QStringList>
#include <QThread>
#include <atomic>
#include <vector>

// Worker en hilo separado que exporta las fotos ya centradas (resultado del
// seguimiento) como imágenes JPG/PNG o como vídeo MP4 (H.264). Re-lee cada foto
// bajo demanda, aplica el mismo centrado del visor y escribe el resultado. La
// UI no se bloquea.
class PhotoExportWorker : public QThread
{
    Q_OBJECT

public:
    enum class Format { Jpg, Png, Mp4 };
    enum class Resolution { Original, Visor, HD, FHD, QHD, UHD };

    struct Settings
    {
        Format format = Format::Jpg;
        Resolution resolution = Resolution::Visor;
        double fps = 10.0;
        int borderMode = 0; // 0 = negro, 1 = réplica
        QString outDir;     // imágenes
        QString outFile;    // mp4
    };

    // `selection` vacío = exportar todas; si no, solo las fotos con `true`.
    PhotoExportWorker(const QStringList& paths, const std::vector<DiscTrack>& tracks,
                      int analysisDim, const Settings& settings,
                      const std::vector<bool>& selection = {}, QObject* parent = nullptr)
        : QThread(parent)
        , paths_(paths)
        , tracks_(tracks)
        , analysisDim_(analysisDim)
        , settings_(settings)
        , selection_(selection)
    {
    }

    void requestStop() { stop_.store(true); }

    void run() override;

signals:
    void progress(int done, int total);
    void finished(bool ok, const QString& error, int frames);
    // La foto `index` acaba de procesarse (para mostrar el nombre en la UI).
    void photoProcessed(int64_t index);

private:
    QStringList paths_;
    std::vector<DiscTrack> tracks_;
    int analysisDim_ = 0;
    Settings settings_;
    std::vector<bool> selection_;
    std::atomic<bool> stop_{false};
};