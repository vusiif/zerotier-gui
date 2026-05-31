#include "StatusTab.h"
#include "../MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGroupBox>
#include <QStandardPaths>
#include <QFile>
#include <QFont>

static QString findZt()
{
    QString p = QStandardPaths::findExecutable("zerotier-cli");
    if (!p.isEmpty()) return p;
    QString fb = "C:/ProgramData/ZeroTier/One/zerotier-cli_x64.exe";
    return QFile::exists(fb) ? fb : "zerotier-cli";
}

StatusTab::StatusTab(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_main(mainWindow),
      m_proc(new QProcess(this)),
      m_netProc(new QProcess(this)),
      m_peerProc(new QProcess(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    // --- 大号状态指示器 ---
    m_statusIndicator = new QLabel("⏳ 检测中...");
    QFont statusFont = m_statusIndicator->font();
    statusFont.setPointSize(24);
    statusFont.setBold(true);
    m_statusIndicator->setFont(statusFont);
    m_statusIndicator->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusIndicator);

    // --- 版本号 ---
    m_versionLabel = new QLabel;
    m_versionLabel->setAlignment(Qt::AlignCenter);
    QFont verFont = m_versionLabel->font();
    verFont.setPointSize(12);
    m_versionLabel->setFont(verFont);
    layout->addWidget(m_versionLabel);

    layout->addSpacing(20);

    // --- 节点信息 GroupBox ---
    auto *infoGroup = new QGroupBox("节点信息");
    auto *infoLayout = new QVBoxLayout(infoGroup);

    m_addressLabel = new QLabel("节点地址: --");
    m_addressLabel->setFont(QFont("Consolas", 10));
    infoLayout->addWidget(m_addressLabel);

    m_portLabel = new QLabel("监听端口: --");
    m_portLabel->setFont(QFont("Consolas", 10));
    infoLayout->addWidget(m_portLabel);

    m_networkCountLabel = new QLabel("在线网络: --");
    m_networkCountLabel->setFont(QFont("Consolas", 10));
    infoLayout->addWidget(m_networkCountLabel);

    m_peerCountLabel = new QLabel("在线 Peers: --");
    m_peerCountLabel->setFont(QFont("Consolas", 10));
    infoLayout->addWidget(m_peerCountLabel);

    infoGroup->setMaximumWidth(400);
    layout->addWidget(infoGroup, 0, Qt::AlignHCenter);

    layout->addSpacing(20);

    // --- 快捷入口按钮 ---
    m_gotoZtBtn = new QPushButton("打开 ZeroTier 管理");
    m_gotoZtBtn->setFixedWidth(200);
    connect(m_gotoZtBtn, &QPushButton::clicked, this, &StatusTab::openZeroTierTabRequested);
    layout->addWidget(m_gotoZtBtn, 0, Qt::AlignHCenter);

    layout->addStretch();

    // --- 连接 info 命令的响应 ---
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        QByteArray data = m_proc->readAllStandardOutput();
        if (exitCode != 0 || data.isEmpty()) {
            showNotInstalled();
            return;
        }

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            showNotInstalled();
            return;
        }

        QJsonObject infoObj = doc.object();

        // 等 listnetworks 和 listpeers 完成后一起更新 UI
        // 用成员变量暂存 infoObj，在 net/peer 完成后检查
        m_proc->setProperty("_infoData", QVariant::fromValue(doc.toJson(QJsonDocument::Compact)));

        // 启动 listnetworks
        if (m_netProc->state() == QProcess::NotRunning) {
            m_main->appendOutput(">> zerotier-cli -j listnetworks\n");
            m_netProc->start(findZt(), {"-j", "listnetworks"});
        }
        // 启动 listpeers
        if (m_peerProc->state() == QProcess::NotRunning) {
            m_main->appendOutput(">> zerotier-cli -j listpeers\n");
            m_peerProc->start(findZt(), {"-j", "listpeers"});
        }
    });

    // --- 尝试完成刷新：两个子命令都完成或有失败时更新 UI ---
    auto attemptFinalUpdate = [this]() {
        // 检查是否两个子命令都有结果（成功或失败）
        bool netDone = m_proc->property("_netDone").toBool();
        bool peerDone = m_peerProc->property("_peerDone").toBool();
        if (!netDone || !peerDone) return;

        QJsonDocument infoDoc = QJsonDocument::fromJson(
            m_proc->property("_infoData").toByteArray());
        if (infoDoc.isObject()) {
            updateUI(infoDoc.object(),
                     m_proc->property("_netCount").toInt(),
                     m_peerProc->property("_peerCount").toInt());
        }
    };

    // --- 连接 listnetworks 响应 ---
    connect(m_netProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, attemptFinalUpdate](int, QProcess::ExitStatus) {
        QByteArray data = m_netProc->readAllStandardOutput();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        int netCount = 0;
        if (err.error == QJsonParseError::NoError && doc.isArray()) {
            netCount = doc.array().size();
        }
        m_proc->setProperty("_netCount", netCount);
        m_proc->setProperty("_netDone", true);
        attemptFinalUpdate();
    });

    // --- 连接 listpeers 响应 ---
    connect(m_peerProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, attemptFinalUpdate](int, QProcess::ExitStatus) {
        QByteArray data = m_peerProc->readAllStandardOutput();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        int peerCount = 0;
        if (err.error == QJsonParseError::NoError && doc.isArray()) {
            peerCount = doc.array().size();
        }
        m_peerProc->setProperty("_peerCount", peerCount);
        m_peerProc->setProperty("_peerDone", true);
        attemptFinalUpdate();
    });

    // 首次显示时自动刷新
    refresh();
}

