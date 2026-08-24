#pragma once

#include "tracking/IDiscDetector.h"

// Detector de arco visible / blob brillante: el centro del disco se reconstruye
// del círculo que forma la fase visible (creciente, menguante, eclipse parcial,
// corona) con DiscArcFit::fitFixedRadius, o del centroide si el blob es
// "simétrico" (disco lleno o corona: redondeado, área ~πR², centrado en su
// caja). Busca primero en la ventana y, si se permite, en todo el frame.
class ArcBlobDiscDetector final : public IDiscDetector
{
public:
    explicit ArcBlobDiscDetector(const DiscTrackerParams& params);

    DiscMethod method() const override { return DiscMethod::ArcBlob; }

    std::vector<DiscDetection> detect(const DetectorContext& ctx) override;

    void onConfirmed(const DetectorContext& ctx,
                     const cv::Point2f& confirmedCenter) override;

    void onMissed() override {}

private:
    // Un blob es "simétrico" cuando es redondeado, de área parecida a πR² y
    // con el centroide en el centro de su caja: así el centroide coincide con
    // el centro del disco.
    bool isSymmetricBlob(float radius, int area, int width, int height) const;
};
