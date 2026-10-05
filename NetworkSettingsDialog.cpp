#include "NetworkSettingsDialog.h"
#include "ManagementClient.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

static const QStringList settingKeys{"allowManaged", "allowGlobal", "allowDefault", "allowDNS"};
static const QStringList ipKeys{"ip", "ip4", "ip6", "ip6plane", "ip6prefix"};

NetworkSettingsDialog::NetworkSettingsDialog(ZeroTierClient *client, const QString &networkId, QWidget *parent)
    : QDialog(parent), m_client(client), m_id(networkId.trimmed().toLower())
{
    setObjectName("networkSettingsDialog"); setWindowTitle(QStringLiteral("网络设置"));
    setModal(true); resize(580, 530);
    auto *layout = new QVBoxLayout(this);
    auto *identity = new QLabel(QStringLiteral("网络 ID：%1").arg(m_id));
    identity->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(identity);
    const QStringList labels{QStringLiteral("允许托管地址和路由"), QStringLiteral("允许全局地址和路由"),
        QStringLiteral("允许默认路由（可能改变上网路径）"), QStringLiteral("允许网络配置 DNS")};
    for (int i = 0; i < settingKeys.size(); ++i) {
        auto *check = new QCheckBox(labels[i]); check->setObjectName(settingKeys[i]);
        m_checks << check; layout->addWidget(check);
    }
    auto *actions = new QHBoxLayout;
    m_reload = new QPushButton(QStringLiteral("重新读取")); m_reload->setObjectName("reloadNetworkSettings");
    m_save = new QPushButton(QStringLiteral("保存设置")); m_save->setObjectName("saveNetworkSettings");
    actions->addWidget(m_reload); actions->addWidget(m_save); layout->addLayout(actions);
    layout->addWidget(new QLabel(QStringLiteral("属性查询（JSON 属性或 IP）")));
    auto *queryRow = new QHBoxLayout;
    m_property = new QComboBox; m_property->setObjectName("networkProperty"); m_property->setEditable(true);
    m_property->addItems(ipKeys); queryRow->addWidget(m_property, 1);
    m_query = new QPushButton(QStringLiteral("查询")); m_query->setObjectName("queryNetworkProperty");
    queryRow->addWidget(m_query); layout->addLayout(queryRow);
    m_result = new QPlainTextEdit; m_result->setObjectName("networkPropertyResult");
    m_result->setReadOnly(true); m_result->setMaximumBlockCount(1000); layout->addWidget(m_result, 1);
    m_status = new QLabel; m_status->setObjectName("networkSettingsStatus"); m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText); layout->addWidget(m_status);
    m_close = new QPushButton(QStringLiteral("关闭")); layout->addWidget(m_close, 0, Qt::AlignRight);
    for (auto *button : {m_reload, m_save, m_query, m_close}) { button->setAutoDefault(false); button->setDefault(false); }
    connect(m_reload, &QPushButton::clicked, this, &NetworkSettingsDialog::reload);
    connect(m_save, &QPushButton::clicked, this, &NetworkSettingsDialog::save);
    connect(m_query, &QPushButton::clicked, this, &NetworkSettingsDialog::query);
    connect(m_close, &QPushButton::clicked, this, &NetworkSettingsDialog::reject);
    setBusy(false);
    QTimer::singleShot(0, this, &NetworkSettingsDialog::reload);
}

void NetworkSettingsDialog::setBusy(bool busy)
{
    m_busy = busy;
    const bool ready = !busy && !m_snapshot.isEmpty();
    for (auto *check : m_checks) check->setEnabled(ready);
    m_save->setEnabled(ready); m_query->setEnabled(ready); m_property->setEnabled(ready);
    m_reload->setEnabled(!busy); m_close->setEnabled(!busy);
}

