#include "ui/dialogs/ExportDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>

ExportDialog::ExportDialog(const QString& defaultName, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Exportar vídeo estabilizado"));
    setMinimumWidth(420);

    auto* layout = new QFormLayout(this);

    // Output path
    auto* pathLayout = new QHBoxLayout;
    pathEdit_ = new QLineEdit(defaultName, this);
    auto* browseBtn = new QPushButton(tr("Examinar…"), this);
    connect(browseBtn, &QPushButton::clicked, this, &ExportDialog::browse);
    pathLayout->addWidget(pathEdit_);
    pathLayout->addWidget(browseBtn);
    layout->addRow(tr("Ruta de salida:"), pathLayout);

    // Suffix
    suffixCombo_ = new QComboBox(this);
    suffixCombo_->addItems({ "_stabilized", "_tracked", "_fixed" });
    suffixCombo_->setEditable(true);
    layout->addRow(tr("Sufijo:"), suffixCombo_);

    // Overwrite
    overwriteCheck_ = new QCheckBox(tr("Sobrescribir si existe"), this);
    layout->addRow(QString(), overwriteCheck_);

    // Buttons
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Exportar"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
}

ExportDialog::Options ExportDialog::options() const
{
    Options opts;
    opts.outputPath = pathEdit_->text();
    opts.suffix = suffixCombo_->currentText();
    opts.overwrite = overwriteCheck_->isChecked();
    return opts;
}

void ExportDialog::browse()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Guardar vídeo"),
        pathEdit_->text(),
        tr("Vídeo MP4 (*.mp4);;Todos los archivos (*)"));
    if (!path.isEmpty())
        pathEdit_->setText(path);
}
