#pragma once

#include "processing/Pipeline.h"

#include <string>

// Trabajo de exportación: ejecuta el pipeline de estabilización de forma
// síncrona (bloqueante). La UI lo lanzará en un worker thread.
class ExportJob
{
public:
    struct Result
    {
        bool ok = false;
        PipelineStats stats;
        std::string error;
    };

    Result run(const std::string& inPath, const std::string& outPath,
               const cv::Rect2f& roi,
               const PipelineSettings& settings = PipelineSettings(),
               const PipelineProgress& progress = PipelineProgress(),
               int64_t startUs = 0,
               const PipelineCancel& cancel = PipelineCancel());

private:
    Pipeline pipeline_;
};