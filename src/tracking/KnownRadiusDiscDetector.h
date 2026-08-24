#pragma once

#include "tracking/DiscTrackerParams.h"
#include "tracking/IDiscDetector.h"

// Detector de radio conocido (estrategia de los alineadores de eclipse): el
// radio del disco se mide una vez en fase completa y se mantiene durante todo
// el evento. Busca el círculo de radio R sobre el contorno del mayor blob
// brillante aceptando arcos más cortos que el detector de arco general: con
// R conocido, un arco corto ya fija el centro (parciales profundos, anillo
// de la totalidad). Opcionalmente busca en todo el frame.
class KnownRadiusDiscDetector final : public IDiscDetector
{
public:
    explicit KnownRadiusDiscDetector(const DiscTrackerParams& params);

    DiscMethod method() const override { return DiscMethod::KnownRadius; }

    std::vector<DiscDetection> detect(const DetectorContext& ctx) override;

    void onConfirmed(const DetectorContext& ctx,
                     const cv::Point2f& confirmedCenter) override;

    void onMissed() override;
};
