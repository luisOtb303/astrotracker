#include "ui/panels/ObjectPanel.h"

#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>

ObjectPanel::ObjectPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    lay->addWidget(new QLabel(tr("Profile"), this));

    profileCombo_ = new QComboBox(this);
    profileCombo_->addItem(tr("Auto"));
    profileCombo_->addItem(tr("Sun"));
    profileCombo_->addItem(tr("Moon"));
    profileCombo_->addItem(tr("Planet"));
    profileCombo_->addItem(tr("Lunar Eclipse"));
    lay->addWidget(profileCombo_);

    descriptionLabel_ = new QLabel(tr("Automatic detection of the object"), this);
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setStyleSheet("color: #b0b0b0; font-size: 11px;");
    lay->addWidget(descriptionLabel_);

    lay->addStretch(1);

    connect(profileCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                const char* descs[] = {
                    "Automatic detection of the object",
                    "Solar disc tracking",
                    "Lunar disc tracking",
                    "Planetary disc tracking",
                    "Lunar eclipse (partial/corona)"
                };
                if (idx >= 0 && idx < 5)
                    descriptionLabel_->setText(tr(descs[idx]));
                emit profileChanged(idx);
            });
}

void ObjectPanel::setProfile(int index)
{
    profileCombo_->setCurrentIndex(index);
}

int ObjectPanel::profile() const
{
    return profileCombo_->currentIndex();
}
