#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QDir>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QSettings>
#include <QVBoxLayout>
#include "ElaTheme.h"
#include <iostream>
#include "ElaApplication.h"
#include "MainWindow.h"
#include "InstallWindow.h"
#include "ZeroTierClient.h"

static void pump(int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
#include "FunctionalChecks.h"
int main(int argc, char **argv)
{
    if (!qEnvironmentVariable("ZT_TEST_PAYLOAD").isEmpty() && argc > 1 && std::string(argv[1]) != "--functional") {
        QCoreApplication child(argc, argv);
        QJsonArray arguments;
        for (const auto &arg : child.arguments().mid(1)) arguments.append(arg);
        QFile history(qEnvironmentVariable("ZT_TEST_HISTORY"));
        if (history.open(QIODevice::Append)) history.write(QJsonDocument(arguments).toJson(QJsonDocument::Compact) + '\n');
        if (child.arguments().value(1) == "-h") { std::cerr << "Available switches: -h -v -j\nAvailable commands: info listnetworks dump\n"; return 0; }
        if (child.arguments().value(1) == "-v") { std::cout << "1.16.2-fixture\n"; return 0; }
        if (child.arguments().value(1) == "dump") {
            const auto mode = qEnvironmentVariable("ZT_TEST_DUMP_MODE");
            if (mode == "fail") { std::cerr << "fixture diagnostic permission denied"; return 5; }
            if (mode == "bad") { std::cout << "200 dump OK"; return 0; }
            const QByteArray content = "platform: Windows\nstatus\n{\"version\":\"fixture\",\"name\":\"测试节点\"}\n";
            if (mode == "inline") { std::cout << content.toStdString(); return 0; }
            const auto path = qEnvironmentVariable("ZT_TEST_DUMP");
            QFile report(path); if (!report.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 3;
            report.write(content); report.close();
            std::cout << "Writing dump to: " << path.toStdString() << '\n'; return 0;
        }
        if (child.arguments().value(1) == "set" || child.arguments().value(1) == "get") {
            QFile payload(qEnvironmentVariable("ZT_TEST_PAYLOAD"));
            if (!payload.open(QIODevice::ReadOnly)) return 3;
            auto rows = QJsonDocument::fromJson(payload.readAll()).array(); payload.close();
            auto object = rows.first().toObject();
            if (child.arguments().value(1) == "set") {
                const auto assignment = child.arguments().last();
                object[assignment.section('=', 0, 0)] = assignment.endsWith("=1"); rows[0] = object;
                if (!payload.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 3;
                payload.write(QJsonDocument(rows).toJson()); std::cout << "{}";
            } else std::cout << "10.0.0.2\n";
            return 0;
        }
        if (child.arguments().contains("-j")) {
            const auto home = qEnvironmentVariable("ZT_TEST_HOME");
            if (!home.isEmpty() && child.arguments().last() == "info") {
                const QJsonObject info{{"address", "abcdef1234"}, {"online", true}, {"version", "fixture"},
                    {"config", QJsonObject{{"settings", QJsonObject{{"homeDir", home}}}}}};
                std::cout << QJsonDocument(info).toJson().toStdString(); return 0;
            }
            QFile payload(qEnvironmentVariable("ZT_TEST_PAYLOAD"));
            if (!payload.open(QIODevice::ReadOnly)) return 3;
            std::cout << payload.readAll().toStdString();
        } else std::cout << "{}";
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--hang") { QCoreApplication app(argc, argv); QTimer::singleShot(10000, &app, &QCoreApplication::quit); return app.exec(); }
        if (std::string(argv[i]) == "-j") {
            const std::string command = i + 1 < argc ? argv[i + 1] : "";
            if (command == "info") std::cout << "{\"online\":true,\"address\":\"abcdef1234\",\"version\":\"test\"}";
            else std::cout << "[]";
            return 0;
        }
    }
    std::cerr << "creating application\n";
    QApplication app(argc, argv);
    QSettings preferences("ZeroTierGui", "ZeroTierGui");
    const bool hadThemeSetting = preferences.contains("dark");
    const auto oldThemeSetting = preferences.value("dark");
#ifdef Q_OS_WIN
    // The offscreen Windows plugin does not enumerate system fonts.
    QFontDatabase::addApplicationFont(qEnvironmentVariable("SystemRoot") + "/Fonts/msyh.ttc");
#endif
    std::cerr << "initializing Ela\n";
    eApp->init();
    if (app.arguments().contains("--functional")) return functionalChecks() ? 1 : 0;
    auto *client = new ZeroTierClient(nullptr, QCoreApplication::applicationFilePath(), 250);
    MainWindow window(nullptr, client);
    std::cerr << "window constructed\n";
    window.show(); pump(80);
    std::cerr << "window shown\n";
    int failed = 0;
    auto check = [&](bool ok, const char *name) { if (!ok) { ++failed; std::cerr << "FAIL: " << name << '\n'; } };
    check(!window.getWindowButtonFlags().testFlag(ElaAppBarType::RouteBackButtonHint)
          && !window.getWindowButtonFlags().testFlag(ElaAppBarType::RouteForwardButtonHint), "browser navigation buttons hidden");
    bool installed = false;
    InstallWindow installer([&] { return installed; });
    installer.show(); pump(30);
    auto *installArt = installer.findChild<QWidget *>("pageArtwork");
    check(installArt && installArt->property("artLoaded").toBool(), "installation artwork embedded and decoded");
    check(installer.grab().save(QDir::currentPath() + "/ui-install.png"), "capture illustrated installer");
    check(installer.findChildren<QTreeWidget *>().isEmpty(), "installer has no management views");
    installer.findChild<QPushButton *>("recheckInstallation")->click();
    check(installer.isVisible(), "missing installation keeps installer open");
    installed = true;
    installer.findChild<QPushButton *>("recheckInstallation")->click();
    check(installer.result() == QDialog::Accepted && !installer.isVisible(), "installation detected closes installer");
    QList<QWidget *> pages;
    for (auto *w : window.findChildren<QWidget *>()) if (w->property("pageIndex").isValid()) pages << w;
    check(pages.size() == 5, "five management pages");
    for (auto *page : pages) check(!page->property("loaded").toBool(), "management widgets lazy at startup");
    check(window.findChildren<QTreeWidget *>().isEmpty(), "no result models allocated at startup");
    check(window.grab().save(QDir::currentPath() + "/ui-start.png"), "capture start page");
    QWidget *servicePage = nullptr;
    for (auto *page : pages) {
        std::cerr << "opening page " << page->property("pageIndex").toInt() << '\n';
        page->setEnabled(true);
        window.navigation(page->property("ElaPageKey").toString()); pump(80);
        check(page->property("loaded").toBool(), "page populated on navigation");
        if (page->property("command").toString() == "service") servicePage = page;
    }
    check(servicePage != nullptr, "service page found");
    check(servicePage && servicePage->findChild<QLabel *>("resultStatus")->text().contains(QStringLiteral("服务")), "service status available without elevation");
    for (auto *page : pages) if (page->property("pageIndex").toInt() == 1) {
        check(page->findChild<QLineEdit *>("networkIdInput") != nullptr, "network input created");
        window.navigation(page->property("ElaPageKey").toString()); pump(80);
    }
    pump(300);
    check(window.grab().save(QDir::currentPath() + "/ui-network.png"), "capture network page");
    QWidget *startPage = nullptr;
    QWidget *networkPage = nullptr;
    for (auto *label : window.findChildren<QLabel *>()) if (label->text() == QStringLiteral("概览")) startPage = label->parentWidget();
    for (auto *page : pages) if (page->property("pageIndex").toInt() == 1) networkPage = page;
    eTheme->setThemeMode(ElaThemeType::Dark);
    pump(100);
    if (networkPage) {
        auto *art = networkPage->findChild<QWidget *>("pageArtwork");
        check(art && art->property("artResource").toString().endsWith("-dark.jpg") && art->property("artLoaded").toBool(), "dark theme loads dark artwork");
    }
    check(app.palette().color(QPalette::WindowText).lightness() > 180, "dark global text is light");
    for (auto *label : window.findChildren<QLabel *>()) if (label->text() == QStringLiteral("概览"))
        check(label->palette().color(QPalette::WindowText).lightness() > 180, "dark title remains readable");
    auto *log = window.findChild<QPlainTextEdit *>();
    check(log && log->palette().color(QPalette::Text).lightness() > 180, "dark log text is light");
    if (startPage) {
        window.navigation(startPage->property("ElaPageKey").toString()); pump(300);
        check(window.grab().save(QDir::currentPath() + "/ui-dark-start.png"), "capture dark start");
    }
    if (networkPage) {
        window.navigation(networkPage->property("ElaPageKey").toString()); pump(300);
        auto *tree = networkPage->findChild<QTreeWidget *>();
        if (!tree) {
            tree = new QTreeWidget(networkPage);
            networkPage->layout()->addWidget(tree);
        }
        tree->setHeaderLabels({QStringLiteral("网络名称"), QStringLiteral("IP 地址")});
        new QTreeWidgetItem(tree, {QStringLiteral("测试网络"), "10.0.0.2"});
        pump(100);
        check(tree->palette().color(QPalette::Text).lightness() > 180, "dark table text is light");
        check(window.grab().save(QDir::currentPath() + "/ui-dark-network.png"), "capture dark native table");
    }
    eTheme->setThemeMode(ElaThemeType::Light); pump(100);
    if (networkPage) {
        auto *art = networkPage->findChild<QWidget *>("pageArtwork");
        check(art && art->property("artResource").toString().endsWith("-light.jpg") && art->property("artLoaded").toBool(), "light theme loads light artwork");
    }
    check(window.grab().save(QDir::currentPath() + "/ui-light-network.png"), "capture light artwork");
    window.setNavigationBarDisplayMode(ElaNavigationType::Compact); pump(300);
    check(window.grab().save(QDir::currentPath() + "/ui-compact-navigation.png"), "capture collapsed sidebar icons");
    window.setNavigationBarDisplayMode(ElaNavigationType::Maximal); pump(300);
    check(app.palette().color(QPalette::WindowText).lightness() < 80, "light theme text restored");
    if (hadThemeSetting) preferences.setValue("dark", oldThemeSetting); else preferences.remove("dark");
    int calls = 0;
    std::cerr << "testing busy state\n";
    client->run({"--hang"}, [&](bool ok, QByteArray) { check(!ok, "fake hung command terminated"); ++calls; });
    for (auto *button : window.findChildren<QPushButton *>()) if (button->property("managementAction").toBool()) check(!button->isEnabled(), "actions disabled while busy");
    pump(400);
    check(calls == 1 && !client->busy(), "busy state recovered after timeout");
    for (auto *button : window.findChildren<QPushButton *>()) if (button->property("managementAction").toBool()) {
        const bool ready = button->objectName() != "exportDiagnostics";
        check(button->isEnabled() == ready, "actions restored according to available result after completion");
    }
    std::cerr << "closing window\n";
    window.close();
    std::cout << (failed ? "UI checks failed" : "UI checks passed") << '\n';
    return failed ? 1 : 0;
}
