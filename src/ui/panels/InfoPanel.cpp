#include "ui/panels/InfoPanel.h"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <cmath>

InfoPanel::InfoPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // --- Tracking Stats ---
    trackingSection_ = new QGroupBox(tr("Tracking Stats"), this);
    auto* tlay = new QVBoxLayout(trackingSection_);
    tlay->setContentsMargins(8, 16, 8, 8);
    tlay->setSpacing(4);

    validCount_ = new QLabel("--", this);
    validCount_->setStyleSheet("color: #2e8b57;");
    tlay->addWidget(validCount_);

    predictedCount_ = new QLabel("--", this);
    predictedCount_->setStyleSheet("color: #ffa500;");
    tlay->addWidget(predictedCount_);

    lostCount_ = new QLabel("--", this);
    lostCount_->setStyleSheet("color: #c04040;");
    tlay->addWidget(lostCount_);

    trackingPercent_ = new QLabel("--", this);
    tlay->addWidget(trackingPercent_);

    mainLayout->addWidget(trackingSection_);

    // --- Frame Info ---
    frameSection_ = new QGroupBox(tr("Frame Info"), this);
    auto* flay = new QVBoxLayout(frameSection_);
    flay->setContentsMargins(8, 16, 8, 8);
    flay->setSpacing(4);

    frameIndexLabel_ = new QLabel("--", this);
    flay->addWidget(frameIndexLabel_);

    timeLabel_ = new QLabel("--", this);
    flay->addWidget(timeLabel_);

    methodLabel_ = new QLabel("--", this);
    flay->addWidget(methodLabel_);

    confidenceLabel_ = new QLabel("--", this);
    flay->addWidget(confidenceLabel_);

    offsetLabel_ = new QLabel("--", this);
    flay->addWidget(offsetLabel_);

    statusLabel_ = new QLabel("--", this);
    flay->addWidget(statusLabel_);

    mainLayout->addWidget(frameSection_);

    // --- EXIF Info ---
    exifSection_ = new QGroupBox(tr("EXIF Info"), this);
    auto* elay = new QVBoxLayout(exifSection_);
    elay->setContentsMargins(8, 16, 8, 8);
    elay->setSpacing(4);

    exifCamera_ = new QLabel("--", this);
    elay->addWidget(exifCamera_);

    exifLens_ = new QLabel("--", this);
    elay->addWidget(exifLens_);

    exifFocal_ = new QLabel("--", this);
    elay->addWidget(exifFocal_);

    exifAperture_ = new QLabel("--", this);
    elay->addWidget(exifAperture_);

    exifShutter_ = new QLabel("--", this);
    elay->addWidget(exifShutter_);

    exifIso_ = new QLabel("--", this);
    elay->addWidget(exifIso_);

    exifDate_ = new QLabel("--", this);
    elay->addWidget(exifDate_);

    exifDimensions_ = new QLabel("--", this);
    elay->addWidget(exifDimensions_);

    mainLayout->addWidget(exifSection_);
    mainLayout->addStretch(1);
}

void InfoPanel::setTrackingStats(int valid, int predicted, int lost)
{
    const int total = valid + predicted + lost;
    validCount_->setText(tr("Valid: %1").arg(valid));
    predictedCount_->setText(tr("Predicted: %1").arg(predicted));
    lostCount_->setText(tr("Lost: %1").arg(lost));

    if (total > 0) {
        const double pct = 100.0 * valid / total;
        trackingPercent_->setText(tr("Success: %1%").arg(pct, 0, 'f', 1));
    } else {
        trackingPercent_->setText("--");
    }
}

void InfoPanel::setFrameInfo(int frameIndex, int totalFrames,
                              const QString& time,
                              const QString& method,
                              float confidence,
                              float offsetX, float offsetY,
                              const QString& status)
{
    frameIndexLabel_->setText(tr("#%1 / %2").arg(frameIndex).arg(totalFrames));
    timeLabel_->setText(time);
    methodLabel_->setText(tr("Method: %1").arg(method));
    confidenceLabel_->setText(tr("Confidence: %1%").arg(std::lround(confidence * 100)));
    offsetLabel_->setText(tr("Offset: %1, %2")
                              .arg(offsetX, 0, 'f', 1)
                              .arg(offsetY, 0, 'f', 1));
    statusLabel_->setText(tr("Status: %1").arg(status));
}

void InfoPanel::setExifInfo(const PhotoExifInfo& exif)
{
    exifCamera_->setText(exif.hasCamera()
                             ? QString::fromStdString(exif.camera)
                             : "--");
    exifLens_->setText(exif.hasLens()
                           ? QString::fromStdString(exif.lens)
                           : "--");
    exifFocal_->setText(exif.hasFocal()
                            ? QString::fromStdString(exif.focalString())
                            : "--");
    exifAperture_->setText(exif.hasAperture()
                               ? QString::fromStdString(exif.apertureString())
                               : "--");
    exifShutter_->setText(exif.hasShutter()
                               ? QString::fromStdString(exif.shutterString())
                               : "--");
    exifIso_->setText(exif.hasIso()
                          ? tr("ISO %1").arg(exif.iso)
                          : "--");
    exifDate_->setText(exif.hasDate()
                           ? QString::fromStdString(exif.date)
                           : "--");
    if (exif.width > 0 && exif.height > 0)
        exifDimensions_->setText(tr("%1x%2").arg(exif.width).arg(exif.height));
    else
        exifDimensions_->setText("--");

    exifSection_->setVisible(!exif.isEmpty());
}

void InfoPanel::clearExifInfo()
{
    exifCamera_->setText("--");
    exifLens_->setText("--");
    exifFocal_->setText("--");
    exifAperture_->setText("--");
    exifShutter_->setText("--");
    exifIso_->setText("--");
    exifDate_->setText("--");
    exifDimensions_->setText("--");
    exifSection_->setVisible(false);
}

void InfoPanel::setMaterialInfo(const QString& info)
{
    frameIndexLabel_->setText(info);
}

void InfoPanel::setTrackingVisible(bool visible)
{
    trackingSection_->setVisible(visible);
}

void InfoPanel::setFrameInfoVisible(bool visible)
{
    frameSection_->setVisible(visible);
}

void InfoPanel::setExifVisible(bool visible)
{
    exifSection_->setVisible(visible);
}
