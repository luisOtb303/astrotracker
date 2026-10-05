#pragma once

#include <QString>
#include <atomic>
#include <functional>
#include <opencv2/core.hpp>

class IVideoReader;

// Mapa de píxeles calientes detectado a partir de varios fotogramas del vídeo.
//
// Un píxel caliente del sensor está SIEMPRE en la misma posición y siempre con
// el mismo valor alto, así que se repite fotograma a fotograma. Una estrella
// real se mueve: por el seeing, por la deriva del trípode y por el propio
// registro. Ese contraste es lo que permite separar los dos casos, cosa que no
// se puede hacer mirando una imagen suelta, donde un punto brillante aislado es
// indistinguible de una estrella de un solo píxel.
//
// Por eso este mapa solo tiene sentido con vídeo. Para fotos se usa el filtro
// espacial de img::apply(), que es más conservador y va apagado por defecto.
//
// Uso típico:
//
//   HotPixelMap::fromVideo(reader, opts) -> mapa
//   HotPixelMap::correct(frame, mapa)   -> fotograma corregido
class HotPixelMap
{
public:
    // Máscara CV_8U con 255 donde hay píxel caliente. Del tamaño del vídeo.
    using Mask = cv::Mat;

    struct Options
    {
        // Fotogramas a muestrear, repartidos por toda la duración. Más
        // fotogramas = más confianza a cambio de más tiempo.
        int sampleCount = 24;
        // Un píxel es caliente si supera el umbral en al menos esta fracción de
        // los fotogramas (0..1). Alto = solo los defectos más persistentes.
        double minHitRatio = 0.7;
        // Píxeles de margen alrededor de cada detección, para que la corrección
        // no deje bordes con halo. 1 basta en la mayoría de los casos.
        int dilate = 1;
        // Umbral del filtro, 0..100 (mismo sentido que el deslizador del panel).
        int sensitivity = 50;
        // Si no es nullptr y está a true, el análisis aborta.
        const std::atomic<bool>* cancel = nullptr;
        // Aviso de avance, opcional. Se llama desde el hilo que analiza, con
        // el número de fotogramas leídos y el total previsto.
        std::function<void(int sampled, int planned)> progress;
    };

    struct Result
    {
        Mask mask;
        int hotCount = 0;   // píxeles detectados antes de dilatar
        int sampled = 0;    // fotogramas realmente leídos
        bool cancelled = false;
        bool ok = false;
        QString error;
    };

    // Analiza el vídeo de `reader` (debe estar abierto). Las muestras se
    // reparten por toda la duración, así que al terminar el lector queda
    // posicionado en el último fotograma leído: quien lo llama debe restaurarlo
    // con seekToUs() + readNext().
    static Result fromVideo(IVideoReader& reader, const Options& opts = {});

    // Sustituye cada píxel marcado por la mediana de sus 8 vecinos. Es la fase
    // de corrección; la de detección es la de arriba.
    static cv::Mat correct(const cv::Mat& frame, const Mask& mask);

    // Fase espacial sobre una imagen suelta (fotos, o vídeo sin mapa): marca los
    // píxeles mucho más brillantes que su entorno y los corrige. Opt-in porque
    // en una imagen aislada no se distingue de una estrella de 1 píxel.
    //
    // `sigma` es el multiplicador del ruido; null = valor por defecto del panel.
    static Mask detectSpatial(const cv::Mat& frame, int sensitivity);
};