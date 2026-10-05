#include "PeersTab.h"
#include "../ZeroTierClient.h"
#include <QJsonArray>
#include <QShowEvent>

PeersTab::PeersTab(MainWindow *mainWindow, QWidget *parent)
    : DataTable("peers", {"名称", "节点 ID", "角色", "连接", "延迟（ms）"}, parent)
{
    Q_UNUSED(mainWindow)
    m_poller = new JsonPoller(this, "listpeers");
    connect(m_poller, &JsonPoller::availabilityChanged, this, &DataTable::setAvailability);
    connect(m_poller, &JsonPoller::updated, this, [this](const QJsonDocument &document) {
        QList<TableRow> rows;
        for (const auto &peer : document.array()) {
            const auto object = peer.toObject();
            bool direct = false;
            for (const auto &path : object["paths"].toArray())
                if (path.toObject()["active"].toBool()) direct = true;
            const int latency = object["latency"].toInt(-1);
            const QString id = object["address"].toString();
            rows.append({id, {id, ZeroTier::translate(object["role"].toString()),
                ZeroTier::translate(direct ? "DIRECT" : "RELAY"),
                latency < 0 ? QStringLiteral("—") : QString::number(latency)}, object});
        }
        setRows(rows);
    });
}

void PeersTab::showEvent(QShowEvent *event)
{
    DataTable::showEvent(event);
    m_poller->refresh();
}
