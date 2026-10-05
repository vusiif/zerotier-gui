#include <QApplication>
#include "AppStyle.h"
#include "MainWindow.h"
#include "InstallWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ZeroTier GUI");
    app.setOrganizationName("ZeroTierGUI");
    applyAppStyle(app);
    if (!zeroTierInstalled()) {
        InstallWindow installer;
        if (installer.exec() != QDialog::Accepted) return 0;
    }
    MainWindow window;
    window.show();
    return app.exec();
}
