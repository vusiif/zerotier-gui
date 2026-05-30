#include "PeersTab.h"
#include "../MainWindow.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QTreeWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QFile>

static const char *ZTCLI = "zerotier-cli";

static QString findZt()
{
    QString p = QStandardPaths::findExecutable(ZTCLI);
    if (!p.isEmpty()) return p;
    QString fb = "C:/ProgramData/ZeroTier/One/zerotier-cli_x64.exe";
    return QFile::exists(fb) ? fb : ZTCLI;
}

PeersTab::PeersTab(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_main(mainWindow), m_proc(new QProcess(this))
{
    auto *layout = new QVBoxLayout(this);

    auto *btnRefresh = new QPushButton("Refresh");
    connect(btnRefresh, &QPushButton::clicked, this, &PeersTab::refresh);
    layout->addWidget(btnRefresh);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({"Address", "Role", "Latency", "Path", "Version"});
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    layout->addWidget(m_tree);

    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        QByteArray data = m_proc->readAllStandardOutput();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isArray()) {
            m_main->appendOutput(QString::fromLocal8Bit(data));
            return;
        }

        m_tree->clear();
        QJsonArray peers = doc.array();
        for (const auto &p : peers) {
            QJsonObject o = p.toObject();
            auto *item = new QTreeWidgetItem(m_tree);
            item->setText(0, o["address"].toString());
            item->setText(1, o["role"].toString());
            item->setText(2, QString::number(o["latency"].toInt()));
            QJsonArray paths = o["paths"].toArray();
            item->setText(3, paths.isEmpty() ? "" : paths[0].toObject()["address"].toString());
            item->setText(4, o["version"].toString());
        }
        m_tree->resizeColumnToContents(0);
        m_main->appendOutput(QString("Loaded %1 peers.\n").arg(peers.size()));
    });

    refresh();
}

void PeersTab::refresh()
{
    if (m_proc->state() != QProcess::NotRunning) return;
    m_main->appendOutput(">> zerotier-cli -j listpeers\n");
    m_proc->start(findZt(), {"-j", "listpeers"});
}
