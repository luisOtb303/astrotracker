#include "ui/timeline/TransportBar.h"

#include <QHBoxLayout>

TransportBar::TransportBar(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    auto* stepBackBtn = new QPushButton("\u25C0", this); // ◀
    stepBackBtn->setToolTip(tr("Step backward"));
    stepBackBtn->setFixedWidth(32);
    lay->addWidget(stepBackBtn);

    playBtn_ = new QPushButton("\u25B6", this); // ▶
    playBtn_->setToolTip(tr("Play / Pause"));
    playBtn_->setFixedWidth(48);
    lay->addWidget(playBtn_);

    auto* stepFwdBtn = new QPushButton("\u25B6", this); // ▶
    stepFwdBtn->setToolTip(tr("Step forward"));
    stepFwdBtn->setFixedWidth(32);
    lay->addWidget(stepFwdBtn);

    stopBtn_ = new QPushButton("\u25A0", this); // ■
    stopBtn_->setToolTip(tr("Stop"));
    stopBtn_->setFixedWidth(32);
    lay->addWidget(stopBtn_);

    lay->addStretch(1);

    connect(stepBackBtn, &QPushButton::clicked, this, &TransportBar::stepBackward);
    connect(playBtn_, &QPushButton::clicked, this, &TransportBar::playPause);
    connect(stepFwdBtn, &QPushButton::clicked, this, &TransportBar::stepForward);
    connect(stopBtn_, &QPushButton::clicked, this, &TransportBar::stop);
}

void TransportBar::setPlaying(bool playing)
{
    playBtn_->setText(playing ? "\u23F8" : "\u25B6"); // ⏸ or ▶
}

void TransportBar::setEnabled(bool enabled)
{
    playBtn_->setEnabled(enabled);
    stopBtn_->setEnabled(enabled);
}
