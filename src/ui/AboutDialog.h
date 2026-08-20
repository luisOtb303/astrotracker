#pragma once

#include <QDialog>

class QListWidget;
class QTabWidget;
class QTextBrowser;

// Diálogo "Ayuda > Acerca de AstroTracker" y "Ayuda > Licencias": pestañas con
// la información de la aplicación (versión, revisión git, compilación y
// copyright) y con las licencias de AstroTracker y de los componentes de
// terceros (textos completos embebidos como recursos Qt).
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

    // Abre el diálogo mostrando la pestaña indicada (0 = Acerca de, 1 = Licencias).
    void showTab(int index);

private:
    QWidget* createAboutTab();
    QWidget* createLicensesTab();

    QTabWidget* tabs_ = nullptr;
    QListWidget* licensesList_ = nullptr;
    QTextBrowser* licenseView_ = nullptr;
};