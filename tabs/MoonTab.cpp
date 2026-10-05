#include "MoonTab.h"
#include "../MainWindow.h"
#include "../ZeroTierClient.h"
#include "../MoonFiles.h"
#include <QLineEdit>
#include <QRegularExpression>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

MoonTab::MoonTab(MainWindow *mainWindow, QWidget *parent)
    : DataTable("moons", {"名称", "Moon ID", "根节点", "固定地址"}, parent), m_main(mainWindow)
{
    auto *actions = new QHBoxLayout;
    m_add = new QPushButton(QStringLiteral("添加 Moon 文件…"));
    actions->addWidget(m_add);
    m_world = new QLineEdit;
    m_world->setObjectName("moonWorldInput");
    m_world->setMaxLength(16);
    m_world->setPlaceholderText(QStringLiteral("Moon world ID（最多16位）"));
    m_seed = new QLineEdit;
    m_seed->setObjectName("moonSeedInput");
    m_seed->setMaxLength(10);
    m_seed->setPlaceholderText(QStringLiteral("seed 节点 ID（10位）"));
    auto *orbit = new QPushButton(QStringLiteral("订阅 Moon"));
    auto *deorbit = new QPushButton(QStringLiteral("取消订阅"));
    actions->addWidget(m_world, 1);
    actions->addWidget(m_seed, 1);
    actions->addWidget(orbit);
    actions->addWidget(deorbit);
    connect(orbit, &QPushButton::clicked, this, [this] { subscribe(false); });
    connect(deorbit, &QPushButton::clicked, this, [this] { subscribe(true); });
    bodyLayout()->insertLayout(0, actions);
    connect(m_add, &QPushButton::clicked, this, &MoonTab::addMoon);
    m_poller = new JsonPoller(this, "listmoons");
    connect(m_poller, &JsonPoller::availabilityChanged, this, &DataTable::setAvailability);
    connect(m_poller, &JsonPoller::updated, this, [this](const QJsonDocument &document) {
        QList<TableRow> rows;
        for (const auto &moon : document.array()) {
            const auto object = moon.toObject();
            const QString id = object["id"].toString();
            QStringList roots, endpoints;
            for (const auto &value : object["roots"].toArray()) {
                const auto root = value.toObject();
                roots << root["identity"].toString().section(':', 0, 0);
                for (const auto &address : root["stableEndpoints"].toArray()) endpoints << address.toString();
            }
            rows.append({id, {id, roots.isEmpty() ? QStringLiteral("—") : roots.join(", "),
                endpoints.isEmpty() ? QStringLiteral("—") : endpoints.join(", ")}, object});
        }
        setRows(rows);
    });
}

void MoonTab::showEvent(QShowEvent *event)
{
    DataTable::showEvent(event);
    m_poller->refresh();
}

void MoonTab::addMoon()
{
    if (m_busy || m_main->operationBusy()) return;
    const QString source = QFileDialog::getOpenFileName(this, QStringLiteral("选择 Moon 文件"), {}, "Moon (*.moon)");
    if (source.isEmpty() || !m_main->beginOperation()) return;
    m_busy = true;
    m_add->setEnabled(false);
    ZeroTier::command(this, {"-j", "info"}, [this, source](bool ok, const QString &output) {
        QString error;
        const auto home = ok ? MoonFiles::homeDirectory(output.toUtf8(), &error) : QString();
        auto finish = [this] { m_busy = false; m_add->setEnabled(true); m_main->endOperation(); };
        if (home.isEmpty()) {
            finish();
            QMessageBox::warning(this, QStringLiteral("添加 Moon 失败"), ok ? error : QStringLiteral("无法读取节点数据目录。"));
            return;
        }
        const auto destination = QDir(home).filePath("moons.d/" + QFileInfo(source).fileName());
        if (QFileInfo(source).canonicalFilePath() == QFileInfo(destination).canonicalFilePath()) {
            finish();
            QMessageBox::information(this, QStringLiteral("添加 Moon"), QStringLiteral("该文件已在 Moon 目录中。"));
            return;
        }
        const bool overwrite = QFileInfo::exists(destination);
        const auto prompt = overwrite ? QStringLiteral("替换同名 Moon 文件并重启服务？连接会短暂中断。")
                                      : QStringLiteral("导入 Moon 文件并重启服务？连接会短暂中断。");
        if (QMessageBox::question(this, QStringLiteral("添加 Moon"), prompt, QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) != QMessageBox::Yes) { finish(); return; }
        error = MoonFiles::importFile(source, home, overwrite);
        if (!error.isEmpty()) { finish(); QMessageBox::warning(this, QStringLiteral("导入失败"), error); return; }
        m_main->endOperation();
        m_main->runService("restart", [this, home](bool restarted) {
            m_busy = false;
            m_add->setEnabled(true);
            if (!restarted) QMessageBox::warning(this, QStringLiteral("服务重启未完成"),
                QStringLiteral("Moon 文件已导入并保留，但服务重启未完成，请检查服务状态。"));
            else {
                m_main->appendOutput(QStringLiteral("Moon 文件已导入；本地文件数：%1").arg(MoonFiles::count(home)));
                m_poller->refresh();
            }
        });
    });
}

void MoonTab::subscribe(bool remove)
{
    if (m_busy || m_main->operationBusy()) return;
    const auto world = (remove && !selectedId().isEmpty() ? selectedId() : m_world->text()).trimmed().toLower();
    const auto seed = m_seed->text().trimmed().toLower();
    if (!QRegularExpression("^[0-9a-f]{1,16}$").match(world).hasMatch() ||
        (!remove && !QRegularExpression("^[0-9a-f]{10}$").match(seed).hasMatch())) {
        QMessageBox::warning(this, QStringLiteral("Moon ID"), QStringLiteral("请输入有效的 world ID；订阅还需要10位 seed 节点 ID。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("Moon 订阅"),
        remove ? QStringLiteral("取消订阅 Moon %1？").arg(world) : QStringLiteral("订阅 Moon %1？").arg(world),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (!m_main->beginOperation()) return;
    m_busy = true;
    const QStringList args = remove ? QStringList{"deorbit", world} : QStringList{"orbit", world, seed};
    ZeroTier::command(this, args, [this](bool ok, const QString &output) {
        m_busy = false;
        m_main->endOperation();
        m_main->appendOutput(output);
        if (!ok) QMessageBox::warning(this, QStringLiteral("Moon 操作未完成"), output);
        else m_poller->refresh();
    });
}
