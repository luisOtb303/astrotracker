#pragma once

#include "export/ExportJob.h"
#include "processing/Pipeline.h"

#include <QString>
#include <QThread>
#include <QVector>
#include <QPointF>
#include <opencv2/core.hpp>

// Worker en hilo separado para las pasadas de análisis y exportación, de forma
// que la UI no se bloquea. Emite progreso y resultado por señales.
class PipelineWorker : public QThread
{
    Q_OBJECT

public:
    enum class Mode
    {
        None,
        Analyze,
        Export
    };

    struct Request
    {
        Mode mode = Mode::None;
        QString inPath;
        QString outPath;
        cv::Rect2f roi;
        PipelineSettings settings;
        int64_t startUs = 0;
    };

    explicit PipelineWorker(const Request& req, QObject* parent = nullptr)
        : QThread(parent)
        , request_(req)
    {
    }

    void run() override
    {
        if (request_.mode == Mode::Analyze) {
            PipelineStats stats;
            const std::vector<cv::Point2f> offsets = Pipeline::analyze(
                request_.inPath.toStdString(), request_.roi, request_.settings, &stats,
                [this](int done, int total) { emit progress(done, total); },
                request_.startUs);

            QVector<QPointF> qOffsets;
            qOffsets.reserve(static_cast<int>(offsets.size()));
            for (const cv::Point2f& p : offsets)
                qOffsets.push_back(QPointF(p.x, p.y));

            emit analyzeFinished(!offsets.empty(), QString(),
                                 qOffsets,
                                 static_cast<int>(stats.frames),
                                 static_cast<int>(stats.valid),
                                 stats.meanConfidence);
        } else if (request_.mode == Mode::Export) {
            ExportJob job;
            const ExportJob::Result res = job.run(
                request_.inPath.toStdString(), request_.outPath.toStdString(),
                request_.roi, request_.settings,
                [this](int done, int total) { emit progress(done, total); },
                request_.startUs);

            emit exportFinished(res.ok, QString::fromStdString(res.error),
                                static_cast<int>(res.stats.frames),
                                static_cast<int>(res.stats.valid));
        }
    }

signals:
    void progress(int done, int total);
    void analyzeFinished(bool ok, const QString& error, QVector<QPointF> offsets,
                         int frames, int valid, double meanConfidence);
    void exportFinished(bool ok, const QString& error, int frames, int valid);

private:
    Request request_;
};