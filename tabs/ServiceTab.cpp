#include "ServiceTab.h"
#include "../MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include <QStandardPaths>
#include <QFile>

static QString findZt()
{
    QString p = QStandardPaths::findExecutable("zerotier-cli");
    if (!p.isEmpty()) return p;
    QString fb = "C:/ProgramData/ZeroTier/One/zerotier-cli_x64.exe";
    return QFile::exists(fb) ? fb : "zerotier-cli";
}

ServiceTab::ServiceTab(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_main(mainWindow), m_proc(new QProcess(this))
{
    auto *layout = new QVBoxLayout(this);

    auto *infoLabel = new QLabel("Manage ZeroTier One service. Requires admin privileges.");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    auto *btnLayout = new QHBoxLayout;

    auto *btnStart = new QPushButton("Start");
    connect(btnStart, &QPushButton::clicked, this, [this]() {
        m_main->appendOutput(">> net start ZeroTierOneService\n");
        m_main->runService("start");
    });
    btnLayout->addWidget(btnStart);

    auto *btnStop = new QPushButton("Stop");
    connect(btnStop, &QPushButton::clicked, this, [this]() {
        m_main->appendOutput(">> net stop ZeroTierOneService\n");
        m_main->runService("stop");
    });
    btnLayout->addWidget(btnStop);

    auto *btnRestart = new QPushButton("Restart");
    connect(btnRestart, &QPushButton::clicked, this, [this]() {
        m_main->appendOutput(">> Restarting service...\n");
        m_main->runService("stop");
        m_main->runService("start");
    });
    btnLayout->addWidget(btnRestart);

    layout->addLayout(btnLayout);

    auto *btnStatus = new QPushButton("Check Status (info)");
    connect(btnStatus, &QPushButton::clicked, this, [this]() {
        if (m_proc->state() != QProcess::NotRunning) return;
        m_main->appendOutput(">> zerotier-cli info\n");
        m_proc->start(findZt(), {"info"});
    });
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]() {
        m_main->appendOutput(QString::fromLocal8Bit(m_proc->readAllStandardOutput()));
    });
    layout->addWidget(btnStatus);

    layout->addStretch();
}
