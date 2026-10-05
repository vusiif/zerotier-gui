#include <QApplication>
#include "AppStyle.h"
#include "MainWindow.h"
#include "InstallWindow.h"
#include "ServiceStartup.h"

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
    ServiceStartup startup;
    window.beginOperation();
    window.showNotice(QStringLiteral("正在检查 ZeroTier 服务…"));
    startup.ensureRunning([&window](bool ok, const QString &error) {
        window.endOperation();
        const auto message = ok ? QStringLiteral("ZeroTier 服务已就绪。") : error;
        window.appendOutput(message);
        window.showNotice(message, ok ? 5000 : 0);
    });
    return app.exec();
}
