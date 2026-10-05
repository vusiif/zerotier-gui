#include "PeersTab.h"
#include "../ZeroTierClient.h"
#include <QJsonArray>
#include <QDateTime>
#include <QTreeWidget>
#include "../PeerPresentation.h"
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
            const QString id = object["address"].toString();
            const auto values = PeerPresentation::columns(object, QDateTime::currentMSecsSinceEpoch());
            rows.append({id, {id, ZeroTier::translate(object["role"].toString()),
                ZeroTier::translate(values[5]), values[2]}, object});
        }
        setRows(rows);
        for (int i = 0; i < tree()->topLevelItemCount(); ++i) {
            auto *item = tree()->topLevelItem(i);
            for (const auto &row : rows) {
                if (item->text(1) != row.id) continue;
                item->setToolTip(3, PeerPresentation::pathDetails(row.details));
                break;
            }
        }
    });
}

void PeersTab::showEvent(QShowEvent *event)
{
    DataTable::showEvent(event);
    m_poller->refresh();
}
