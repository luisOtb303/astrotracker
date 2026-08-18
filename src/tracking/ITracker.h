#pragma once

#include <opencv2/core.hpp>

// Resultado del seguimiento de un frame.
struct TrackResult
{
    cv::Rect2f rect;     // posición del objeto en píxeles de imagen
    float confidence = 0.f; // 0..1
    bool found = false;  // false si el tracker no pudo confirmar la posición
};

// Interfaz de trackers de objeto. El tracker trabaja sobre frames BGR8 y
// devuelve la posición del objeto; el pipeline decide si descartar la medición
// según la confianza y el estado del MotionModel.
class ITracker
{
public:
    virtual ~ITracker() = default;

    // Inicializa el tracker con el frame y la ROI seleccionada.
    virtual bool init(const cv::Mat& frame, const cv::Rect2f& roi) = 0;

    // Seguimiento del objeto en el frame actual.
    virtual TrackResult track(const cv::Mat& frame) = 0;
};