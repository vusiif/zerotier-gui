#include "DiagnosticReport.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *name) { if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } };
    QTemporaryDir directory;
    const auto source = directory.filePath("zerotier_dump.txt");
    const QByteArray report = "platform: Windows\nstatus\n{\"name\":\"测试\"}\n";
    auto write = [&](QByteArray bytes) { QFile file(source); check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "fixture writable"); file.write(bytes); };
    const auto started = QDateTime::currentMSecsSinceEpoch();
    write(report);
    const auto message = "Writing dump to: " + source.toUtf8() + "\r\n";
    auto result = DiagnosticReport::read(message, started);
    check(result.error.isEmpty() && result.text.contains(QStringLiteral("测试")), "Windows output path resolves full Unicode report");
    check(DiagnosticReport::read(report, started).text == QString::fromUtf8(report).trimmed(), "inline report supported");
    check(DiagnosticReport::read(message + report, started).text == QString::fromUtf8(report).trimmed(), "Windows write failure inline fallback preferred");
    check(!DiagnosticReport::read(message, started + 10000).error.isEmpty(), "stale report rejected");
    for (const auto &bad : {QByteArray{}, QByteArray("200 dump OK"), QByteArray("Writing dump to: missing.txt"),
        QByteArray("Writing dump to: ../zerotier_dump.txt"), QByteArray("Writing dump to: ") + directory.filePath("other.txt").toUtf8()})
        check(!DiagnosticReport::read(bad, started).error.isEmpty(), "invalid or unsupported output cannot become report");
    write({}); check(!DiagnosticReport::read(message, started).error.isEmpty(), "empty report rejected");
    write("unexpected output"); check(!DiagnosticReport::read(message, started).error.isEmpty(), "wrong report header rejected");
    write(QByteArray(4 * 1024 * 1024 + 1, 'x')); check(!DiagnosticReport::read(message, started).error.isEmpty(), "oversized report rejected");
    QString error;
    const auto destination = directory.filePath("导出报告.txt");
    check(DiagnosticReport::save(destination, QString::fromUtf8(report), &error), "UTF8 export succeeds");
    QFile saved(destination); check(saved.open(QIODevice::ReadOnly) && saved.readAll() == report, "export preserves complete report bytes");
    check(!DiagnosticReport::save(directory.filePath("missing/export.txt"), "test", &error) && !error.isEmpty(), "save failure supplies error");
    std::cout << "diagnostic report failures=" << failures << '\n'; return failures ? 1 : 0;
}
