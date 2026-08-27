#include "ui/panels/InputPanel.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

InputPanel::InputPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    openVideoBtn_ = new QPushButton(tr("Open Video"), this);
    lay->addWidget(openVideoBtn_);
    connect(openVideoBtn_, &QPushButton::clicked, this, &InputPanel::openVideo);

    openPhotosBtn_ = new QPushButton(tr("Open Photos..."), this);
    lay->addWidget(openPhotosBtn_);
    connect(openPhotosBtn_, &QPushButton::clicked, this, &InputPanel::openPhotos);

    openProjectBtn_ = new QPushButton(tr("Open Project..."), this);
    lay->addWidget(openProjectBtn_);
    connect(openProjectBtn_, &QPushButton::clicked, this, &InputPanel::openProject);

    materialInfoLabel_ = new QLabel(this);
    materialInfoLabel_->setWordWrap(true);
    materialInfoLabel_->setStyleSheet("color: #b0b0b0;");
    lay->addWidget(materialInfoLabel_);

    lay->addStretch(1);
}

void InputPanel::setMaterialInfo(const QString& info)
{
    materialInfoLabel_->setText(info);
}
