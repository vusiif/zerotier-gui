#include "InstallWindow.h"

#include "ManagementClient.h"
#include "AppLog.h"
#include <QPushButton>

#include <QLabel>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProcess>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTimer>
#include <QCloseEvent>
#include <QStringConverter>
#include <memory>

bool zeroTierInstalled()
{
    const auto status = ZeroTierClient::serviceStatus();
    return !ZeroTierClient::executable().isEmpty() || status == "running" || status == "stopped" || status == "pending";
}

InstallWindow::InstallWindow(std::function<bool()> detector, QWidget *parent)
    : QDialog(parent), m_detector(std::move(detector)), m_process(new QProcess(this)), m_timeout(new QTimer(this))
{
    setWindowTitle(QStringLiteral("安装 ZeroTier"));
    resize(660, 590);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);

    m_status = new QLabel(QStringLiteral("尚未安装 ZeroTier，请使用 winget 安装后继续。"), this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_install = new QPushButton(QStringLiteral("使用 winget 安装 ZeroTier"), this);
    m_detect = new QPushButton(QStringLiteral("重新检测并继续"), this);
    m_detect->setObjectName("recheckInstallation");
    layout->addWidget(m_install);
    layout->addWidget(m_detect);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->hide();
    layout->addWidget(m_progress);
    layout->addStretch();
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(600000);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        AppLog::write(QStringLiteral("winget 安装超时，已停止本次等待。"));
        m_process->kill();
    });
    auto decoder = std::make_shared<QStringDecoder>(QStringDecoder::Utf8);
    connect(m_install, &QPushButton::clicked, this, [this, decoder] { decoder->resetState(); install(); });
    connect(m_detect, &QPushButton::clicked, this, [this] { detect(); });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this, decoder] {
        const QString text = (*decoder)(m_process->readAllStandardOutput());
        AppLog::write(text.right(16384));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_timeout->stop();
        setBusy(false);
        m_status->setText(QStringLiteral("winget 无法启动：%1").arg(m_process->errorString()));
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        m_timeout->stop();
        AppLog::write(QStringLiteral("winget 安装结束，退出码 %1").arg(code));
        setBusy(false);
        // An installer exit code alone is not proof that ZeroTier is available.
        if (status == QProcess::NormalExit && (code == 0 || code == 3010)) {
            detect();
            if (result() != QDialog::Accepted)
                m_status->setText(code == 3010 ? QStringLiteral("安装要求重启 Windows。请重启后打开本程序。")
                                             : QStringLiteral("安装已结束，尚未检测到 ZeroTier，请稍后重新检测。"));
        } else m_status->setText(QStringLiteral("安装未完成，退出码：%1。请检查网络连接与 winget 后重试。").arg(code));
    });
    setBusy(false);
    // Also notices installations performed outside this window.
    auto *poll = new QTimer(this);
    connect(poll, &QTimer::timeout, this, [this] { if (m_process->state() == QProcess::NotRunning && m_detector()) accept(); });
    poll->start(1000);
}
void InstallWindow::setBusy(bool busy)
{
    m_progress->setVisible(busy);
    m_detect->setEnabled(!busy);
    const bool winget = !QStandardPaths::findExecutable("winget.exe").isEmpty();
    m_install->setEnabled(!busy && winget);
    if (!busy && !winget) m_status->setText(QStringLiteral("未安装 ZeroTier，winget 不可用。请安装 Microsoft 的“应用安装程序”，然后重新检测。"));
}
void InstallWindow::detect()
{
    if (m_detector()) { accept(); return; }
    setBusy(false);
}
void InstallWindow::install()
{
    if (m_process->state() != QProcess::NotRunning) return;
    if (QMessageBox::question(this, QStringLiteral("安装 ZeroTier"), QStringLiteral("使用 winget 下载并安装 ZeroTier，并接受软件包与来源协议，是否继续？")) != QMessageBox::Yes) return;
    const auto winget = QStandardPaths::findExecutable("winget.exe");
    if (winget.isEmpty()) { setBusy(false); return; }
    setBusy(true);
    m_status->setText(QStringLiteral("正在安装 ZeroTier，请等待完成…"));
    m_process->start(winget, {"install", "--id", "ZeroTier.ZeroTierOne", "--exact", "--source", "winget",
                            "--accept-package-agreements", "--accept-source-agreements", "--disable-interactivity"});
    m_timeout->start();
}
void InstallWindow::closeEvent(QCloseEvent *event)
{
    if (m_process->state() != QProcess::NotRunning) { event->ignore(); return; }
    QDialog::closeEvent(event);
}
void InstallWindow::reject()
{
    // Escape must not close the dialog while winget is still installing.
    if (m_process->state() == QProcess::NotRunning) QDialog::reject();
}
