#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ZeroTier GUI");

    MainWindow window;
    window.show();

    return app.exec();
}
