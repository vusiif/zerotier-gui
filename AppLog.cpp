#include "AppLog.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

QString AppLog::path()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath("zerotier-gui.log");
}

void AppLog::write(const QString &text)
{
    constexpr qint64 limit = 1024 * 1024;
    const auto fileName = path();
    if (!QDir().mkpath(QFileInfo(fileName).absolutePath())) return;
    QByteArray entry = (QDateTime::currentDateTime().toString(Qt::ISODate) + ' ' + text.trimmed() + '\n').toUtf8();
    if (entry.size() > limit / 2) entry = entry.left(limit / 2 - 32) + "\n[truncated]\n";
    QFile current(fileName);
    if (current.size() + entry.size() > limit) {
        QByteArray tail;
        if (current.open(QIODevice::ReadOnly)) {
            current.seek(qMax(qint64(0), current.size() - limit / 2));
            tail = current.read(limit / 2);
            const auto newline = tail.indexOf('\n');
            if (newline >= 0) tail.remove(0, newline + 1);
            current.close();
        }
        QSaveFile replacement(fileName);
        if (replacement.open(QIODevice::WriteOnly)) {
            replacement.write(tail);
            replacement.write(entry);
            replacement.commit();
        }
    } else if (current.open(QIODevice::WriteOnly | QIODevice::Append)) {
        current.write(entry);
    }
}
