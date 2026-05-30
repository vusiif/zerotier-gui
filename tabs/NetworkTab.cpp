#include "NetworkTab.h"
#include "../MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTreeWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QMessageBox>
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

NetworkTab::NetworkTab(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_main(mainWindow), m_proc(new QProcess(this))
{
    auto *layout = new QVBoxLayout(this);

    auto *joinLayout = new QHBoxLayout;
    m_input = new QLineEdit;
    m_input->setPlaceholderText("16-digit Network ID...");
    joinLayout->addWidget(m_input);

    auto *btnJoin = new QPushButton("Join");
    connect(btnJoin, &QPushButton::clicked, this, &NetworkTab::joinNetwork);
    joinLayout->addWidget(btnJoin);

    auto *btnLeave = new QPushButton("Leave");
    connect(btnLeave, &QPushButton::clicked, this, &NetworkTab::leaveNetwork);
    joinLayout->addWidget(btnLeave);

    layout->addLayout(joinLayout);

    auto *btnRefresh = new QPushButton("Refresh");
    connect(btnRefresh, &QPushButton::clicked, this, &NetworkTab::refresh);
    layout->addWidget(btnRefresh);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({"Network ID", "Name", "Status", "Type", "Assigned IPs"});
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
        QJsonArray nets = doc.array();
        for (const auto &n : nets) {
            QJsonObject o = n.toObject();
            auto *item = new QTreeWidgetItem(m_tree);
            item->setText(0, o["nwid"].toString());
            item->setText(1, o["name"].toString());
            item->setText(2, o["status"].toString());
            item->setText(3, o["type"].toString());
            QJsonArray addrs = o["assignedAddresses"].toArray();
            QStringList sl;
            for (const auto &a : addrs) sl << a.toString();
            item->setText(4, sl.join(", "));
        }
        m_tree->resizeColumnToContents(0);
        m_main->appendOutput(QString("Loaded %1 networks.\n").arg(nets.size()));
    });

    refresh();
}

void NetworkTab::refresh()
{
    if (m_proc->state() != QProcess::NotRunning) return;
    m_main->appendOutput(">> zerotier-cli -j listnetworks\n");
    m_proc->start(findZt(), {"-j", "listnetworks"});
}

void NetworkTab::joinNetwork()
{
    QString id = m_input->text().trimmed();
    if (id.isEmpty()) {
        QMessageBox::warning(this, "Input", "Enter a Network ID.");
        return;
    }
    m_main->appendOutput(">> zerotier-cli join " + id + "\n");
    auto *p = new QProcess(this);
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p]() {
        m_main->appendOutput(QString::fromLocal8Bit(p->readAllStandardOutput()));
    });
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            p, &QObject::deleteLater);
    p->start(findZt(), {"join", id});
    m_input->clear();
}

void NetworkTab::leaveNetwork()
{
    QString id = m_input->text().trimmed();
    if (id.isEmpty()) {
        QMessageBox::warning(this, "Input", "Enter a Network ID.");
        return;
    }
    m_main->appendOutput(">> zerotier-cli leave " + id + "\n");
    auto *p = new QProcess(this);
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p]() {
        m_main->appendOutput(QString::fromLocal8Bit(p->readAllStandardOutput()));
    });
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            p, &QObject::deleteLater);
    p->start(findZt(), {"leave", id});
    m_input->clear();
}