void StatusTab::refresh()
{
    m_statusIndicator->setText("⏳ 检测中...");
    m_versionLabel->clear();
    m_addressLabel->setText("节点地址: --");
    m_portLabel->setText("监听端口: --");
    m_networkCountLabel->setText("在线网络: --");
    m_peerCountLabel->setText("在线 Peers: --");

    m_peerProc->setProperty("_peerDone", false);
    m_proc->setProperty("_netDone", false);
    m_proc->setProperty("_netCount", QVariant());

    if (m_proc->state() == QProcess::NotRunning) {
        m_main->appendOutput(">> zerotier-cli -j info\n");
        m_proc->start(findZt(), {"-j", "info"});
    }
}

void StatusTab::updateUI(const QJsonObject &infoJson, int networkCount, int peerCount)
{
    bool online = infoJson["online"].toBool();
    QString version = infoJson["version"].toString();
    QString address = infoJson["address"].toString();
    int port = infoJson["config"].toObject()["settings"].toObject()["primaryPort"].toInt(9993);

    if (online) {
        m_statusIndicator->setText("🟢  ZeroTier 在线");
        m_statusIndicator->setStyleSheet("color: #27ae60;");
    } else {
        m_statusIndicator->setText("🔴  ZeroTier 离线");
        m_statusIndicator->setStyleSheet("color: #e74c3c;");
    }

    m_versionLabel->setText(QString("版本 %1").arg(version));
    m_addressLabel->setText(QString("节点地址: %1").arg(address));
    m_portLabel->setText(QString("监听端口: %1").arg(port));
    m_networkCountLabel->setText(QString("在线网络: %1 个").arg(networkCount));
    m_peerCountLabel->setText(QString("在线 Peers: %1 个").arg(peerCount));
}

void StatusTab::showNotInstalled()
{
    m_statusIndicator->setText("⚠️  ZeroTier 未安装");
    m_statusIndicator->setStyleSheet("color: #f39c12;");
    m_versionLabel->setText("请先安装 ZeroTier One 服务");
    m_addressLabel->setText("节点地址: --");
    m_portLabel->setText("监听端口: --");
    m_networkCountLabel->setText("在线网络: --");
    m_peerCountLabel->setText("在线 Peers: --");
}
