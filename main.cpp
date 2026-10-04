#include <QApplication>
#include "ElaApplication.h"
#include "MainWindow.h"
#include "InstallWindow.h"
#include "ElaTheme.h"
#include <QSettings>
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ZeroTier GUI");
    eApp->init();
    eTheme->setThemeMode(QSettings("ZeroTierGui", "ZeroTierGui").value("dark", false).toBool() ? ElaThemeType::Dark : ElaThemeType::Light);
    // Do not construct the management UI until the local installation check passes.
    if (!zeroTierInstalled()) {
        InstallWindow installer;
        if (installer.exec() != QDialog::Accepted) return 0;
    }
    MainWindow window;
    window.show();
    return app.exec();
}
