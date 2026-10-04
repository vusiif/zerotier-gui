#include "ZeroTierUninstaller.h"
#include "ZeroTierClient.h"
#include <QProcess>
#include <QTimer>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
ZeroTierUninstaller::ZeroTierUninstaller(QObject *parent, const QString &program, Detector detector, int timeoutMs)
    : QObject(parent), m_process(new QProcess(this)), m_timer(new QTimer(this)), m_program(program),
      m_detector(detector ? std::move(detector) : Detector([] { return !ZeroTierClient::executable().isEmpty() || ZeroTierClient::serviceStatus() != "missing"; })), m_timeout(timeoutMs)
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this] { m_timedOut = true; m_process->kill(); });
    connect(m_process, &QProcess::readyReadStandardOutput, this, &ZeroTierUninstaller::readOutput);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete(false, m_process->errorString());
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        readOutput();
        if (m_timedOut) { complete(false, QStringLiteral("卸载等待超时；安装程序可能仍在运行，请检查实际状态后重试。")); return; }
        if (status != QProcess::NormalExit || (code != 0 && code != 3010)) { complete(false, QStringLiteral("卸载未完成（退出码 %1），请查看设置页日志。").arg(code)); return; }
        if (m_detector()) { complete(false, code == 3010 ? QStringLiteral("卸载要求重启 Windows，请重启后确认。") : QStringLiteral("卸载命令已结束，仍检测到 ZeroTier 程序或服务，请检查实际状态。")); return; }
        complete(true, code == 3010 ? QStringLiteral("ZeroTier 已卸载，卸载器要求重启 Windows。请重启后再次打开 GUI。")
                                   : QStringLiteral("ZeroTier 已卸载。退出并重新启动 GUI 后可进入安装窗口。"));
    });
}
QStringList ZeroTierUninstaller::arguments()
{
    return {"uninstall", "--id", "ZeroTier.ZeroTierOne", "--exact", "--source", "winget", "--silent", "--accept-source-agreements", "--disable-interactivity"};
}
void ZeroTierUninstaller::run(std::function<void(bool, QString)> callback)
{
    if (m_callback) { callback(false, QStringLiteral("已有卸载操作正在进行。")); return; }
    const auto program = m_program.isEmpty() ? QStandardPaths::findExecutable("winget.exe") : m_program;
    if (program.isEmpty()) { callback(false, QStringLiteral("winget 不可用，请安装 Microsoft“应用安装程序”或到 Windows 设置卸载。")); return; }
    m_callback = std::move(callback); m_timedOut = false; m_decoder = QStringDecoder(QStringDecoder::Utf8);
    m_timer->start(m_timeout); m_process->start(program, arguments());
}
void ZeroTierUninstaller::readOutput()
{
    const auto bytes = m_process->readAllStandardOutput();
    if (!bytes.isEmpty()) { const QString text = m_decoder(bytes); emit log(text.right(16384)); }
}
void ZeroTierUninstaller::complete(bool ok, const QString &message)
{
    if (!m_callback) return;
    m_timer->stop(); auto callback = std::move(m_callback); m_callback = {}; callback(ok, message);
}
