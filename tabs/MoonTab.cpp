#include "MoonTab.h"
#include "../MainWindow.h"
#include "../ZeroTierClient.h"
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
    actions->addStretch();
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
    const QString source = QFileDialog::getOpenFileName(this, QStringLiteral("选择 Moon 文件"), {}, "Moon (*.moon)");
    if (source.isEmpty()) return;
    const QString directory = QDir(ZeroTier::homeDirectory()).filePath("moons.d");
    const QString destination = QDir(directory).filePath(QFileInfo(source).fileName());
    if (QFileInfo(source).absoluteFilePath().compare(QFileInfo(destination).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        QMessageBox::information(this, QStringLiteral("添加 Moon"), QStringLiteral("该文件已在 Moon 目录中。"));
        return;
    }
    if (QFileInfo::exists(destination) && QMessageBox::question(this, QStringLiteral("替换 Moon 文件"),
        QStringLiteral("同名文件已存在，确定替换并重启服务？"), QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) != QMessageBox::Yes) return;
    m_add->setEnabled(false);
    const QString script = "$ErrorActionPreference = 'Stop'\ntry {\n"
        "New-Item -ItemType Directory -Path " + ZeroTier::quotePowerShell(directory) + " -Force | Out-Null\n"
        "Copy-Item -LiteralPath " + ZeroTier::quotePowerShell(source) + " -Destination " + ZeroTier::quotePowerShell(destination) + " -Force -ErrorAction Stop\n"
        "Restart-Service -Name ZeroTierOneService -ErrorAction Stop\nexit 0\n} catch { Write-Output $_; exit 1 }";
    m_main->runElevatedScript(QStringLiteral("添加 Moon 并重启服务"), script, [this](bool ok) {
        m_add->setEnabled(true);
        if (!ok) QMessageBox::warning(this, QStringLiteral("添加 Moon 失败"), QStringLiteral("文件复制或服务重启失败，请查看操作日志。"));
        else m_poller->refresh();
    });
}
