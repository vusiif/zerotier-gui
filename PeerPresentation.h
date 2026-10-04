#pragma once
#include <QJsonObject>
#include <QStringList>

namespace PeerPresentation {
// Column order keeps the original ID/role/latency/path/version columns stable.
QStringList columns(const QJsonObject &peer, qint64 nowMs);
QString pathDetails(const QJsonObject &peer);
}
