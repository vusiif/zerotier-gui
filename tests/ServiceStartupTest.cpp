#include "ServiceStartup.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *name) { if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } };
    auto scenario = [&](QStringList states, QString error, bool expected, int startsExpected, const QString &reason) {
        int probes = 0, starts = 0, callbacks = 0;
        QEventLoop loop;
        ServiceStartup startup(nullptr, [&] { return states[qMin(probes++, states.size() - 1)]; },
                               [&] { ++starts; return error; }, 40, 2);
        QTimer::singleShot(500, &loop, &QEventLoop::quit);
        startup.ensureRunning([&](bool ok, QString message) {
            ++callbacks; check(ok == expected, "correct completion result");
            check(reason.isEmpty() || message.contains(reason), "failure reason retained");
            QTimer::singleShot(4, &loop, &QEventLoop::quit);
        });
        if (startup.busy()) startup.ensureRunning([&](bool ok, QString message) { check(!ok && !message.isEmpty(), "concurrent startup rejected"); });
        loop.exec();
        check(callbacks == 1 && !startup.busy(), "one callback and busy cleared");
        check(starts == startsExpected, "start issued only when needed");
    };
    scenario({"running"}, {}, true, 0, {});
    scenario({"stopped", "pending", "running"}, {}, true, 1, {});
    scenario({"pending", "running"}, {}, true, 0, {});
    scenario({"pending", "stopped", "pending", "running"}, {}, true, 1, {});
    scenario({"missing"}, {}, false, 0, QStringLiteral("未安装"));
    scenario({"unknown"}, {}, false, 0, QStringLiteral("权限"));
    scenario({"stopped"}, QStringLiteral("错误 5"), false, 1, "5");
    scenario({"stopped", "stopped"}, {}, false, 1, QStringLiteral("又停止"));
    scenario({"pending"}, {}, false, 0, QStringLiteral("超时"));
    scenario({"stopped", "pending"}, {}, false, 1, QStringLiteral("超时"));
    std::cout << (failures ? "Service startup checks failed" : "Service startup checks passed") << '\n';
    return failures ? 1 : 0;
}
