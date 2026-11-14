#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("fxbViewer");
    QCoreApplication::setApplicationName("FXB Viewer");
    QCoreApplication::setApplicationVersion("2.0.0");
    MainWindow window;
    window.show();
    return app.exec();
}
