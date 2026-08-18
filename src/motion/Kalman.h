#pragma once

#include <opencv2/core.hpp>

// Filtro de Kalman lineal de velocidad constante para el seguimiento de un
// objeto puntual en 2D. Estado: (x, y, vx, vy) en píxeles y píxeles/frame
// (paso temporal unitario). Medición: (x, y) del centro del objeto.
class Kalman
{
public:
    Kalman();

    // Reinicia el estado en la posición dada con velocidad nula.
    void reset(float x, float y);

    // Avanza el estado una predicción (sin medición).
    void predict();

    // Incorpora una medición (centro del objeto) y predice el siguiente paso.
    void correct(float mx, float my);

    float x() const { return s_(0); }
    float y() const { return s_(1); }
    float vx() const { return s_(2); }
    float vy() const { return s_(3); }

private:
    cv::Matx44f F_;
    cv::Matx44f Q_;
    cv::Matx<float,2,4> H_;
    cv::Matx22f R_;
    cv::Matx44f P_;
    cv::Matx44f I_;
    cv::Vec4f s_ = cv::Vec4f::zeros();
};