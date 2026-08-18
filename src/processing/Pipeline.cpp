#include "processing/Pipeline.h"

#include "processing/FrameTransformer.h"
#include "tracking/TemplateTracker.h"
#include "motion/MotionModel.h"
#include "stabilization/Stabilizer.h"
#include "video/FFmpegVideoReader.h"
#include "video/FFmpegVideoWriter.h"

#include <cmath>

namespace {

cv::Point2f rectCenter(const cv::Rect2f& r)
{
    return {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
}

} // namespace

std::vector<cv::Point2f> Pipeline::analyze(const std::string& inPath,
                                           const cv::Rect2f& roi,
                                           PipelineStats* stats)
{
    std::vector<cv::Point2f> offsets;

    FFmpegVideoReader reader;
    if (!reader.open(inPath) || roi.width < 4.f || roi.height < 4.f)
        return offsets;

    Frame frame;
    if (!reader.readNext(frame))
        return offsets;

    TemplateTracker tracker;
    if (!tracker.init(frame.image, roi))
        return offsets;

    MotionModel model;
    Stabilizer stabilizer;
    model.reset(rectCenter(roi));
    stabilizer.reset(cv::Size(frame.image.cols, frame.image.rows),
                     rectCenter(roi));

    PipelineStats s;
    int count = 0;
    double confSum = 0.0;

    do {
        const TrackResult r = tracker.track(frame.image);
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
    } while (reader.readNext(frame));

    s.frames = count;
    s.meanConfidence = count ? confSum / count : 0.0;

    if (stats)
        *stats = s;
    return offsets;
}

bool Pipeline::run(const std::string& inPath, const std::string& outPath,
                   const cv::Rect2f& roi, BorderMode borderMode,
                   PipelineStats* stats) const
{
    PipelineStats s;
    const std::vector<cv::Point2f> offsets = analyze(inPath, roi, &s);
    if (offsets.empty())
        return false;

    FFmpegVideoReader reader;
    if (!reader.open(inPath))
        return false;

    FFmpegVideoWriter writer;
    if (!writer.open(outPath, reader.width(), reader.height(), reader.fps()))
        return false;

    FrameTransformer transformer(borderMode);

    Frame frame;
    size_t i = 0;
    while (reader.readNext(frame)) {
        const cv::Point2f off = (i < offsets.size()) ? offsets[i] : cv::Point2f();
        writer.write(transformer.transform(frame.image, off));
        ++i;
    }
    writer.close();

    if (stats)
        *stats = s;
    return true;
}