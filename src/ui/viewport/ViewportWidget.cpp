#include "ui/viewport/ViewportWidget.h"
#include "ui/VideoView.h"
#include "ui/viewport/TrackingOverlay.h"
#include "ui/viewport/ZoomController.h"

ViewportWidget::ViewportWidget(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet("font-weight: bold; padding: 2px;");
    layout->addWidget(titleLabel_);

    // Stack VideoView and TrackingOverlay on top of each other.
    auto* stack = new QWidget(this);
    auto* stackLayout = new QVBoxLayout(stack);
    stackLayout->setContentsMargins(0, 0, 0, 0);

    view_ = new VideoView(stack);
    overlay_ = new TrackingOverlay(stack);

    stackLayout->addWidget(view_);
    overlay_->setGeometry(0, 0, 10000, 10000); // will be resized by parent
    overlay_->raise();

    layout->addWidget(stack, 1);

    zoom_ = new ZoomController(view_, this);

    view_->setMinimumSize(320, 240);
}

void ViewportWidget::setDisplayMode(DisplayMode mode)
{
    mode_ = mode;
    updateOverlayVisibility();
    emit displayModeChanged(mode);
}

void ViewportWidget::setFrame(const cv::Mat& frame)
{
    view_->setFrame(frame);
}

void ViewportWidget::setTitle(const QString& title)
{
    titleLabel_->setText(title);
}

void ViewportWidget::updateOverlayVisibility()
{
    overlay_->setVisible(mode_ == DisplayMode::Tracking);
}
