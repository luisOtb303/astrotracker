#include "ui/panels/TransformPanel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

TransformPanel::TransformPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    lay->addWidget(new QLabel(tr("Border"), this));
    borderCombo_ = new QComboBox(this);
    borderCombo_->addItem(tr("Black border"));
    borderCombo_->addItem(tr("Replicate border"));
    lay->addWidget(borderCombo_);

    lay->addWidget(new QLabel(tr("Smoothing"), this));
    smoothSpin_ = new QDoubleSpinBox(this);
    smoothSpin_->setRange(0.01, 1.0);
    smoothSpin_->setSingleStep(0.05);
    smoothSpin_->setValue(0.3);
    lay->addWidget(smoothSpin_);

    previewCheck_ = new QCheckBox(tr("Preview stabilized"), this);
    lay->addWidget(previewCheck_);

    lay->addStretch(1);

    connect(borderCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TransformPanel::borderModeChanged);
    connect(smoothSpin_, &QDoubleSpinBox::valueChanged,
            this, &TransformPanel::smoothingChanged);
    connect(previewCheck_, &QCheckBox::toggled,
            this, &TransformPanel::previewToggled);
}

int TransformPanel::borderMode() const { return borderCombo_->currentIndex(); }
double TransformPanel::smoothing() const { return smoothSpin_->value(); }
bool TransformPanel::previewEnabled() const { return previewCheck_->isChecked(); }
