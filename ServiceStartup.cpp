#include "ServiceStartup.h"
#include "ZeroTierClient.h"
#include <QTimer>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

static QString startInstalledService()
{
#ifdef Q_OS_WIN
    const auto manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) return QStringLiteral("无法访问服务管理器（错误 %1）。").arg(GetLastError());
    const auto service = OpenServiceW(manager, L"ZeroTierOneService", SERVICE_START);
    const auto openError = GetLastError();
    CloseServiceHandle(manager);
    if (!service) return QStringLiteral("无法打开 ZeroTier 服务（错误 %1）。").arg(openError);
    const bool ok = StartServiceW(service, 0, nullptr);
    const auto error = GetLastError();
    CloseServiceHandle(service);
    if (ok || error == ERROR_SERVICE_ALREADY_RUNNING) return {};
    return QStringLiteral("ZeroTier 服务启动失败（错误 %1），请检查服务配置及管理员权限。").arg(error);
#else
    return QStringLiteral("此平台尚未实现自动启动服务。");
#endif
}

ServiceStartup::ServiceStartup(QObject *parent, Probe probe, Starter starter, int timeoutMs, int intervalMs)
    : QObject(parent), m_probe(probe ? std::move(probe) : ZeroTierClient::serviceStatus),
      m_starter(starter ? starter : startInstalledService), m_timer(new QTimer(this)), m_timeoutMs(timeoutMs), m_nativeStarter(!starter)
{
    m_timer->setInterval(intervalMs);
    connect(m_timer, &QTimer::timeout, this, &ServiceStartup::poll);
}
void ServiceStartup::ensureRunning(std::function<void(bool, QString)> callback)
{
    if (busy()) { callback(false, QStringLiteral("服务启动检查正在进行。")); return; }
    m_callback = std::move(callback);
    ++m_attempt;
    m_requested = false;
    m_starting = false;
    m_elapsed.start();
    emit busyChanged(true);
    m_timer->start();
    poll();
}
void ServiceStartup::poll()
{
    const auto status = m_probe();
    if (status == "running") { complete(true); return; }
    if (status == "missing") { complete(false, QStringLiteral("ZeroTier 服务未安装，请重新安装 ZeroTier。")); return; }
    if (status == "unknown") { complete(false, QStringLiteral("无法查询 ZeroTier 服务状态，请检查权限。")); return; }
    if (m_elapsed.elapsed() >= m_timeoutMs) {
        complete(false, QStringLiteral("等待 ZeroTier 服务启动超时，请在服务页检查状态；服务可能仍在启动。")); return;
    }
    if (m_starting) return;
    if (status == "stopped") {
        if (m_requested) { complete(false, QStringLiteral("ZeroTier 服务启动后又停止，请检查服务日志。")); return; }
        m_requested = true;
        if (m_nativeStarter) {
            // StartService can wait on SCM; keep that wait off the GUI thread.
            m_starting = true;
            auto *watcher = new QFutureWatcher<QString>(this);
            const auto attempt = m_attempt;
            connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, attempt] {
                const auto error = watcher->result(); watcher->deleteLater();
                if (!busy() || attempt != m_attempt) return;
                m_starting = false;
                if (!error.isEmpty()) complete(false, error); else poll();
            });
            watcher->setFuture(QtConcurrent::run(m_starter));
            return;
        }
        const auto error = m_starter();
        if (!error.isEmpty()) complete(false, error);
    }
    // Pending services are polled without blocking the GUI or issuing repeated starts.
}
void ServiceStartup::complete(bool ok, const QString &error)
{
    m_timer->stop();
    auto callback = std::move(m_callback); m_callback = {};
    emit busyChanged(false);
    callback(ok, error);
}
