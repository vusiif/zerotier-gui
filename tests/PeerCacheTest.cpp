#include "PeerCacheMaintenance.h"
#include "ServiceControl.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
static void pump(int ms = 30) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
class FixtureControl : public ServiceControl {
public:
    QString state = "running"; QStringList actions;
    bool stopOK = true, startOK = true, stopChangesState = true, startChangesState = true;
    std::function<void()> afterStop;
    void run(const QString &action, std::function<void(bool, QString)> callback) override {
        actions << action; const bool ok = action == "stop" ? stopOK : startOK;
        if (action == "stop" && stopChangesState) state = "stopped";
        if (action == "start" && startOK && startChangesState) state = "running";
        if (action == "stop" && afterStop) afterStop();
        QTimer::singleShot(0, this, [callback, ok] { callback(ok, ok ? QString{} : QString("fixture failure")); });
    }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *name) { if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } };
    auto prepare = [&](const QString &home) {
        QDir().mkpath(home + "/peers.d");
        for (const auto &name : {"peers.d/cache", "identity.secret", "networks.dummy", "moon.dummy"}) {
            QFile file(QDir(home).filePath(name)); check(file.open(QIODevice::WriteOnly), "fixture writable"); file.write("preserved");
        }
    };
    for (int mode = 0; mode < 8; ++mode) {
        QTemporaryDir directory; prepare(directory.path()); FixtureControl control;
        if (mode == 1) { control.stopOK = false; control.stopChangesState = false; }
        if (mode == 2) control.startOK = false;
        if (mode == 3) control.startChangesState = false;
        if (mode == 4) control.stopOK = false;
        if (mode == 5) control.afterStop = [&] { QDir(directory.path()).rename("peers.d", "external-backup"); QFile f(directory.filePath("peers.d")); f.open(QIODevice::WriteOnly); };
        if (mode == 6 || mode == 7) control.afterStop = [&] { control.state = "pending"; };
        if (mode == 7) control.startOK = false;
        PeerCacheMaintenance maintenance(&control, nullptr, [&] { return control.state; });
        int callbacks = 0; bool succeeded = false; QString message;
        maintenance.run(directory.path(), [&](bool ok, QString text) { ++callbacks; succeeded = ok; message = text; });
        if (mode == 0) maintenance.run(directory.path(), [&](bool ok, QString) { check(!ok, "concurrent maintenance rejected"); });
        pump();
        check(callbacks == 1 && succeeded == (mode == 0), "workflow completes once with verified result");
        check(QFile::exists(directory.filePath("identity.secret")) && QFile::exists(directory.filePath("networks.dummy")) && QFile::exists(directory.filePath("moon.dummy")), "identity networks and moon retained");
        const auto backups = QDir(directory.path()).entryList({"peers.d.backup-*"}, QDir::Dirs);
        if (mode == 0 || mode == 2 || mode == 3) {
            check(backups.size() == 1 && QFile::exists(directory.filePath(backups.first() + "/cache")), "cleared cache backed up intact");
            check(control.actions == QStringList{"stop", "start"}, "stop before move and start afterwards");
        }
        if (mode == 1 || mode == 4 || mode == 6 || mode == 7) check(QFile::exists(directory.filePath("peers.d/cache")) && backups.isEmpty(), "stop failure never clears cache");
        if (mode == 4 || mode == 5 || mode == 6) check(control.state == "running", "operation failure restores service when stopped or pending");
        if (mode == 7) check(message.contains(QStringLiteral("无法确认服务恢复")), "failed uncertain-state recovery explicit");
        if (mode == 2 || mode == 3) check(message.contains(QStringLiteral("未恢复")), "restart failure explicit with backup location");
    }
    {
        QTemporaryDir directory; FixtureControl control; PeerCacheMaintenance maintenance(&control, nullptr, [&] { return control.state; });
        maintenance.run(directory.path(), [&](bool ok, QString) { check(ok, "missing cache no-op"); });
        check(control.actions.isEmpty(), "missing cache never stops service");
        maintenance.run("relative", [&](bool ok, QString) { check(!ok, "relative home rejected"); });
        check(!PeerCacheMaintenance::validate(QDir::rootPath()).isEmpty(), "filesystem root rejected");
        QFile file(directory.filePath("peers.d")); file.open(QIODevice::WriteOnly); file.close();
        check(!PeerCacheMaintenance::validate(directory.path()).isEmpty(), "cache file cannot be renamed as directory");
    }
    std::cout << "cache failures=" << failures << "\n"; return failures ? 1 : 0;
}
