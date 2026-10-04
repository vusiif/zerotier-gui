#include "InfoTab.h"
#include "../MainWindow.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QTreeWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QTimer>
#include <QStandardPaths>
#include <QFile>

static QString findZt()
{
    QString p = QStandardPaths::findExecutable("zerotier-cli");
    if (!p.isEmpty()) return p;
    QString fb = "C:/ProgramData/ZeroTier/One/zerotier-cli_x64.exe";
    return QFile::exists(fb) ? fb : "zerotier-cli";
}

InfoTab::InfoTab(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_main(mainWindow), m_proc(new QProcess(this))
{
    auto *layout = new QVBoxLayout(this);

    auto *btnRefresh = new QPushButton("Refresh");
    connect(btnRefresh, &QPushButton::clicked, this, &InfoTab::refresh);
    layout->addWidget(btnRefresh);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({"Property", "Value"});
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    layout->addWidget(m_tree);

    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        QByteArray data = m_proc->readAllStandardOutput();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            m_main->appendOutput(QString::fromLocal8Bit(data));
            return;
        }

        m_tree->clear();
        QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            auto *item = new QTreeWidgetItem(m_tree);
            item->setText(0, it.key());
            QJsonValue jv = it.value();
            if (jv.isObject()) {
                item->setText(1, QString::fromUtf8(QJsonDocument(jv.toObject()).toJson(QJsonDocument::Compact)));
            } else if (jv.isArray()) {
                item->setText(1, QString::fromUtf8(QJsonDocument(jv.toArray()).toJson(QJsonDocument::Compact)));
            } else {
                item->setText(1, jv.toVariant().toString());
            }
        }
        m_tree->resizeColumnToContents(0);
        m_main->appendOutput("Node info refreshed.\n");
    });

    QTimer::singleShot(0, this, &InfoTab::refresh);
}

void InfoTab::refresh()
{
    if (m_proc->state() != QProcess::NotRunning) return;
    m_main->appendOutput(">> zerotier-cli -j info\n");
    m_proc->start(findZt(), {"-j", "info"});
}
