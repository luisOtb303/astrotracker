#include "ui/dialogs/TrackingLostDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

TrackingLostDialog::TrackingLostDialog(int lostFrame, int totalFrames, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Tracking perdido"));
    setMinimumWidth(380);

    auto* layout = new QVBoxLayout(this);

    auto* label = new QLabel(
        tr("El objeto se perdió en el frame %1 de %2.\n"
           "¿Qué desea hacer?")
            .arg(lostFrame)
            .arg(totalFrames),
        this);
    label->setWordWrap(true);
    layout->addWidget(label);

    layout->addSpacing(12);

    auto* continueBtn = new QPushButton(tr("Continuar (ignorar pérdida)"), this);
    connect(continueBtn, &QPushButton::clicked, this, &TrackingLostDialog::onContinue);
    layout->addWidget(continueBtn);

    auto* extendBtn = new QPushButton(tr("Expandir ROI y reintentar"), this);
    connect(extendBtn, &QPushButton::clicked, this, &TrackingLostDialog::onExtendRoi);
    layout->addWidget(extendBtn);

    auto* manualBtn = new QPushButton(tr("Seleccionar posición manual"), this);
    connect(manualBtn, &QPushButton::clicked, this, &TrackingLostDialog::onManualPoint);
    layout->addWidget(manualBtn);

    layout->addSpacing(8);

    auto* cancelBtn = new QPushButton(tr("Cancelar exportación"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &TrackingLostDialog::onManualPoint);
    layout->addWidget(cancelBtn);
}

void TrackingLostDialog::onContinue()
{
    action_ = Action::Continue;
    accept();
}

void TrackingLostDialog::onExtendRoi()
{
    action_ = Action::ExtendRoi;
    accept();
}

void TrackingLostDialog::onManualPoint()
{
    action_ = Action::ManualPoint;
    accept();
}
