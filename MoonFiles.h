#pragma once
#include <QByteArray>
#include <QString>
namespace MoonFiles {
QString homeDirectory(const QByteArray &info, QString *error);
QString importFile(const QString &source, const QString &home, bool overwrite);
int count(const QString &home);
}
