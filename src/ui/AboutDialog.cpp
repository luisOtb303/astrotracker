#include "ui/AboutDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#ifndef ASTROTRACKER_VERSION
#define ASTROTRACKER_VERSION "0.0.0"
#endif
#ifndef ASTROTRACKER_BUILD_TYPE
#define ASTROTRACKER_BUILD_TYPE "unknown"
#endif
#ifndef ASTROTRACKER_GIT_REV
#define ASTROTRACKER_GIT_REV "unknown"
#endif

namespace {

QString loadResource(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

// Componentes de terceros con su licencia y el texto completo embebido.
struct LicenseEntry
{
    const char* title;    // texto del elemento de la lista
    const char* summary;  // breve descripción del componente y su licencia
    const char* resource; // recurso Qt con el texto completo
};

const LicenseEntry kLicenses[] = {
    {"AstroTracker", "Aplicación — licencia del proyecto: GNU GPL v3.",
     ":/licenses/gpl-3.0.txt"},
    {"Qt 6", "UI (Widgets), vinculación dinámica: GNU LGPL v3.",
     ":/licenses/lgpl-3.0.txt"},
    {"OpenCV", "Visión (cv::Mat, seguimiento): Apache-2.0.",
     ":/licenses/apache-2.0.txt"},
    {"FFmpeg (libav)", "Vídeo (build gyan.dev con libx264): la combinación "
                       "resultante es GNU GPL v2.",
     ":/licenses/gpl-2.0.txt"},
    {"LibRaw", "Decodificación RAW (CR2/CR3, DNG, NEF…): GNU LGPL v2.1 "
               "(alternativa CDDL-1.0).",
     ":/licenses/lgpl-2.1.txt"},
    {"LibRaw (alternativa)", "Opción CDDL-1.0 del mismo LibRaw.",
     ":/licenses/cddl-1.0.txt"},
    {"vid.stab", "Referencia de estabilización; módulo opcional, fuera del MVP: "
                 "GNU GPL v2 o posterior.",
     ":/licenses/gpl-2.0.txt"},
};

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Acerca de AstroTracker"));
    setMinimumSize(560, 420);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(createAboutTab(), tr("Acerca de"));
    tabs_->addTab(createLicensesTab(), tr("Licencias"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* aboutQt = buttons->addButton(tr("Acerca de &Qt"), QDialogButtonBox::ActionRole);
    connect(aboutQt, &QPushButton::clicked, this, [this] { QMessageBox::aboutQt(this); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* lay = new QVBoxLayout(this);
    lay->addWidget(tabs_);
    lay->addWidget(buttons);
}

void AboutDialog::showTab(int index)
{
    tabs_->setCurrentIndex(index);
}

QWidget* AboutDialog::createAboutTab()
{
    auto* page = new QWidget(this);
    auto* lay = new QVBoxLayout(page);

    auto* head = new QHBoxLayout;
    auto* icon = new QLabel(page);
    icon->setPixmap(QIcon(QStringLiteral(":/favicon/app-512.png")).pixmap(64, 64));
    head->addWidget(icon);

    auto* name = new QLabel(QApplication::applicationName(), page);
    QFont nameFont = name->font();
    nameFont.setPointSize(nameFont.pointSize() + 8);
    nameFont.setBold(true);
    name->setFont(nameFont);
    head->addWidget(name, 1);
    lay->addLayout(head);

    auto* desc = new QLabel(tr("Sigue y estabiliza el Sol o la Luna en vídeos y "
                               "secuencias de fotos tomadas sobre trípode manual, "
                               "sin star tracker."),
                            page);
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->addRow(tr("Versión:"), new QLabel(QStringLiteral(ASTROTRACKER_VERSION), page));
    form->addRow(tr("Revisión:"), new QLabel(QStringLiteral(ASTROTRACKER_GIT_REV), page));
    form->addRow(tr("Compilación:"),
                 new QLabel(QStringLiteral(ASTROTRACKER_BUILD_TYPE " · " __DATE__ " " __TIME__), page));
    form->addRow(tr("Copyright:"), new QLabel(tr("Copyright © 2026 AstroTracker Team"), page));
    form->addRow(tr("Licencia:"), new QLabel(tr("GNU GPL v3 — GNU General Public License"), page));
    lay->addLayout(form);

    lay->addStretch(1);
    return page;
}

QWidget* AboutDialog::createLicensesTab()
{
    auto* page = new QWidget(this);
    auto* lay = new QVBoxLayout(page);

    auto* intro = new QLabel(tr("AstroTracker es software libre (GPLv3). Los componentes "
                                "de terceros mantienen sus propias licencias; a "
                                "continuación se muestran los textos completos."),
                             page);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    auto* split = new QHBoxLayout;
    licensesList_ = new QListWidget(page);
    licensesList_->setFixedWidth(200);
    for (const LicenseEntry& e : kLicenses)
        licensesList_->addItem(QString::fromUtf8(e.title));
    split->addWidget(licensesList_);

    licenseView_ = new QTextBrowser(page);
    licenseView_->setOpenExternalLinks(true);
    split->addWidget(licenseView_, 1);
    lay->addLayout(split, 1);

    connect(licensesList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= static_cast<int>(sizeof(kLicenses) / sizeof(kLicenses[0])))
            return;
        const LicenseEntry& e = kLicenses[row];
        licenseView_->setHtml(QStringLiteral("<h3>%1</h3><p>%2</p><hr>%3")
                                  .arg(QString::fromUtf8(e.title),
                                       QString::fromUtf8(e.summary),
                                       loadResource(QString::fromUtf8(e.resource))));
    });
    licensesList_->setCurrentRow(0);
    return page;
}