void NetworkSettingsDialog::fetch(std::function<void(QJsonObject, QString)> callback)
{
    QPointer<NetworkSettingsDialog> guard(this);
    m_client->run({"-j", "listnetworks"}, [guard, callback](bool ok, QByteArray bytes) {
        if (!guard) return;
        if (!ok) { callback({}, QStringLiteral("读取失败：%1").arg(guard->m_client->lastError())); return; }
        const auto doc = QJsonDocument::fromJson(bytes);
        if (!doc.isArray()) { callback({}, QStringLiteral("网络列表不是有效 JSON 数组。")); return; }
        for (const auto &value : doc.array()) {
            const auto object = value.toObject();
            if (object.value("id").toString().toLower() != guard->m_id && object.value("nwid").toString().toLower() != guard->m_id) continue;
            for (const auto &key : settingKeys) {
                if (!object.value(key).isBool()) { callback({}, QStringLiteral("设置字段 %1 缺失或类型错误。请重新读取。").arg(key)); return; }
            }
            callback(object, {}); return;
        }
        callback({}, QStringLiteral("未找到该网络，请确认已加入后重新读取。"));
    });
}

void NetworkSettingsDialog::display(const QJsonObject &object)
{
    m_snapshot = object;
    for (int i = 0; i < settingKeys.size(); ++i) m_checks[i]->setChecked(object.value(settingKeys[i]).toBool());
    const auto selected = m_property->currentText();
    m_property->clear(); m_property->addItems(ipKeys); m_property->addItems(object.keys());
    m_property->setCurrentText(selected);
}

void NetworkSettingsDialog::reload()
{
    if (m_busy) return;
    if (!QRegularExpression("^[0-9a-f]{16}$").match(m_id).hasMatch() || !m_client->hasAdminPrivileges()) {
        m_status->setText(QStringLiteral("需要有效的 16 位网络 ID 和管理员权限。")); return;
    }
    if (m_client->busy()) { m_status->setText(QStringLiteral("请等待其他命令完成后重新读取。")); return; }
    setBusy(true); m_status->setText(QStringLiteral("正在读取…"));
    fetch([this](QJsonObject object, QString error) {
        display(object); setBusy(false);
        m_status->setText(error.isEmpty() ? QStringLiteral("已读取当前设置。") : error);
    });
}

void NetworkSettingsDialog::save()
{
    if (m_busy || m_snapshot.isEmpty()) return;
    QJsonObject wanted; QStringList changed, review;
    for (int i = 0; i < settingKeys.size(); ++i) {
        wanted.insert(settingKeys[i], m_checks[i]->isChecked());
        if (wanted.value(settingKeys[i]) != m_snapshot.value(settingKeys[i])) {
            changed << settingKeys[i];
            review << QStringLiteral("%1：%2 → %3").arg(m_checks[i]->text(), m_snapshot.value(settingKeys[i]).toBool() ? QStringLiteral("开") : QStringLiteral("关"), m_checks[i]->isChecked() ? QStringLiteral("开") : QStringLiteral("关"));
        }
    }
    if (changed.isEmpty()) { m_status->setText(QStringLiteral("设置没有变化。")); return; }
    // Hold the entire workflow, including confirmation and readback, as one operation.
    setBusy(true);
    if (QMessageBox::question(this, QStringLiteral("确认网络设置"), QStringLiteral("将修改网络 %1：\n%2\n\n默认路由和 DNS 设置可能影响本机网络。是否保存？").arg(m_id, review.join('\n'))) != QMessageBox::Yes) { setBusy(false); return; }
    if (!m_client->hasAdminPrivileges() || m_client->busy()) {
        m_status->setText(QStringLiteral("没有管理员权限或其他命令正在执行，请重新读取。")); setBusy(false); return;
    }
    m_status->setText(QStringLiteral("正在检查当前设置…"));
    fetch([this, wanted, changed](QJsonObject current, QString error) {
        if (!error.isEmpty()) { m_snapshot = {}; setBusy(false); m_status->setText(error); return; }
        for (const auto &key : settingKeys) {
            if (current.value(key) != m_snapshot.value(key)) {
                display(current); setBusy(false);
                m_status->setText(QStringLiteral("设置已被其他程序修改，已加载最新值；请重新确认后保存。")); return;
            }
        }
        writeNext(changed, wanted, 0, {});
    });
}

