#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <opencv2/core.hpp>

class VideoView;
class TrackingOverlay;
class ZoomController;

// Container widget that wraps a VideoView with its TrackingOverlay and
// provides display mode switching (Original / Tracking / Result).
class ViewportWidget : public QWidget
{
    Q_OBJECT
public:
    enum class DisplayMode { Original, Tracking, Result };
    Q_ENUM(DisplayMode)

    explicit ViewportWidget(const QString& title, QWidget* parent = nullptr);

    VideoView* videoView() const { return view_; }
    TrackingOverlay* overlay() const { return overlay_; }
    ZoomController* zoom() const { return zoom_; }

    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return mode_; }

    void setFrame(const cv::Mat& frame);
    void setTitle(const QString& title);

signals:
    void displayModeChanged(DisplayMode mode);

private:
    void updateOverlayVisibility();

    QLabel* titleLabel_;
    VideoView* view_;
    TrackingOverlay* overlay_;
    ZoomController* zoom_;
    DisplayMode mode_ = DisplayMode::Original;
};
