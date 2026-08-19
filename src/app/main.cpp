#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("AstroTracker");
    app.setApplicationName("AstroTracker");
    app.setApplicationVersion("0.1.0");

    MainWindow window;
    window.show();

    if (argc > 1)
        window.openPath(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}