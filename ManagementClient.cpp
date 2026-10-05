#include "ManagementClient.h"
#include "ZeroTierClient.h"
#include <QProcess>
#include <QTimer>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
ZeroTierClient::ZeroTierClient(QObject *parent, const QString &program, int timeoutMs) : QObject(parent), m_process(new QProcess(this)), m_timeout(new QTimer(this)), m_program(program), m_timeoutMs(timeoutMs)
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        m_lastError = QStringLiteral("命令超时，请检查 ZeroTier 服务是否响应。");
        emit log(m_lastError); m_overflow = true; m_process->kill();
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        auto bytes = m_process->readAllStandardOutput();
        if (m_buffer.size() + bytes.size() > 1024 * 1024) {
            m_lastError = QStringLiteral("命令输出超过 1MB，已停止读取。");
            m_overflow = true; m_process->kill();
        }
        else m_buffer += bytes;
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        auto bytes = m_process->readAllStandardError();
        m_error += bytes.left(qMax(0, 4096 - int(m_error.size())));
        emit log(QString::fromLocal8Bit(bytes).left(4096));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        emit log(m_process->errorString());
        if (m_lastError.isEmpty()) m_lastError = m_process->errorString();
        if (m_error.isEmpty()) m_error = m_process->errorString().toUtf8();
        if (e == QProcess::FailedToStart) complete(false);
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        auto tail = m_process->readAllStandardOutput();
        if (m_buffer.size() + tail.size() <= 1024 * 1024) m_buffer += tail;
        else { m_overflow = true; m_lastError = QStringLiteral("命令输出超过 1MB。"); }
        if ((code != 0 || status != QProcess::NormalExit) && m_lastError.isEmpty())
            m_lastError = QStringLiteral("命令执行失败（退出码 %1），请检查权限与服务状态。").arg(code);
        complete(!m_overflow && code == 0 && status == QProcess::NormalExit);
    });
}
ZeroTierClient::~ZeroTierClient()
{
    m_timeout->stop();
    m_callback = {};
    m_process->disconnect(this);
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

QString ZeroTierClient::executable()
{
    const auto configured = QSettings("ZeroTierGui", "ZeroTierGui").value("cliPath").toString();
    if (QFileInfo::exists(configured) && configured.endsWith(".exe", Qt::CaseInsensitive)) return configured;
    return ZeroTier::executable();
}

QString ZeroTierClient::dataDirectory()
{
    return QSettings("ZeroTierGui", "ZeroTierGui").value("dataDirectory", ZeroTier::homeDirectory()).toString();
}
QStringList ZeroTierClient::nativeArguments(const QString &program, const QStringList &arguments)
{
    auto result = arguments;
    if (QFileInfo(program).fileName().startsWith("zerotier-one", Qt::CaseInsensitive)) result.prepend("-q");
    return result;
}
bool ZeroTierClient::isAdministrator()
{
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{}; DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token); return ok && elevation.TokenIsElevated;
#else
    return false;
#endif
}
QString ZeroTierClient::serviceStatus()
{
#ifdef Q_OS_WIN
    auto manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) return "unknown";
    auto service = OpenServiceW(manager, L"ZeroTierOneService", SERVICE_QUERY_STATUS);
    const DWORD openError = service ? ERROR_SUCCESS : GetLastError();
    CloseServiceHandle(manager);
    if (!service) return openError == ERROR_SERVICE_DOES_NOT_EXIST ? "missing" : "unknown";
    SERVICE_STATUS_PROCESS status{}; DWORD bytes = 0;
    const bool ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytes);
    CloseServiceHandle(service);
    if (!ok) return "unknown";
    if (status.dwCurrentState == SERVICE_RUNNING) return "running";
    if (status.dwCurrentState == SERVICE_STOPPED) return "stopped";
    return "pending";
#else
    return "unknown";
#endif
}
bool ZeroTierClient::busy() const { return bool(m_callback); }
void ZeroTierClient::run(const QStringList &arguments, std::function<void(bool, QByteArray)> callback)
{
    if (busy()) {
        emit log(QStringLiteral("请等待当前命令完成。"));
        callback(false, QStringLiteral("已有命令正在执行，请稍后重试。").toUtf8()); return;
    }
    m_lastError.clear();
    auto path = m_program.isEmpty() ? executable() : m_program;
    if (path.isEmpty()) { m_lastError = QStringLiteral("未找到 ZeroTier 程序，请先安装或指定程序位置。"); callback(false, m_lastError.toUtf8()); return; }
    if (path.endsWith(".bat", Qt::CaseInsensitive)) { m_lastError = QStringLiteral("请选择 ZeroTier 原生 exe 程序。"); callback(false, m_lastError.toUtf8()); return; }
    m_buffer.clear(); m_error.clear(); m_overflow = false; m_callback = std::move(callback);
    emit busyChanged(true);
    emit log("> zerotier-cli " + arguments.join(' '));
    auto cliArgs = arguments;
    if (m_program.isEmpty() && QDir(activeDataDirectory()).exists()) cliArgs.prepend("-D" + QDir::toNativeSeparators(activeDataDirectory()));
    auto args = nativeArguments(path, cliArgs);
    m_timeout->start(m_timeoutMs); m_process->start(path, args);
}
void ZeroTierClient::complete(bool success)
{
    m_timeout->stop();
    if (!m_callback) return;
    auto callback = std::move(m_callback); m_callback = {};
    // CLI help and some informational commands write only to stderr even on success.
    if (m_buffer.isEmpty()) m_buffer = m_error;
    auto bytes = std::move(m_buffer);
    m_buffer = {}; m_error.clear(); m_error.squeeze();
    emit busyChanged(false);
    callback(success, std::move(bytes));
}
