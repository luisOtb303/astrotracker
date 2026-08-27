#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QString>
#include "common/PhotoExifInfo.h"

// Right-side info panel showing tracking statistics, frame info, and EXIF data.
class InfoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit InfoPanel(QWidget* parent = nullptr);

    // Tracking stats
    void setTrackingStats(int valid, int predicted, int lost);

    // Frame info
    void setFrameInfo(int frameIndex, int totalFrames,
                      const QString& time,
                      const QString& method,
                      float confidence,
                      float offsetX, float offsetY,
                      const QString& status);

    // EXIF info (photos mode)
    void setExifInfo(const PhotoExifInfo& exif);
    void clearExifInfo();

    // Material info
    void setMaterialInfo(const QString& info);

    void setTrackingVisible(bool visible);
    void setFrameInfoVisible(bool visible);
    void setExifVisible(bool visible);

signals:
    void toggleVisibility();

private:
    QWidget* trackingSection_;
    QWidget* frameSection_;
    QWidget* exifSection_;

    QLabel* validCount_;
    QLabel* predictedCount_;
    QLabel* lostCount_;
    QLabel* trackingPercent_;

    QLabel* frameIndexLabel_;
    QLabel* timeLabel_;
    QLabel* methodLabel_;
    QLabel* confidenceLabel_;
    QLabel* offsetLabel_;
    QLabel* statusLabel_;

    QLabel* exifCamera_;
    QLabel* exifLens_;
    QLabel* exifFocal_;
    QLabel* exifAperture_;
    QLabel* exifShutter_;
    QLabel* exifIso_;
    QLabel* exifDate_;
    QLabel* exifDimensions_;
};
