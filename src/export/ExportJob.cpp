#include "export/ExportJob.h"

ExportJob::Result ExportJob::run(const std::string& inPath, const std::string& outPath,
                                 const cv::Rect2f& roi, const PipelineSettings& settings,
                                 const PipelineProgress& progress, int64_t startUs)
{
    Result res;
    PipelineStats stats;
    if (!pipeline_.run(inPath, outPath, roi, settings, &stats, progress, startUs)) {
        res.error = "no se pudo estabilizar el vídeo";
        return res;
    }
    res.ok = true;
    res.stats = stats;
    return res;
}