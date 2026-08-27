#include "ui/timeline/TimelineWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(2);

    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setEnabled(false);
    lay->addWidget(slider_);

    auto* bottom = new QHBoxLayout();
    frameLabel_ = new QLabel(tr("Frame: - / -"), this);
    timeLabel_ = new QLabel("00:00:00.000", this);
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bottom->addWidget(frameLabel_, 1);
    bottom->addWidget(timeLabel_);
    lay->addLayout(bottom);

    connect(slider_, &QSlider::valueChanged, this, &TimelineWidget::valueChanged);
}

void TimelineWidget::setRange(int min, int max)
{
    slider_->setRange(min, max);
}

void TimelineWidget::setValue(int value)
{
    slider_->setValue(value);
}

void TimelineWidget::setEnabled(bool enabled)
{
    slider_->setEnabled(enabled);
}

void TimelineWidget::setFrameLabel(int current, int total)
{
    frameLabel_->setText(tr("Frame: %1 / %2").arg(current).arg(total));
}

void TimelineWidget::setTimeLabel(const QString& time)
{
    timeLabel_->setText(time);
}

int TimelineWidget::value() const
{
    return slider_->value();
}
