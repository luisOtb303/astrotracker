#pragma once

#include "processing/HotPixelMap.h"

#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>

// Analiza un vídeo en busca de píxeles calientes del sensor.
//
// Va en un hilo propio porque son varias decenas de fotogramas con
// medianBlur: leerlos por el hilo de la UI congelaría la ventana durante
// segundos. Usa un FFmpegVideoReader propio, no el del visor, para no
// descolocar la posición de reproducción mientras trabaja.
class HotPixelScanWorker : public QThread
{
    Q_OBJECT

public:
    explicit HotPixelScanWorker(QObject* parent = nullptr);
    ~HotPixelScanWorker() override;

    // `path` = fichero de vídeo. Devuelve false si ya hay un análisis en curso.
    bool startScan(const QString& path, const HotPixelMap::Options& opts);

    // Pide abortar. No termina el análisis al instante: comprueba entre
    // fotogramas, así que tarda lo que un fotograma.
    void cancel();

    // Detiene el hilo y espera. Obligatorio antes de destruirlo.
    void shutdown();

signals:
    void progress(int sampled, int planned);
    void finished(const HotPixelMap::Result& result);

protected:
    void run() override;

private:
    QString path_;
    HotPixelMap::Options opts_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> running_{false};
};