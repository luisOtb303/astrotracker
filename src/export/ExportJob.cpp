#include "export/ExportJob.h"

ExportJob::Result ExportJob::run(const std::string& inPath, const std::string& outPath,
                                 const cv::Rect2f& roi, BorderMode borderMode)
{
    Result res;
    PipelineStats stats;
    if (!pipeline_.run(inPath, outPath, roi, borderMode, &stats)) {
        res.error = "no se pudo estabilizar el vídeo";
        return res;
    }
    res.ok = true;
    res.stats = stats;
    return res;
}