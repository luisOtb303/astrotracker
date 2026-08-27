#include "processing/Pipeline.h"

#include "processing/FrameTransformer.h"
#include "tracking/ArcBlobDiscDetector.h"
#include "tracking/DiscArcFit.h"
#include "tracking/DiscTracker.h"
#include "tracking/ITracker.h"
#include "tracking/TemplateTracker.h"
#include "tracking/CentroidTracker.h"
#include "motion/MotionModel.h"
#include "stabilization/Stabilizer.h"
#include "video/FFmpegVideoReader.h"
#include "video/FFmpegVideoWriter.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
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
    if (!reader.open(inPath))
        return offsets;

    // Motor unificado (Disc): ROI opcional como semilla; sin ROI, detección
    // automática del disco en el primer frame. Los motores clásicos exigen ROI.
    const bool useDisc = settings.tracker == TrackerType::Disc;
    const bool roiValid = roi.width >= 4.f && roi.height >= 4.f;
    if (!useDisc && !roiValid)
        return offsets;

    const int64_t total = reader.frameCount();

    Frame frame;
    if (startUs > 0 && !reader.seekToUs(startUs))
        return offsets;
    if (!reader.readNext(frame))
        return offsets;

    std::unique_ptr<ITracker> classicTracker;
    std::unique_ptr<DiscTracker> discTracker;
    cv::Point2f objectCenter;

    if (useDisc) {
        discTracker = std::make_unique<DiscTracker>();
        if (roiValid) {
            discTracker->init({roi.x + roi.width * 0.5f, roi.y + roi.height * 0.5f},
                              0.5f * std::min(roi.width, roi.height));
            objectCenter = rectCenter(roi);
        } else {
            // Siembra automática: estima el radio desde el área del mayor
           // blob (mucho más fiable que un porcentaje del ancho) y afina
            // con el barrido de radio.
            cv::Mat gray;
            cv::cvtColor(frame.image, gray, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(gray, gray, cv::Size(0, 0), 1.2);
            const cv::Rect full(0, 0, gray.cols, gray.rows);
            float guess = std::max(20.f, 0.10f * gray.cols);
            cv::Point2f prior(gray.cols * 0.5f, gray.rows * 0.5f);
            {
                cv::Point2f blobC;
                int area = 0, bw = 0, bh = 0;
                if (DiscArcFit::blobInfo(gray, full, blobC, area, bw, bh) &&
                    area > 100 && bw > 8 && bh > 8) {
                    const float aspect =
                        static_cast<float>(bw) / static_cast<float>(bh);
                    if (aspect > 0.4f && aspect < 2.5f) {
                        guess = std::clamp(
                            std::sqrt(static_cast<float>(area) /
                                      static_cast<float>(CV_PI)),
                            10.f, 0.45f * std::min(gray.cols, gray.rows));
                        prior = blobC;
                    }
                }
            }
            const DiscArcEstimate seed =
                DiscArcFit::fitDisc(gray, full, prior, guess, 3.f);
            if (!seed.ok || seed.radius <= 0.f ||
                cv::norm(seed.center - prior) > 2.0f * guess)
                return offsets; // no se encontró disco creíble
            discTracker->init(seed.center, seed.radius);
            objectCenter = seed.center;
        }
        discTracker->setProfile(trackingProfileFor(settings.profile));
    } else {
        if (settings.tracker == TrackerType::Centroid)
            classicTracker = std::make_unique<CentroidTracker>(settings.searchFactor);
        else
            classicTracker = std::make_unique<TemplateTracker>(settings.searchFactor);
        if (!classicTracker->init(frame.image, roi))
            return offsets;
        objectCenter = rectCenter(roi);
    }

    MotionModel model;
    Stabilizer stabilizer;
    model.reset(rectCenter(roiValid ? roi : cv::Rect2f()));
    stabilizer.reset(cv::Size(frame.image.cols, frame.image.rows), objectCenter);
    stabilizer.setSmoothing(settings.smoothingAlpha);
    // DiscTracker ya suaviza internamente con su propio modelo de movimiento:
    // aplicar el EMA del estabilizador encima doblaría el retraso.
    if (discTracker)
        stabilizer.setSmoothing(1.0f);
    if (settings.target.x >= 0.f && settings.target.y >= 0.f)
        stabilizer.setTarget(TargetPosition{settings.target});

    PipelineStats s;
    int count = 0;
    double confSum = 0.0;

    do {
        float confidence = 0.f;
        int bucket = 0; // 0 válida, 1 incierta, 2 perdida
        cv::Point2f current;
        if (discTracker) {
            const DiscTrack t = discTracker->track(frame.image);
            current = t.center;
            confidence = t.confidence;
            if (!t.predicted && t.status == TrackStatus::VALID)
                bucket = 0;
            else if (t.predicted && t.status == TrackStatus::LOST)
                bucket = 2;
            else
                bucket = 1;
        } else {
            const TrackResult r = classicTracker->track(frame.image);
            current = r.found ? rectCenter(r.rect) : model.position();
            model.update(r.found, current);
            confidence = r.confidence;
            bucket = r.found ? 0
                             : (model.status() == TrackStatus::UNCERTAIN ? 1 : 2);
            current = model.position();
        }

        offsets.push_back(stabilizer.update(current));
        if (bucket == 0)
            ++s.valid;
        else if (bucket == 1)
            ++s.uncertain;
        else
            ++s.lost;
        confSum += confidence;
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