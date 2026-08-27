#include "ui/panels/ExportPanel.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

ExportPanel::ExportPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    selectionLabel_ = new QLabel(this);
    selectionLabel_->setStyleSheet("color: #b0b0b0;");
    lay->addWidget(selectionLabel_);

    exportBtn_ = new QPushButton(tr("Export..."), this);
    exportBtn_->setEnabled(false);
    lay->addWidget(exportBtn_);

    lay->addStretch(1);

    connect(exportBtn_, &QPushButton::clicked, this, &ExportPanel::exportRequested);
}

void ExportPanel::setSelectionInfo(int selected, int total)
{
    selectionLabel_->setText(tr("%1 of %2 photos selected").arg(selected).arg(total));
    exportBtn_->setEnabled(total > 0 && selected > 0);
}
