#include "MoonFiles.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

QString MoonFiles::homeDirectory(const QByteArray &info, QString *error)
{
    const auto doc = QJsonDocument::fromJson(info);
    const auto path = doc.object().value("config").toObject().value("settings").toObject().value("homeDir").toString();
    if (!doc.isObject() || path.trimmed().isEmpty() || !QDir::isAbsolutePath(path) || !QFileInfo(path).isDir()) {
        *error = QStringLiteral("无法从节点 info 获取有效的 homeDir，未导入文件。请检查服务与数据目录。");
        return {};
    }
    error->clear();
    return QDir::cleanPath(path);
}
QString MoonFiles::importFile(const QString &sourcePath, const QString &home, bool overwrite)
{
    if (!QDir::isAbsolutePath(home) || !QFileInfo(home).isDir()) return QStringLiteral("服务数据目录无效。");
    if (QFileInfo(sourcePath).suffix().compare("moon", Qt::CaseInsensitive) != 0) return QStringLiteral("请选择 .moon 文件。");
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly) || source.size() == 0 || source.size() > 1024 * 1024)
        return QStringLiteral("文件无法读取或大小无效（需要 1 字节至 1MB）。");
    const auto bytes = source.read(1024 * 1024 + 1);
    if (source.error() != QFile::NoError || bytes.isEmpty() || bytes.size() > 1024 * 1024) return QStringLiteral("Moon 文件读取失败或超过 1MB。");
    const auto root = QDir(home).filePath("moons.d");
    const QFileInfo rootInfo(root);
    if (rootInfo.isSymLink() || rootInfo.isJunction() || (rootInfo.exists() && !rootInfo.isDir()))
        return QStringLiteral("moons.d 不是普通目录，未导入文件。");
    if (!QDir().mkpath(root)) return QStringLiteral("无法创建 moons.d，请检查目录权限。");
    const auto destination = QDir(root).filePath(QFileInfo(sourcePath).fileName());
    if (QFileInfo(destination).isSymLink() || QFileInfo(destination).isJunction())
        return QStringLiteral("目标文件是链接，未覆盖。");
    if (QFile::exists(destination) && !overwrite) return QStringLiteral("已取消覆盖，原文件保留。");
    QSaveFile target(destination);
    if (!target.open(QIODevice::WriteOnly) || target.write(bytes) != bytes.size() || !target.commit()) return target.errorString();
    return {};
}
int MoonFiles::count(const QString &home)
{
    return QDir(QDir(home).filePath("moons.d")).entryList({"*.moon"}, QDir::Files | QDir::NoSymLinks).size();
}
