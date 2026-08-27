#pragma once

#include <QDialog>

class QLabel;
class QSlider;

class TrackingLostDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Action { Continue, ExtendRoi, ManualPoint, Cancel };

    explicit TrackingLostDialog(int lostFrame, int totalFrames, QWidget* parent = nullptr);

    Action selectedAction() const { return action_; }

private slots:
    void onContinue();
    void onExtendRoi();
    void onManualPoint();

private:
    Action action_ = Action::Cancel;
};
