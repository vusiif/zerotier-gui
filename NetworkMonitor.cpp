#include "NetworkMonitor.h"
#include "ManagementClient.h"
#include <QTimer>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>

NetworkMonitor::NetworkMonitor(ZeroTierClient *client, QObject *parent,
                               std::function<bool()> allowed, int intervalMs)
    : QObject(parent), m_client(client), m_timer(new QTimer(this)),
      m_allowed(allowed ? std::move(allowed) : [] { return true; }), m_intervalMs(intervalMs)
{
    m_clock.start();
    m_timer->setInterval(intervalMs);
    connect(m_timer, &QTimer::timeout, this, &NetworkMonitor::poll);
}
void NetworkMonitor::track(const QString &value)
{
    const auto id = value.trimmed().toLower();
    if (!QRegularExpression("^[0-9a-f]{16}$").match(id).hasMatch()) return;
    m_entries.insert(id, Entry{++m_generation, QStringLiteral("REQUESTING_CONFIGURATION")});
    emit statusChanged(id, "REQUESTING_CONFIGURATION");
    m_timer->start();
    QTimer::singleShot(0, this, &NetworkMonitor::poll);
}
void NetworkMonitor::forget(const QString &id)
{
    m_entries.remove(id.trimmed().toLower());
    if (m_entries.isEmpty()) m_timer->stop();
}
bool NetworkMonitor::tracks(const QString &id) const
{
    return m_entries.contains(id.trimmed().toLower());
}
void NetworkMonitor::poll()
{
    if (m_entries.isEmpty() || m_inFlight || m_client->busy() || !m_allowed()) return;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (!it->leavePending || it->attempts >= 3 || m_clock.elapsed() < it->retryAt) continue;
        const auto id = it.key(); const auto generation = it->generation;
        ++it->attempts;
        m_inFlight = true;
        m_client->run({"leave", id}, [this, id, generation](bool ok, QByteArray) {
            m_inFlight = false;
            auto current = m_entries.find(id);
            if (current == m_entries.end() || current->generation != generation) return;
            if (ok) {
                forget(id);
                emit diagnostic(QStringLiteral("网络 %1 不存在（NOT_FOUND），已自动退出。\n").arg(id));
                emit automaticallyLeft(id);
            } else {
                current->leavePending = false;
                current->retryAt = m_clock.elapsed() + m_intervalMs;
                const auto reason = m_client->lastError().isEmpty() ? QStringLiteral("命令未成功执行") : m_client->lastError();
                const auto message = QStringLiteral("网络 %1 自动退出失败（%2/3）：%3%4").arg(id).arg(current->attempts)
                                .arg(reason, current->attempts >= 3 ? QStringLiteral("；请手动退出或重新加入。") : QStringLiteral("；稍后重试。"));
                emit diagnostic(message + '\n');
                emit automaticLeaveFailed(id, message);
            }
        });
        return;
    }
    QHash<QString, quint64> snapshot;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) snapshot.insert(it.key(), it->generation);
    m_inFlight = true;
    m_client->run({"-j", "listnetworks"}, [this, snapshot](bool ok, QByteArray bytes) {
        m_inFlight = false;
        const auto doc = QJsonDocument::fromJson(bytes);
        if (!ok || !doc.isArray()) {
            emit diagnostic(QStringLiteral("网络跟踪查询失败，将重试：%1\n").arg(ok ? QStringLiteral("无效 JSON 数组") : m_client->lastError()));
            return;
        }
        bool leaveReady = false;
        for (const auto &value : doc.array()) {
            const auto object = value.toObject();
            const auto id = object.value("nwid").toString(object.value("id").toString()).toLower();
            auto current = m_entries.find(id);
            if (!snapshot.contains(id) || current == m_entries.end() || current->generation != snapshot.value(id)) continue;
            const auto status = object.value("status").toString();
            if (status.isEmpty()) continue;
            const bool changed = current->status != status;
            current->status = status;
            // Only an explicit NOT_FOUND for a tracked network can trigger leave.
            current->leavePending = status == "NOT_FOUND";
            leaveReady |= current->leavePending && current->attempts < 3 && m_clock.elapsed() >= current->retryAt;
            if (changed) emit statusChanged(id, status);
        }
        if (leaveReady) QTimer::singleShot(0, this, &NetworkMonitor::poll);
    });
}
