#include "PeerCacheMaintenance.h"
#include "ServiceControl.h"
#include "ZeroTierClient.h"
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QUuid>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

static bool redirected(const QString &path)
{
    if (QFileInfo(path).isSymLink()) return true;
#ifdef Q_OS_WIN
    const auto attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return false;
#endif
}
PeerCacheMaintenance::PeerCacheMaintenance(ServiceControl *control, QObject *parent, Probe probe)
    : QObject(parent), m_control(control), m_probe(probe ? std::move(probe) : Probe(ZeroTierClient::serviceStatus)) {}

QString PeerCacheMaintenance::validate(const QString &home)
{
    const QFileInfo root(home);
    if (!QDir::isAbsolutePath(home) || !root.isDir() || QDir(home).isRoot() || redirected(home))
        return QStringLiteral("服务目录无效或指向链接，未清理缓存。");
    const auto cache = QDir(home).filePath("peers.d");
    const QFileInfo info(cache);
    if (redirected(cache) || (info.exists() && !info.isDir())) return QStringLiteral("peers.d 不是普通目录，未清理缓存。");
    if (info.exists() && QDir(root.canonicalFilePath()).filePath("peers.d") != info.canonicalFilePath())
        return QStringLiteral("缓存目录超出服务数据目录，未清理。");
    return {};
}
QString PeerCacheMaintenance::moveAside(const QString &home, QString *backup)
{
    backup->clear();
    const auto error = validate(home); if (!error.isEmpty()) return error;
    QDir root(QFileInfo(home).canonicalFilePath());
    if (!QFileInfo::exists(root.filePath("peers.d"))) return {};
    const auto name = "peers.d.backup-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    // Rename within the verified parent. Never traverse or delete cache contents.
    if (!root.rename("peers.d", name)) return QStringLiteral("缓存改名备份失败，原目录保留。");
    *backup = root.filePath(name); return {};
}
void PeerCacheMaintenance::run(const QString &home, std::function<void(bool, QString)> callback)
{
    if (m_callback) { callback(false, QStringLiteral("缓存维护正在进行。")); return; }
    const auto error = validate(home); if (!error.isEmpty()) { callback(false, error); return; }
    if (!QFileInfo::exists(QDir(home).filePath("peers.d"))) { callback(true, QStringLiteral("没有 Peers 缓存，无需清理。")); return; }
    if (m_probe() != "running") { callback(false, QStringLiteral("请在服务运行时清理缓存；当前状态不确定或已停止。")); return; }
    m_callback = std::move(callback);
    QPointer<PeerCacheMaintenance> guard(this);
    m_control->run("stop", [guard, home](bool ok, QString reason) {
        if (!guard) return;
        const auto state = guard->m_probe();
        if (state != "stopped") {
            const auto message = QStringLiteral("未确认服务停止，未移动缓存。%1").arg(reason);
            if (state == "running") { guard->finish(false, message + QStringLiteral(" 服务仍在运行。")); return; }
            // Original state was Running. Try to restore it even if the stop/query failed.
            guard->m_control->run("start", [guard, message](bool restored, QString error) {
                if (!guard) return;
                guard->finish(false, message + (restored && guard->m_probe() == "running"
                    ? QStringLiteral(" 服务已恢复运行。") : QStringLiteral(" 无法确认服务恢复：%1，请检查实际状态。").arg(error)));
            });
            return;
        }
        QString backup;
        const auto error = ok ? moveAside(home, &backup) : (reason.isEmpty() ? QStringLiteral("停止服务失败，未移动缓存。") : reason);
        guard->m_control->run("start", [guard, error, backup](bool started, QString startError) {
            if (!guard) return;
            if (!started || guard->m_probe() != "running") {
                guard->finish(false, QStringLiteral("服务未恢复运行：%1 %2\n缓存备份：%3").arg(startError, error, backup.isEmpty() ? QStringLiteral("未移动") : backup)); return;
            }
            if (!error.isEmpty()) { guard->finish(false, error + QStringLiteral(" 服务已恢复运行。")); return; }
            guard->finish(true, backup.isEmpty() ? QStringLiteral("缓存不存在，服务已恢复运行。") : QStringLiteral("缓存已清理，服务已恢复运行。备份保留在：\n%1").arg(backup));
        });
    });
}
void PeerCacheMaintenance::finish(bool ok, const QString &message)
{
    auto callback = std::move(m_callback); m_callback = {};
    if (callback) callback(ok, message);
}
