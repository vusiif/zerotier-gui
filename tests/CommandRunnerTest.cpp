#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <iostream>
#include "ZeroTierClient.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.contains("--child")) {
        const auto mode = args.last();
        if (mode == "ok") { std::cout << "{\"online\":true}"; return 0; }
        if (mode == "help") { std::cerr << "Available switches: -h -v -j"; return 0; }
        if (mode == "fail") { std::cerr << "permission denied"; return 5; }
        if (mode == "big") { std::cout << std::string(2 * 1024 * 1024, 'x'); return 0; }
        if (mode == "hang") { QTimer::singleShot(10000, &app, &QCoreApplication::quit); return app.exec(); }
    }
    int failures = 0;
    auto check = [&failures](bool condition, const char *name) {
        if (!condition) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    check(ZeroTierClient::nativeArguments("C:/ZeroTier/zerotier-one_x64.exe", {"-j", "info"}) == QStringList{"-q", "-j", "info"}, "Windows daemon CLI prefix");
    check(ZeroTierClient::nativeArguments("C:/ZeroTier/zerotier-cli.exe", {"info"}) == QStringList{"info"}, "Standalone CLI arguments");
    auto run = [&](const QString &program, const QString &mode, bool expected, const QByteArray &contains, int timeout) {
        ZeroTierClient client(nullptr, program, timeout);
        QEventLoop loop;
        int callbacks = 0;
        QTimer watchdog; watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        watchdog.start(5000);
        client.run({"--child", mode}, [&](bool ok, QByteArray bytes) {
            ++callbacks;
            check(ok == expected, qPrintable(mode + " result"));
            check(contains.isEmpty() || bytes.contains(contains), qPrintable(mode + " output"));
            if (mode == "hang") check(client.lastError().contains(QStringLiteral("超时")), "timeout reason retained");
            if (mode == "big") check(client.lastError().contains("1MB"), "output limit reason retained");
            QTimer::singleShot(10, &loop, &QEventLoop::quit);
        });
        if (mode == "hang") {
            check(client.busy(), "busy while running");
            client.run({"--child", "ok"}, [&](bool ok, QByteArray bytes) {
                check(!ok && !bytes.isEmpty(), "busy request rejected with reason");
            });
        }
        loop.exec();
        check(callbacks == 1, qPrintable(mode + " exactly one callback"));
        check(!client.busy(), qPrintable(mode + " clears busy"));
    };
    const auto self = QCoreApplication::applicationFilePath();
    run(self, "ok", true, "online", 2000);
    run(self, "help", true, "Available switches", 2000);
    run(self, "fail", false, "permission denied", 2000);
    run(self, "big", false, {}, 2000);
    run(self, "hang", false, {}, 100);
    run(self + ".missing", "missing", false, {}, 1000);
    {
        ZeroTierClient reusable(nullptr, self, 1000);
        QEventLoop loop;
        int callbacks = 0;
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        reusable.run({"--child", "fail"}, [&](bool ok, QByteArray) {
            check(!ok, "first reusable command failed"); ++callbacks;
            reusable.run({"--child", "ok"}, [&](bool nextOk, QByteArray bytes) {
                check(nextOk && reusable.lastError().isEmpty() && !bytes.contains("denied"), "reuse clears previous error and output");
                ++callbacks; loop.quit();
            });
        });
        loop.exec();
        check(callbacks == 2 && !reusable.busy(), "callback can start next command");
    }
    std::cout << (failures ? "FAILED" : "All command runner checks passed") << '\n';
    return failures ? 1 : 0;
}
