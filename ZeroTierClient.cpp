#include "ZeroTierClient.h"
#include "ManagementClient.h"
#include "AppLog.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWidget>
#include <memory>

QString ZeroTier::dataDirectory()
{
#ifdef Q_OS_WIN
    return QDir(qEnvironmentVariable("ProgramData", "C:/ProgramData")).filePath("ZeroTier");
#else
    return QStringLiteral("/var/lib/zerotier-one");
#endif
}

QString ZeroTier::homeDirectory()
{
#ifdef Q_OS_WIN
    return QDir(dataDirectory()).filePath("One");
#else
    return dataDirectory();
#endif
}

QString ZeroTier::executable()
{
    const auto cli = QStandardPaths::findExecutable("zerotier-cli");
    if (!cli.isEmpty()) {
        const auto info = QFileInfo(cli);
        // Windows ships a .bat wrapper. QProcess runs the sibling binary directly.
        if (info.suffix().compare("bat", Qt::CaseInsensitive) != 0 &&
            info.suffix().compare("cmd", Qt::CaseInsensitive) != 0) return cli;
        for (const auto &binary : {"zerotier-one_x64.exe", "zerotier-one_x86.exe"}) {
            const auto path = info.dir().filePath(binary);
            if (QFileInfo::exists(path)) return path;
        }
    }
    const QStringList candidates = {
        QDir(homeDirectory()).filePath("zerotier-cli_x64.exe"),
        QDir(homeDirectory()).filePath("zerotier-one_x64.exe"),
        QDir(homeDirectory()).filePath("zerotier-one_x86.exe"),
        QDir(qEnvironmentVariable("ProgramFiles(x86)", "C:/Program Files (x86)"))
            .filePath("ZeroTier/One/zerotier-one_x64.exe"),
        QDir(qEnvironmentVariable("ProgramFiles", "C:/Program Files"))
            .filePath("ZeroTier/One/zerotier-one_x64.exe")
    };
    for (const auto &candidate : candidates)
        if (QFileInfo::exists(candidate)) return candidate;
    return {};
}

static QStringList cliArguments(const QString &program, const QStringList &arguments)
{
    auto result = arguments;
    if (QFileInfo(program).fileName().startsWith("zerotier-one")) result.prepend("-q");
    return result;
}

QString ZeroTier::translate(const QString &value)
{
    static const QHash<QString, QString> labels = {
        {"PLANET", "根节点"}, {"MOON", "中转站"}, {"LEAF", "成员"},
        {"DIRECT", "直链"}, {"RELAY", "中转"}, {"OK", "已连接"},
        {"ONLINE", "在线"}, {"OFFLINE", "离线"}, {"PRIVATE", "私有"},
        {"PUBLIC", "公开"}, {"ACCESS_DENIED", "等待授权"},
        {"REQUESTING_CONFIGURATION", "获取配置中"}, {"NOT_FOUND", "网络不存在"},
        {"PORT_ERROR", "端口错误"}, {"CLIENT_TOO_OLD", "客户端版本过旧"},
        {"AUTHENTICATION_REQUIRED", "需要身份验证"},
        {"address", "节点 ID"}, {"id", "ID"}, {"nwid", "网络 ID"},
        {"name", "名称"}, {"role", "角色"}, {"latency", "延迟（ms）"},
        {"paths", "连接路径"}, {"version", "版本"}, {"status", "状态"},
        {"type", "类型"}, {"assignedAddresses", "分配的 IP"},
        {"online", "在线"}, {"tcpFallbackActive", "TCP 回退"},
        {"roots", "根节点"}, {"stableEndpoints", "固定地址"},
        {"identity", "身份"}, {"timestamp", "时间戳"}, {"signature", "签名"},
        {"active", "有效"}, {"preferred", "首选"}, {"trustedPathId", "可信路径 ID"},
        {"lastReceive", "最近接收"}, {"lastSend", "最近发送"},
        {"portDeviceName", "虚拟网卡"}, {"mac", "MAC 地址"}, {"mtu", "MTU"},
        {"allowManaged", "允许托管地址"}, {"allowGlobal", "允许全局地址"},
        {"allowDefault", "允许默认路由"}, {"allowDNS", "允许 DNS"},
        {"routes", "路由"}, {"target", "目标"}, {"via", "网关"},
        {"dns", "DNS"}, {"servers", "服务器"}, {"domain", "域名"},
        {"worldId", "世界 ID"}, {"worldType", "世界类型"},
        {"config", "配置"}, {"settings", "设置"}, {"homeDir", "数据目录"},
        {"moonFile", "Moon 文件"}, {"localName", "本机名称"}
    };
    return labels.value(value, value);
}

QString ZeroTier::jsonText(const QJsonValue &value)
{
    if (value.isBool()) return value.toBool() ? QStringLiteral("是") : QStringLiteral("否");
    if (value.isNull() || value.isUndefined()) return QStringLiteral("—");
    if (value.isObject()) return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray()) return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return translate(value.toVariant().toString());
}

