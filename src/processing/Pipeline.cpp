#include "processing/Pipeline.h"

#include "processing/FrameTransformer.h"
#include "tracking/ITracker.h"
#include "tracking/TemplateTracker.h"
#include "tracking/CentroidTracker.h"
#include "motion/MotionModel.h"
#include "stabilization/Stabilizer.h"
#include "video/FFmpegVideoReader.h"
#include "video/FFmpegVideoWriter.h"

#include <cmath>
#include <memory>

namespace {

cv::Point2f rectCenter(const cv::Rect2f& r)
{
    return {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
}

} // namespace

std::vector<cv::Point2f> Pipeline::analyze(const std::string& inPath,
                                           const cv::Rect2f& roi,
                                           const PipelineSettings& settings,
                                           PipelineStats* stats,
                                           const PipelineProgress& progress,
                                           int64_t startUs)
{
    std::vector<cv::Point2f> offsets;

    FFmpegVideoReader reader;
    if (!reader.open(inPath) || roi.width < 4.f || roi.height < 4.f)
        return offsets;

    const int64_t total = reader.frameCount();

    Frame frame;
    if (startUs > 0 && !reader.seekToUs(startUs))
        return offsets;
    if (!reader.readNext(frame))
        return offsets;

    std::unique_ptr<ITracker> tracker;
    if (settings.tracker == TrackerType::Centroid)
        tracker = std::make_unique<CentroidTracker>(settings.searchFactor);
    else
        tracker = std::make_unique<TemplateTracker>(settings.searchFactor);
    if (!tracker->init(frame.image, roi))
        return offsets;

    MotionModel model;
    Stabilizer stabilizer;
    model.reset(rectCenter(roi));
    stabilizer.reset(cv::Size(frame.image.cols, frame.image.rows), rectCenter(roi));
    stabilizer.setSmoothing(settings.smoothingAlpha);
    if (settings.target.x >= 0.f && settings.target.y >= 0.f)
        stabilizer.setTarget(TargetPosition{settings.target});

    PipelineStats s;
    int count = 0;
    double confSum = 0.0;

    do {
        const TrackResult r = tracker->track(frame.image);
        const cv::Point2f center = r.found ? rectCenter(r.rect) : model.position();
        model.update(r.found, center);
        offsets.push_back(stabilizer.update(model.position()));

        if (r.found)
            ++s.valid;
        else if (model.status() == TrackStatus::UNCERTAIN)
            ++s.uncertain;
        else
            ++s.lost;
        confSum += r.confidence;
        ++count;
        if (progress)
            progress(count, static_cast<int>(total > 0 ? total : count));
    } while (reader.readNext(frame));

    s.frames = count;
    s.meanConfidence = count ? confSum / count : 0.0;

    if (stats)
        *stats = s;
    return offsets;
}

bool Pipeline::run(const std::string& inPath, const std::string& outPath,
                   const cv::Rect2f& roi, const PipelineSettings& settings,
                   PipelineStats* stats, const PipelineProgress& progress,
                   int64_t startUs) const
{
    PipelineStats s;
    const std::vector<cv::Point2f> offsets = analyze(inPath, roi, settings, &s, progress, startUs);
    if (offsets.empty())
        return false;

    FFmpegVideoReader reader;
    if (!reader.open(inPath))
        return false;

    FFmpegVideoWriter writer;
    if (!writer.open(outPath, reader.width(), reader.height(), reader.fps()))
        return false;

    const int64_t startIndex = (startUs > 0)
                                   ? static_cast<int64_t>(std::llround(startUs * reader.fps() / 1e6))
                                   : 0;

    FrameTransformer transformer(settings.borderMode);

    Frame frame;
    int64_t k = 0;
    const int64_t n = static_cast<int64_t>(offsets.size());
    while (reader.readNext(frame)) {
        const int64_t oi = k - startIndex;
        const cv::Point2f off = (oi >= 0 && oi < n) ? offsets[oi] : cv::Point2f();
        writer.write(transformer.transform(frame.image, off));
        ++k;
    }
    writer.close();

    if (stats)
        *stats = s;
    return true;
}