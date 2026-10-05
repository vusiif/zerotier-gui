#include "ServiceTab.h"
#include "../MainWindow.h"
#include "../ZeroTierClient.h"
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

ServiceTab::ServiceTab(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_main(mainWindow)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(14);
    auto *title = new QLabel(QStringLiteral("ZeroTier 服务"));
    title->setObjectName("serviceTitle");
    QFont font = title->font();
    font.setPointSize(18);
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    m_installation = new QLabel;
    m_installation->setWordWrap(true);
    layout->addWidget(m_installation);
    m_service = new QLabel(QStringLiteral("等待服务状态…"));
    m_service->setWordWrap(true);
    layout->addWidget(m_service);
    auto *serviceActions = new QHBoxLayout;
    m_start = new QPushButton(QStringLiteral("启动"));
    m_stop = new QPushButton(QStringLiteral("停止"));
    m_restart = new QPushButton(QStringLiteral("重启"));
    serviceActions->addWidget(m_start);
    serviceActions->addWidget(m_stop);
    serviceActions->addWidget(m_restart);
    serviceActions->addStretch();
    layout->addLayout(serviceActions);
    connect(m_start, &QPushButton::clicked, this, [this] { changeService("start"); });
    connect(m_stop, &QPushButton::clicked, this, [this] { changeService("stop"); });
    connect(m_restart, &QPushButton::clicked, this, [this] { changeService("restart"); });
    auto *packages = new QHBoxLayout;
    m_install = new QPushButton(QStringLiteral("使用 winget 安装 ZeroTier"));
    m_install->setObjectName("installButton");
    m_uninstall = new QPushButton(QStringLiteral("卸载 ZeroTier 并删除数据"));
    m_uninstall->setObjectName("uninstallButton");
    packages->addWidget(m_install);
    packages->addWidget(m_uninstall);
    packages->addStretch();
    layout->addLayout(packages);
    connect(m_install, &QPushButton::clicked, this, [this] { packageAction(false); });
    connect(m_uninstall, &QPushButton::clicked, this, [this] { packageAction(true); });
    auto *hint = new QLabel(QStringLiteral("程序启动时请求管理员授权，用于读取 ZeroTier 认证信息和管理服务。"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_feedback = new QLabel;
    m_feedback->setObjectName("serviceFeedback");
    m_feedback->setWordWrap(true);
    m_feedback->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_feedback);
    layout->addStretch();
    m_poller = new JsonPoller(this, "info", true);
    connect(m_poller, &JsonPoller::availabilityChanged, this, [this](bool available, const QString &message) {
        if (!available) m_service->setText(message);
    });
    connect(m_poller, &JsonPoller::updated, this, [this](const QJsonDocument &document) {
        const auto object = document.object();
        m_service->setText(QStringLiteral("节点 %1 · %2 · 版本 %3")
            .arg(object["address"].toString(), object["online"].toBool() ? QStringLiteral("在线") : QStringLiteral("离线"),
                 object["version"].toString()));
    });
    auto *timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, &ServiceTab::updateInstallation);
    timer->start();
    updateInstallation();
}

void ServiceTab::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateInstallation();
    m_poller->refresh();
}

void ServiceTab::updateInstallation()
{
    m_installed = !ZeroTier::executable().isEmpty();
    m_installation->setText(m_installed ? QStringLiteral("已找到 ZeroTier，服务状态自动更新。")
                                       : QStringLiteral("尚未安装 ZeroTier，请使用 winget 安装后继续。"));
    updateButtons();
}

void ServiceTab::updateButtons()
{
    m_install->setEnabled(!m_busy && !m_installed);
    m_uninstall->setEnabled(!m_busy && m_installed);
    for (auto *button : {m_start, m_stop, m_restart}) button->setEnabled(!m_busy && m_installed);
}

void ServiceTab::changeService(const QString &action)
{
    if (m_busy) return;
    m_busy = true;
    updateButtons();
    m_feedback->setText(QStringLiteral("正在操作服务…"));
    m_main->runService(action, [this](bool ok) {
        m_busy = false;
        updateInstallation();
        m_feedback->setText(ok ? QStringLiteral("服务操作完成。") : QStringLiteral("服务操作未完成，请检查管理员权限和服务状态。"));
        m_poller->refresh();
    });
}

void ServiceTab::packageAction(bool uninstall)
{
    if (m_busy) return;
    const QString winget = QStandardPaths::findExecutable("winget");
    if (winget.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法使用 winget"), QStringLiteral("未找到 winget，请安装 Windows 应用安装程序后重试。"));
        return;
    }
    const QString title = uninstall ? QStringLiteral("卸载 ZeroTier") : QStringLiteral("安装 ZeroTier");
    const QString text = uninstall
        ? QStringLiteral("卸载 ZeroTier，并删除 %1 中的全部数据（包括节点身份和网络配置），确定继续？").arg(ZeroTier::dataDirectory())
        : QStringLiteral("使用 winget 下载并安装 ZeroTier，并接受软件包与来源协议，是否继续？");
    QMessageBox confirmation(QMessageBox::Question, title, text,
                             QMessageBox::Yes | QMessageBox::No, this);
    confirmation.setDefaultButton(QMessageBox::No);
    confirmation.setButtonText(QMessageBox::Yes, uninstall ? QStringLiteral("卸载并删除数据") : QStringLiteral("继续安装"));
    confirmation.setButtonText(QMessageBox::No, QStringLiteral("取消"));
    if (confirmation.exec() != QMessageBox::Yes) return;
    m_busy = true;
    updateButtons();
    m_feedback->setText(uninstall ? QStringLiteral("正在卸载并清理数据…") : QStringLiteral("正在下载和安装…"));
    m_main->runElevatedScript(title, ZeroTier::packageScript(uninstall, winget), [this, uninstall](bool ok) {
        m_busy = false;
        updateInstallation();
        m_feedback->setText(ok ? (uninstall ? QStringLiteral("ZeroTier 已卸载，数据目录已删除。") : QStringLiteral("安装完成，可进入网络页面加入网络。"))
                               : (uninstall ? QStringLiteral("卸载或数据清理未完成，请检查权限和服务状态。") : QStringLiteral("安装未完成，请检查网络连接和 winget。")));
        m_poller->refresh();
    });
}
