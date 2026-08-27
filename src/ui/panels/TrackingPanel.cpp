#include "ui/panels/TrackingPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>

TrackingPanel::TrackingPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    // --- Video mode controls ---
    videoControls_ = new QWidget(this);
    auto* vlay = new QVBoxLayout(videoControls_);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(4);

    vlay->addWidget(new QLabel(tr("Tracker"), this));
    trackerCombo_ = new QComboBox(this);
    trackerCombo_->addItem(tr("Template"));
    trackerCombo_->addItem(tr("Centroid"));
    trackerCombo_->addItem(tr("Disc (profile)"));
    vlay->addWidget(trackerCombo_);

    vlay->addWidget(new QLabel(tr("Border"), this));
    borderCombo_ = new QComboBox(this);
    borderCombo_->addItem(tr("Black border"));
    borderCombo_->addItem(tr("Replicate border"));
    vlay->addWidget(borderCombo_);

    vlay->addWidget(new QLabel(tr("Smoothing"), this));
    smoothSpin_ = new QDoubleSpinBox(this);
    smoothSpin_->setRange(0.01, 1.0);
    smoothSpin_->setSingleStep(0.05);
    smoothSpin_->setValue(0.3);
    vlay->addWidget(smoothSpin_);

    analyzeBtn_ = new QPushButton(tr("Analyze"), this);
    vlay->addWidget(analyzeBtn_);

    stopBtn_ = new QPushButton(tr("Stop"), this);
    stopBtn_->setEnabled(false);
    vlay->addWidget(stopBtn_);

    lay->addWidget(videoControls_);

    // --- Photo mode controls ---
    photoControls_ = new QWidget(this);
    auto* play = new QVBoxLayout(photoControls_);
    play->setContentsMargins(0, 0, 0, 0);
    play->setSpacing(4);

    play->addWidget(new QLabel(tr("Method (this photo)"), this));
    methodCombo_ = new QComboBox(this);
    methodCombo_->addItem(tr("(per profile)"));
    methodCombo_->addItem(tr("Template"));
    methodCombo_->addItem(tr("ArcBlob"));
    methodCombo_->addItem(tr("KnownRadius"));
    methodCombo_->addItem(tr("PhaseCorrelation"));
    methodCombo_->addItem(tr("Centroid"));
    play->addWidget(methodCombo_);

    clearOverrideBtn_ = new QPushButton(tr("Clear override"), this);
    clearOverrideBtn_->setEnabled(false);
    play->addWidget(clearOverrideBtn_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet("color: #b0b0b0;");
    play->addWidget(statusLabel_);

    analyzeBtn_ = new QPushButton(tr("Auto Calculate"), this);
    play->addWidget(analyzeBtn_);

    stopBtn_ = new QPushButton(tr("Stop"), this);
    stopBtn_->setEnabled(false);
    play->addWidget(stopBtn_);

    lay->addWidget(photoControls_);
    lay->addStretch(1);

    // Connections
    connect(analyzeBtn_, &QPushButton::clicked, this, &TrackingPanel::analyzeRequested);
    connect(stopBtn_, &QPushButton::clicked, this, &TrackingPanel::stopRequested);
    connect(trackerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackingPanel::trackerChanged);
    connect(borderCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackingPanel::borderModeChanged);
    connect(smoothSpin_, &QDoubleSpinBox::valueChanged,
            this, &TrackingPanel::smoothingChanged);

    setMode(false);
}

int TrackingPanel::trackerIndex() const { return trackerCombo_->currentIndex(); }
int TrackingPanel::borderMode() const { return borderCombo_->currentIndex(); }
double TrackingPanel::smoothing() const { return smoothSpin_->value(); }
int TrackingPanel::methodOverride() const { return methodCombo_->currentIndex(); }

void TrackingPanel::setMode(bool videoMode)
{
    videoMode_ = videoMode;
    videoControls_->setVisible(videoMode);
    photoControls_->setVisible(!videoMode);
}

void TrackingPanel::setTrackingActive(bool active)
{
    analyzeBtn_->setEnabled(!active);
    stopBtn_->setEnabled(active);
    trackerCombo_->setEnabled(!active);
    borderCombo_->setEnabled(!active);
    smoothSpin_->setEnabled(!active);
    methodCombo_->setEnabled(!active);
}

void TrackingPanel::setAnalyzeEnabled(bool enabled)
{
    analyzeBtn_->setEnabled(enabled);
}

void TrackingPanel::setStatusText(const QString& text)
{
    statusLabel_->setText(text);
}
