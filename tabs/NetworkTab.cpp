#include "NetworkTab.h"
#include "../MainWindow.h"
#include "../ZeroTierClient.h"
#include "../ManagementClient.h"
#include "../NetworkMonitor.h"
#include "../NetworkSettingsDialog.h"
#include <QStatusBar>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QVBoxLayout>

NetworkTab::NetworkTab(MainWindow *mainWindow, QWidget *parent)
    : DataTable("networks", {"名称", "网络 ID", "状态", "分配的 IP"}, parent), m_main(mainWindow)
{
    auto *actions = new QHBoxLayout;
    m_input = new QLineEdit;
    m_input->setObjectName("networkIdInput");
    m_input->setPlaceholderText(QStringLiteral("输入 16 位十六进制网络 ID"));
    m_input->setMaxLength(16);
    actions->addWidget(m_input, 1);
    m_join = new QPushButton(QStringLiteral("加入网络"));
    m_leave = new QPushButton(QStringLiteral("退出网络"));
    actions->addWidget(m_join);
    actions->addWidget(m_leave);
    m_settings = new QPushButton(QStringLiteral("网络设置"));
    m_settings->setObjectName("networkSettingsButton");
    m_settings->setEnabled(false);
    actions->addWidget(m_settings);
    connect(m_settings, &QPushButton::clicked, this, &NetworkTab::editSettings);
    connect(tree(), &QTreeWidget::currentItemChanged, this, [this] {
        m_settings->setEnabled(!selectedId().isEmpty() && !m_operating && !m_settingsOpen);
    });
    m_client = new ZeroTierClient(this);
    connect(m_client, &ZeroTierClient::log, m_main, &MainWindow::appendOutput);
    m_monitor = new NetworkMonitor(m_client, this, [this] { return !m_operating && !m_settingsOpen && !m_main->operationBusy(); });
    connect(m_monitor, &NetworkMonitor::diagnostic, m_main, &MainWindow::appendOutput);
    connect(m_monitor, &NetworkMonitor::automaticallyLeft, this, [this](const QString &id) {
        const auto message = QStringLiteral("网络 %1 不存在，已自动退出。").arg(id);
        m_main->appendOutput(message);
        m_main->statusBar()->showMessage(message, 15000);
        m_poller->refresh();
    });
    connect(m_monitor, &NetworkMonitor::automaticLeaveFailed, this, [this](const QString &id, const QString &reason) {
        m_main->statusBar()->showMessage(QStringLiteral("网络 %1 自动退出失败：%2").arg(id, reason), 15000);
    });
    bodyLayout()->insertLayout(0, actions);
    connect(m_join, &QPushButton::clicked, this, &NetworkTab::joinNetwork);
    connect(m_leave, &QPushButton::clicked, this, &NetworkTab::leaveNetwork);
    connect(m_input, &QLineEdit::returnPressed, this, &NetworkTab::joinNetwork);
    m_poller = new JsonPoller(this, "listnetworks");
    connect(m_poller, &JsonPoller::availabilityChanged, this, &DataTable::setAvailability);
    connect(m_poller, &JsonPoller::updated, this, [this](const QJsonDocument &document) {
        QList<TableRow> rows;
        for (const auto &network : document.array()) {
            const auto object = network.toObject();
            const QString id = object["nwid"].toString(object["id"].toString());
            QStringList addresses;
            for (const auto &address : object["assignedAddresses"].toArray()) addresses << address.toString();
            rows.append({id, {id, ZeroTier::translate(object["status"].toString()),
                addresses.isEmpty() ? QStringLiteral("—") : addresses.join(", ")}, object});
        }
        setRows(rows);
    });
}

void NetworkTab::showEvent(QShowEvent *event)
{
    DataTable::showEvent(event);
    m_poller->refresh();
}

void NetworkTab::joinNetwork()
{
    operate("join", m_input->text().trimmed().toLower());
}

void NetworkTab::leaveNetwork()
{
    const QString id = selectedId().isEmpty() ? m_input->text().trimmed().toLower() : selectedId();
    if (!QRegularExpression("^[0-9a-fA-F]{16}$").match(id).hasMatch()) {
        QMessageBox::warning(this, QStringLiteral("退出网络"), QStringLiteral("请选择一个网络或输入完整的 16 位网络 ID。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("退出网络"), QStringLiteral("确定退出网络 %1？").arg(id),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    operate("leave", id);
}

void NetworkTab::operate(const QString &action, const QString &id)
{
    if (m_operating || m_settingsOpen) return;
    if (!QRegularExpression("^[0-9a-fA-F]{16}$").match(id).hasMatch()) {
        QMessageBox::warning(this, QStringLiteral("网络 ID"), QStringLiteral("请输入完整的 16 位十六进制网络 ID。"));
        return;
    }
    if (!m_main->beginOperation()) return;
    m_operating = true;
    m_join->setEnabled(false);
    m_leave->setEnabled(false);
    m_settings->setEnabled(false);
    const QString submittedInput = m_input->text();
    ZeroTier::command(this, {action, id}, [this, action, id, submittedInput](bool ok, const QString &output) {
        m_operating = false;
        m_main->endOperation();
        m_join->setEnabled(true);
        m_leave->setEnabled(true);
        m_settings->setEnabled(!selectedId().isEmpty());
        m_main->appendOutput((action == "join" ? QStringLiteral("加入网络：") : QStringLiteral("退出网络：")) + output + '\n');
        if (!ok) QMessageBox::warning(this, QStringLiteral("操作失败"), output.isEmpty() ? QStringLiteral("请检查 ZeroTier 服务和管理员权限。") : output);
        else {
            if (action == "join") m_monitor->track(id);
            else m_monitor->forget(id);
            if (m_input->text() == submittedInput) m_input->clear();
            m_poller->refresh();
        }
    });
}

void NetworkTab::editSettings()
{
    const auto id = selectedId();
    if (id.isEmpty() || m_operating || m_settingsOpen) return;
    if (!m_main->beginOperation()) return;
    m_settingsOpen = true;
    m_join->setEnabled(false);
    m_leave->setEnabled(false);
    m_settings->setEnabled(false);
    NetworkSettingsDialog dialog(m_client, id, this);
    dialog.exec();
    m_main->endOperation();
    m_settingsOpen = false;
    m_join->setEnabled(true);
    m_leave->setEnabled(true);
    m_settings->setEnabled(!selectedId().isEmpty());
    m_poller->refresh();
}