void NetworkSettingsDialog::writeNext(const QStringList &keys, const QJsonObject &wanted, int index, const QStringList &applied)
{
    if (index == keys.size()) { verify(wanted, applied); return; }
    const auto key = keys[index];
    m_status->setText(QStringLiteral("正在保存 %1…").arg(key));
    QPointer<NetworkSettingsDialog> guard(this);
    m_client->run({"set", m_id, key + (wanted.value(key).toBool() ? "=1" : "=0")},
        [guard, keys, wanted, index, applied, key](bool ok, QByteArray) {
            if (!guard) return;
            if (!ok) {
                guard->verify(wanted, applied, QStringLiteral("%1 保存失败：%2。其余设置未继续提交。").arg(key, guard->m_client->lastError())); return;
            }
            auto next = applied; next << key;
            guard->writeNext(keys, wanted, index + 1, next);
        });
}

void NetworkSettingsDialog::verify(const QJsonObject &wanted, const QStringList &applied, const QString &failure)
{
    fetch([this, wanted, applied, failure](QJsonObject current, QString error) {
        if (!error.isEmpty()) {
            m_snapshot = {}; setBusy(false);
            m_status->setText(failure + QStringLiteral(" 无法确认最终状态：%1。部分设置可能已生效，请重新读取。").arg(error)); return;
        }
        QStringList mismatch;
        for (const auto &key : applied) if (current.value(key) != wanted.value(key)) mismatch << key;
        display(current); setBusy(false);
        if (!mismatch.isEmpty()) m_status->setText(failure + QStringLiteral(" 回读与提交值不一致：%1。开关显示的是实际值。").arg(mismatch.join(", ")));
        else if (!failure.isEmpty()) m_status->setText(failure + QStringLiteral(" 已重新读取实际值；此前提交：%1。").arg(applied.isEmpty() ? QStringLiteral("无") : applied.join(", ")));
        else m_status->setText(QStringLiteral("设置已保存并回读确认。"));
    });
}

void NetworkSettingsDialog::query()
{
    if (m_busy || m_snapshot.isEmpty()) return;
    const auto key = m_property->currentText().trimmed();
    if ((!ipKeys.contains(key) && !m_snapshot.contains(key)) || !QRegularExpression("^[A-Za-z][A-Za-z0-9_]*$").match(key).hasMatch()) {
        m_status->setText(QStringLiteral("请选择 IP 查询项或当前网络 JSON 中的属性名。")); return;
    }
    if (!m_client->hasAdminPrivileges() || m_client->busy()) { m_status->setText(QStringLiteral("需要管理员权限且命令执行器空闲。")); return; }
    m_result->clear(); setBusy(true);
    QPointer<NetworkSettingsDialog> guard(this);
    m_client->run({"get", m_id, key}, [guard](bool ok, QByteArray bytes) {
        if (!guard) return;
        const auto result = QString::fromUtf8(bytes).trimmed();
        // Some CLI versions return exit code 0 for an unknown property.
        const bool valid = ok && !result.startsWith("error", Qt::CaseInsensitive)
            && !result.startsWith("unknown network ID", Qt::CaseInsensitive);
        guard->m_result->setPlainText(result);
        guard->m_status->setText(valid ? (result.isEmpty() ? QStringLiteral("查询完成，无匹配值。") : QStringLiteral("查询完成。")) : QStringLiteral("查询失败：%1").arg(ok ? result : guard->m_client->lastError()));
        guard->setBusy(false);
    });
}

void NetworkSettingsDialog::reject() { if (!m_busy) QDialog::reject(); }
void NetworkSettingsDialog::closeEvent(QCloseEvent *event) { if (m_busy) event->ignore(); else QDialog::closeEvent(event); }
