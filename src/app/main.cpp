#include <QApplication>
#include <QIcon>

#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("AstroTracker");
    app.setApplicationName("AstroTracker");
    app.setApplicationVersion(QStringLiteral(ASTROTRACKER_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/favicon/app-512.png")));

    MainWindow window;
    window.show();

    if (argc > 1)
        window.openPath(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}