QString ZeroTier::quotePowerShell(const QString &value)
{
    QString escaped = value;
    escaped.replace('\\', '/');
    escaped.replace("'", "''");
    return "'" + escaped + "'";
}

ZeroTier::PowerShellInvocation ZeroTier::preparePowerShell(const QString &script)
{
    PowerShellInvocation invocation;
    invocation.directory = std::make_shared<QTemporaryDir>(
        QDir::tempPath() + "/zerotier-command-XXXXXX");
    if (!invocation.directory->isValid()) return invocation;
    // The child creates this file. QTemporaryFile::close() would retain an
    // exclusive Windows file handle and prevent PowerShell from opening it.
    invocation.logPath = invocation.directory->filePath("output.log");
    const QString wrapped = "$ProgressPreference = 'SilentlyContinue'\n"
        "$PSDefaultParameterValues['Out-File:Encoding'] = 'utf8'\n& {\n"
        + script + "\n} *> " + quotePowerShell(invocation.logPath);
    invocation.arguments = {"-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden",
                            "-Command", wrapped};
    return invocation;
}

QString ZeroTier::packageScript(bool uninstall, const QString &winget)
{
    QString script = "$ErrorActionPreference = 'Stop'\n& " + quotePowerShell(winget);
    script += uninstall
        ? " uninstall --id ZeroTier.ZeroTierOne --exact --source winget --silent --disable-interactivity\n"
        : " install --id ZeroTier.ZeroTierOne --exact --source winget --silent --accept-package-agreements --accept-source-agreements --disable-interactivity\n";
    script += "$code = $LASTEXITCODE\nif ($code -notin @(0, 3010, 1641)) { exit $code }\n";
    if (uninstall) {
        // This target is fixed, never supplied by a text field or by CLI JSON.
        // Resolve and validate it in the same shell that performs the deletion.
        script += R"PS($expected = [IO.Path]::GetFullPath((Join-Path $env:ProgramData 'ZeroTier'))
$target = [IO.Path]::GetFullPath()PS" + quotePowerShell(dataDirectory()) + R"PS()
if ($target -ne $expected -or [IO.Path]::GetFileName($target) -ne 'ZeroTier') { exit 1 }
if (Test-Path -LiteralPath $target) {
    $item = Get-Item -LiteralPath $target -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { exit 1 }
    $links = Get-ChildItem -LiteralPath $target -Recurse -Force -ErrorAction Stop |
        Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }
    if ($links) { exit 1 }
    Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
    if (Test-Path -LiteralPath $target) { exit 1 }
}
)PS";
    }
    return "try {\n" + script + "\nexit 0\n} catch { Write-Output $_; exit 1 }";
}

void ZeroTier::command(QObject *owner, const QStringList &arguments,
                       std::function<void(bool, const QString &)> finished)
{
    const QString program = executable();
    if (program.isEmpty()) {
        finished(false, QStringLiteral("未找到 ZeroTier，请先安装。"));
        return;
    }
    auto *client = new ZeroTierClient(owner, program);
    QObject::connect(client, &ZeroTierClient::log, client, [](const QString &text) { AppLog::write(text); });
    client->run(arguments, [client, finished](bool ok, QByteArray output) {
        finished(ok, QString::fromUtf8(output).trimmed());
        client->deleteLater();
    });
}

JsonPoller::JsonPoller(QWidget *owner, const QString &command, bool object)
    : QObject(owner), m_owner(owner), m_command(command), m_object(object)
{
    m_interval.setInterval(2000);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(5000);
    connect(&m_interval, &QTimer::timeout, this, &JsonPoller::refresh);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        m_process.kill();
        failed(QStringLiteral("ZeroTier 响应超时，正在自动重试"));
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_timeout.stop();
        failed(QStringLiteral("无法读取 ZeroTier，请检查安装或管理员权限"));
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        m_timeout.stop();
        auto document = QJsonDocument::fromJson(m_process.readAllStandardOutput());
        if (code != 0 || status != QProcess::NormalExit ||
            (m_object ? !document.isObject() : !document.isArray())) {
            failed(QStringLiteral("服务未响应，请检查服务状态或以管理员身份运行"));
            return;
        }
        emit availabilityChanged(true, QStringLiteral("每 2 秒自动更新"));
        emit updated(document);
    });
    m_interval.start();
    QTimer::singleShot(0, this, &JsonPoller::refresh);
}

JsonPoller::~JsonPoller()
{
    m_interval.stop();
    m_timeout.stop();
    m_process.disconnect(this);
    disconnect();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

void JsonPoller::failed(const QString &message)
{
    emit availabilityChanged(false, message);
}

void JsonPoller::refresh()
{
    if (!m_owner->isVisible() || m_process.state() != QProcess::NotRunning) return;
    const QString program = ZeroTier::executable();
    if (program.isEmpty()) {
        failed(QStringLiteral("未安装 ZeroTier，请在服务页面安装"));
        return;
    }
    m_process.start(program, cliArguments(program, {"-j", m_command}));
    m_timeout.start();
}
