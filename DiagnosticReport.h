#pragma once
#include <QByteArray>
#include <QString>
namespace DiagnosticReport {
struct Result { QString text; QString error; };
// Windows CLI normally writes to zerotier_dump.txt; other builds can print the report.
Result read(const QByteArray &output, qint64 startedAtMs);
bool save(const QString &path, const QString &text, QString *error);
}
