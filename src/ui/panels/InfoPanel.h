#pragma once

#include <QScrollArea>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QString>
#include "common/PhotoExifInfo.h"
#include "common/PhotoFileInfo.h"

// Right-side info panel showing tracking statistics, frame info, and file
// metadata (file + EXIF in photos mode; file + frame in video mode).
// All text is selectable so it can be copied.
class InfoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit InfoPanel(QWidget* parent = nullptr);

    // Tracking stats (video mode)
    void setTrackingStats(int valid, int predicted, int lost);

    // Frame info (video mode)
    void setFrameInfo(int frameIndex, int totalFrames,
                      const QString& time,
                      const QString& method,
                      float confidence,
                      float offsetX, float offsetY,
                      const QString& status);

    // Photos mode: file + EXIF combined.
    void setFileAndExif(const PhotoFileInfo& file, const PhotoExifInfo& exif);

    // Video mode: file metadata + current frame position.
    void setVideoFileInfo(const QString& name, const QString& path,
                          qint64 sizeBytes, const QString& modifyDate,
                          int frameIndex, int totalFrames);

    void clearExifInfo();
    void clearFileInfo();

    // Material info
    void setMaterialInfo(const QString& info);

    void setTrackingVisible(bool visible);
    void setFrameInfoVisible(bool visible);
    void setFileInfoVisible(bool visible);
    void setExifVisible(bool visible); // kept for compatibility

    signals:
    void toggleVisibility();

private:
    static QLabel* makeLabel(const QString& title);
    void setLabelText(QLabel* label, const QString& value);
    static QString formatSize(qint64 bytes);

    QWidget* trackingSection_;
    QWidget* frameSection_;

    // File + metadata section (photos: file + EXIF; video: file + frame).
    QWidget* fileSection_;
    QLabel* fileValueName_;
    QLabel* fileValuePath_;
    QLabel* fileValueSize_;
    QLabel* fileValueType_;
    QLabel* fileValueDate_;
    QLabel* fileValueDimension_;
    QLabel* fileValueExtra_; // EXIF cámara/lente... o frame X/Y

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
};
