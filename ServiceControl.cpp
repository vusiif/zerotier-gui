#include "ServiceControl.h"
#include <QProcess>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

ServiceControl::ServiceControl(QObject *parent) : QObject(parent), m_process(new QProcess(this)), m_timeout(new QTimer(this))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] { m_timedOut = true; m_process->kill(); });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        const auto bytes = m_process->readAllStandardOutput();
        m_output += bytes.left(qMax(0, 8192 - int(m_output.size())));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete(false, m_process->errorString());
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        complete(!m_timedOut && code == 0 && status == QProcess::NormalExit,
                 m_timedOut ? QStringLiteral("服务操作超时，请检查实际服务状态。") : QString::fromUtf8(m_output));
    });
}
ServiceControl::~ServiceControl()
{
    m_callback = {};
    m_timeout->stop();
    m_process->disconnect(this);
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

QString ServiceControl::script(const QString &action)
{
    if (action != "start" && action != "stop" && action != "restart") return {};
    QString body;
    if (action != "start") body +=
        "try { Stop-Service -Name ZeroTierOneService -ErrorAction Stop } catch { "
        "$s=Get-Service -Name ZeroTierOneService -ErrorAction Stop; "
        "if ($s.Status -eq 'StopPending') { $s.WaitForStatus('Stopped',[TimeSpan]::FromSeconds(30)) }; "
        "if ($s.Status -ne 'Stopped') { throw } }; "
        "$s=Get-Service -Name ZeroTierOneService -ErrorAction Stop; $s.WaitForStatus('Stopped',[TimeSpan]::FromSeconds(30)); ";
    if (action != "stop") body +=
        "Start-Service -Name ZeroTierOneService -ErrorAction Stop; "
        "$s=Get-Service -Name ZeroTierOneService -ErrorAction Stop; $s.WaitForStatus('Running',[TimeSpan]::FromSeconds(30)); ";
    return "[Console]::OutputEncoding=[Text.UTF8Encoding]::new(); try { " + body +
           "exit 0 } catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }";
}
void ServiceControl::run(const QString &action, std::function<void(bool, QString)> callback)
{
    if (m_callback) { callback(false, QStringLiteral("已有服务操作正在进行。")); return; }
    const auto command = script(action);
    if (command.isEmpty()) { callback(false, QStringLiteral("无效的服务操作。")); return; }
    m_callback = std::move(callback); m_output.clear(); m_timedOut = false;
    m_timeout->start(120000);
    const auto powershell = qEnvironmentVariable("SystemRoot") + "/System32/WindowsPowerShell/v1.0/powershell.exe";
    m_process->start(powershell, {"-NoProfile", "-NonInteractive", "-Command", command});
}
void ServiceControl::complete(bool ok, const QString &reason)
{
    if (!m_callback) return;
    m_timeout->stop();
    auto callback = std::move(m_callback); m_callback = {};
    callback(ok, ok ? QString{} : (reason.trimmed().isEmpty() ? QStringLiteral("服务操作失败，请检查权限及服务状态。") : reason.trimmed()));
}
