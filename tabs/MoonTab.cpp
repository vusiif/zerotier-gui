#include "MoonTab.h"
#include "../MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

static const char *ZTCLI = "zerotier-cli";

static QString findZt()
{
    QString p = QStandardPaths::findExecutable(ZTCLI);
    if (!p.isEmpty()) return p;
    QString fb = "C:/ProgramData/ZeroTier/One/zerotier-cli_x64.exe";
    return QFile::exists(fb) ? fb : ZTCLI;
}

MoonTab::MoonTab(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_main(mainWindow), m_proc(new QProcess(this))
{
    auto *layout = new QVBoxLayout(this);

    auto *infoLabel = new QLabel("Add Moon nodes by selecting .moon files.");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    auto *btnLayout = new QHBoxLayout;

    auto *btnAdd = new QPushButton("Add Moon File...");
    connect(btnAdd, &QPushButton::clicked, this, &MoonTab::addMoon);
    btnLayout->addWidget(btnAdd);

    auto *btnRefresh = new QPushButton("Refresh");
    connect(btnRefresh, &QPushButton::clicked, this, &MoonTab::refreshMoonList);
    btnLayout->addWidget(btnRefresh);

    layout->addLayout(btnLayout);

    m_moonList = new QListWidget;
    layout->addWidget(m_moonList);

    QTimer::singleShot(0, this, &MoonTab::refreshMoonList);
}

void MoonTab::addMoon()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "Select Moon File", QString(), "Moon Files (*.moon);;All Files (*)");
    if (filePath.isEmpty()) return;

    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, filePath]() {
        QByteArray data = proc->readAllStandardOutput();
        proc->deleteLater();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            m_main->appendOutput("Failed to get ZeroTier home directory.\n");
            return;
        }

        QString homeDir = doc.object()["config"].toObject()["settings"].toObject()["homeDir"].toString();
        if (homeDir.isEmpty()) {
            m_main->appendOutput("Could not determine ZeroTier home directory.\n");
            return;
        }

        QString moonDir = homeDir + "/moon.d";
        QDir dir(moonDir);
        if (!dir.exists()) dir.mkpath(".");

        QString destFile = moonDir + "/" + QFileInfo(filePath).fileName();
        if (QFile::exists(destFile)) QFile::remove(destFile);

        if (QFile::copy(filePath, destFile)) {
            m_main->appendOutput("Moon file copied: " + destFile + "\n");
            m_main->appendOutput("Restarting service...\n");
            m_main->runService("stop");
            m_main->runService("start");
            refreshMoonList();
        } else {
            m_main->appendOutput("Failed to copy moon file.\n");
        }
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
        m_main->appendOutput("[ERROR] " + QString::fromLocal8Bit(proc->readAllStandardError()));
    });
    proc->start(findZt(), {"-j", "info"});
}

void MoonTab::refreshMoonList()
{
    m_moonList->clear();

    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc]() {
        QByteArray data = proc->readAllStandardOutput();
        proc->deleteLater();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

        QString homeDir = doc.object()["config"].toObject()["settings"].toObject()["homeDir"].toString();
        if (homeDir.isEmpty()) return;

        QDir dir(homeDir + "/moon.d");
        if (!dir.exists()) {
            m_moonList->addItem("(No moon.d directory)");
            return;
        }

        QStringList moons = dir.entryList({"*.moon"}, QDir::Files);
        if (moons.isEmpty()) {
            m_moonList->addItem("(No moon nodes)");
        } else {
            m_moonList->addItems(moons);
        }
        m_main->appendOutput(QString("Found %1 moon node(s).\n").arg(moons.size()));
    });
    proc->start(findZt(), {"-j", "info"});
}
