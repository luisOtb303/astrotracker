#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    struct Options {
        QString outputPath;
        QString suffix;
        bool overwrite = false;
    };

    explicit ExportDialog(const QString& defaultName, QWidget* parent = nullptr);

    Options options() const;

private slots:
    void browse();

private:
    QLineEdit* pathEdit_ = nullptr;
    QComboBox* suffixCombo_ = nullptr;
    QCheckBox* overwriteCheck_ = nullptr;
};
