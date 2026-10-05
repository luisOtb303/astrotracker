#pragma once

#include "common/ImageAdjust.h"

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include <opencv2/core.hpp>

// Aplica los ajustes de imagen fuera del hilo de la UI.
//
// El motivo es el denoise: fastNlMeansDenoising puede tardar cientos de ms en
// una imagen grande, y hacerlo en el hilo principal congela la ventana
// (incluido el propio deslizador que lo dispara). Aqui el frame crudo se
// guarda una vez y cada cambio de parametro solo reencadena el trabajo.
//
// Al arrastrar un deslizador llegan peticiones muchisimas. Se coalescen: si
// llega una nueva mientras el hilo procesa la anterior, al terminar se
// procesa solo la ultima. El contador de peticiones es monotonico (no el
// valor del ajuste) para que pedir dos veces el mismo valor tambien se
// procese, igual que en PhotoFrameLoader.
class AdjustWorker : public QThread
{
    Q_OBJECT

public:
    explicit AdjustWorker(QObject* parent = nullptr);

    // Fija el frame crudo de referencia. Se guarda una copia para que el
    // llamante pueda reutilizar su cv::Mat.
    void setSource(const cv::Mat& src);

    // Mapa de píxeles calientes confirmado fotograma a fotograma (HotPixelMap).
    // Se usa en lugar de la detección espacial cuando está disponible. Vacío =
    // solo detección espacial.
    //
    // No dispara el reprocesado por sí mismo: quien cambia el mapa llama a
    // request() con los ajustes vigentes, que es lo que invalida el resultado en
    // vuelo de la petición anterior.
    void setHotMask(const cv::Mat& mask);

    // Pide reprocesar con estos ajustes. Devuelve false si el ajuste es
    // identidad y no hay nada que hacer.
    //
    // El número de petición es del worker, no del llamante: para descartar
    // resultados obsoletos hay que compararlo con currentSeq(), nunca con un
    // contador propio. Dos contadores se desincronizan en cuanto hay una
    // petición identidad (que no avanza el del worker) y a partir de ahi se
    // descarta todo en silencio.
    bool request(const ImageAdjust& adj);

    // Descarta ajustes pendientes (cambiar de foto, valores nuevos). El hilo
    // sigue vivo para la siguiente petición.
    void reset();

    // Número de la última petición. Es el seq que Bringe adjusted(), así que
    // un resultado con otro seq ya no corresponde a lo vigente.
    //
    // request() y reset() lo avanzan SIEMPRE, también para el ajuste identidad:
    // es lo que invalida un resultado que estuviera en vuelo.
    quint64 currentSeq();

    // Detiene el hilo y espera a que termine. Obligatorio antes de destruirlo:
    // un QThread en ejecución no se puede destruir.
    void shutdown();

    // Espera a que el hilo termine el trabajo pendiente. Solo para tests.
    bool waitIdle(int msecs = 5000);

signals:
    // Emitido con el frame ya ajustado y el número de petición al que
    // corresponde, para descartar resultados que ya no son los vigentes.
    void adjusted(const cv::Mat& out, quint64 seq);

protected:
    void run() override;

private:
    QMutex mutex_;
    QWaitCondition cond_;

    cv::Mat source_;
    cv::Mat hotMask_;
    ImageAdjust requested_;
    bool hasRequested_ = false;
    quint64 requestSeq_ = 0;

    quint64 appliedSeq_ = 0;
    std::atomic<bool> stop_{false};
    std::atomic<bool> busy_{false};
};