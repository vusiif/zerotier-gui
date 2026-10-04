#include "MoonFiles.h"
#include "ServiceControl.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *name) { if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } };
    QTemporaryDir directory;
    QString error;
    const auto home = directory.filePath(QStringLiteral("服务 数据目录")); QDir().mkpath(home);
    const auto info = QJsonDocument(QJsonObject{{"config", QJsonObject{{"settings", QJsonObject{{"homeDir", home}}}}}}).toJson();
    check(MoonFiles::homeDirectory(info, &error) == home && error.isEmpty(), "Unicode spaced homeDir parsed");
    for (auto value : {QByteArray("bad"), QByteArray("[]"), QByteArray("{}"), QByteArray("{\"config\":{\"settings\":{\"homeDir\":\"relative\"}}}")})
        check(MoonFiles::homeDirectory(value, &error).isEmpty() && !error.isEmpty(), "invalid homeDir rejected");
    const auto source = directory.filePath("sample.moon");
    { QFile f(source); check(f.open(QIODevice::WriteOnly), "source writable"); f.write("first"); }
    check(MoonFiles::importFile(source, home, false).isEmpty(), "Moon copy succeeds");
    check(MoonFiles::count(home) == 1, "local Moon counted");
    { QFile f(source); check(f.open(QIODevice::WriteOnly), "source rewritten"); f.write("second"); }
    check(!MoonFiles::importFile(source, home, false).isEmpty(), "overwrite requires consent");
    check(MoonFiles::importFile(source, home, true).isEmpty(), "explicit overwrite succeeds");
    QFile imported(home + "/moons.d/sample.moon");
    check(imported.open(QIODevice::ReadOnly) && imported.readAll() == "second", "overwrite preserves exact new bytes");
    check(!MoonFiles::importFile(source, "relative", true).isEmpty(), "relative destination rejected");
    check(!MoonFiles::importFile(directory.filePath("missing.moon"), home, true).isEmpty(), "missing file rejected");
    const auto wrong = directory.filePath("wrong.txt");
    { QFile f(wrong); check(f.open(QIODevice::WriteOnly), "extension fixture writable"); f.write("test"); }
    check(!MoonFiles::importFile(wrong, home, true).isEmpty(), "wrong extension rejected");
    { QFile f(home + "/moons.d/readme.txt"); check(f.open(QIODevice::WriteOnly), "non-Moon fixture writable"); f.write("test"); }
    check(MoonFiles::count(home) == 1, "unrelated files excluded from Moon count");
    check(ServiceControl::script("unknown").isEmpty(), "unknown service action rejected");
#ifdef Q_OS_WIN
    auto run = [&](const QString &action, const QString &mode, bool success, const QByteArray &expectedTrace) {
        const auto trace = directory.filePath(action + mode + ".txt");
        auto quoted = trace; quoted.replace("'", "''");
        const auto mocks = QString(
            "$global:state='Running'; $global:mode='%1'; $global:trace='%2'; "
            "function Get-Service { param($Name,$ErrorAction); $o=[pscustomobject]@{Status=$global:state}; "
            "$o | Add-Member ScriptMethod WaitForStatus { param($status,$timeout); "
            "if ($global:mode -eq 'pendingPipe' -and $status -eq 'Stopped') { $global:state='Stopped' }; "
            "$this.Status=$global:state; if ($this.Status -ne $status) { throw 'wrong state' } }; return $o }; "
            "function Stop-Service { param($Name,$ErrorAction); Add-Content $global:trace 'stop'; "
            "if ($global:mode -eq 'denied') { throw 'access denied' }; "
            "if ($global:mode -eq 'pendingPipe') { $global:state='StopPending'; throw '109' }; "
            "$global:state='Stopped'; if ($global:mode -eq 'pipe') { throw '109' } }; "
            "function Start-Service { param($Name,$ErrorAction); Add-Content $global:trace 'start'; "
            "if ($global:mode -eq 'startFailed') { throw 'start failed' }; $global:state='Running' }; ").arg(mode, quoted);
        QProcess process;
        process.start(qEnvironmentVariable("SystemRoot") + "/System32/WindowsPowerShell/v1.0/powershell.exe",
                      {"-NoProfile", "-NonInteractive", "-Command", mocks + ServiceControl::script(action)});
        const bool finished = process.waitForFinished(10000);
        check(finished && process.exitStatus() == QProcess::NormalExit && (process.exitCode() == 0) == success, "service script verifies final state");
        if (!finished) { process.kill(); process.waitForFinished(1000); }
        QFile f(trace); check(f.open(QIODevice::ReadOnly) && f.readAll().replace("\r", "") == expectedTrace, "service action order and failure isolation");
    };
    run("start", "normal", true, "start\n");
    run("stop", "normal", true, "stop\n");
    run("restart", "normal", true, "stop\nstart\n");
    run("restart", "pipe", true, "stop\nstart\n");
    run("restart", "pendingPipe", true, "stop\nstart\n");
    run("restart", "denied", false, "stop\n");
    run("restart", "startFailed", false, "stop\nstart\n");
#endif
    std::cout << (failures ? "Moon/service checks failed" : "Moon/service checks passed") << '\n';
    return failures ? 1 : 0;
}
