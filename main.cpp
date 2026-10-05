#include <QApplication>
#include "AppStyle.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ZeroTier GUI");
    app.setOrganizationName("ZeroTierGUI");
    applyAppStyle(app);
    MainWindow window;
    window.show();
    return app.exec();
}
