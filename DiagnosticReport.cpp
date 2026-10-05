#include "DiagnosticReport.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>

DiagnosticReport::Result DiagnosticReport::read(const QByteArray &output, qint64 startedAtMs)
{
    QString text = QString::fromUtf8(output);
    if (text.contains(QChar::ReplacementCharacter)) text = QString::fromLocal8Bit(output);
    text = text.trimmed();
    // If writing to the desktop fails, the CLI may print the full report after the path.
    const int inlineStart = text.indexOf("platform:");
    if (inlineStart == 0 || (inlineStart > 0 && text[inlineStart - 1] == '\n'))
        return {text.mid(inlineStart), {}};
    QString path;
    for (const auto &line : text.split('\n'))
        if (line.trimmed().startsWith("Writing dump to: ")) path = line.trimmed().mid(17).trimmed();
    const QFileInfo info(path);
    if (path.isEmpty() || !QDir::isAbsolutePath(path) || info.fileName() != "zerotier_dump.txt"
        || !info.isFile() || info.isSymLink()) return {{}, QStringLiteral("CLI 没有返回可读取的诊断报告。")};
    if (info.lastModified().toMSecsSinceEpoch() < startedAtMs - 2000)
        return {{}, QStringLiteral("诊断文件没有在本次操作中更新，未使用旧报告。")};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {{}, QStringLiteral("无法读取报告：%1").arg(file.errorString())};
    const auto data = file.read(4 * 1024 * 1024 + 1);
    if (data.size() > 4 * 1024 * 1024) return {{}, QStringLiteral("诊断报告超过 4MB，未加载。")};
    const auto report = QString::fromUtf8(data).trimmed();
    if (!report.startsWith("platform:")) return {{}, QStringLiteral("诊断文件为空或格式不匹配。")};
    return {report, {}};
}

bool DiagnosticReport::save(const QString &path, const QString &text, QString *error)
{
    QSaveFile file(path);
    const auto bytes = text.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString(); return false;
    }
    return true;
}
