#include "PeerPresentation.h"
#include <QJsonArray>
#include <cmath>

namespace {
QJsonObject preferredPath(const QJsonObject &peer)
{
    for (const auto &value : peer.value("paths").toArray()) {
        const auto path = value.toObject();
        if (path.value("preferred").isBool() && path.value("preferred").toBool()
            && !path.value("address").toString().isEmpty()) return path;
    }
    return {};
}
QString age(const QJsonValue &value, qint64 nowMs)
{
    if (!value.isDouble()) return QStringLiteral("未知");
    const auto timestamp = value.toDouble();
    if (!std::isfinite(timestamp) || timestamp < 0 || timestamp > 9007199254740991.0 || std::floor(timestamp) != timestamp)
        return QStringLiteral("未知");
    if (timestamp == 0) return QStringLiteral("无记录");
    if (timestamp > nowMs) return QStringLiteral("时钟异常");
    return QString::number(nowMs - static_cast<qint64>(timestamp));
}
QString flag(const QJsonValue &value)
{
    return value.isBool() ? (value.toBool() ? QStringLiteral("是") : QStringLiteral("否")) : QStringLiteral("未知");
}
}

QStringList PeerPresentation::columns(const QJsonObject &peer, qint64 nowMs)
{
    const auto preferred = preferredPath(peer);
    QStringList addresses;
    for (const auto &value : peer.value("paths").toArray()) {
        const auto address = value.toObject().value("address").toString();
        if (!address.isEmpty() && !addresses.contains(address)) addresses << address;
    }
    QString link = QStringLiteral("未知");
    // Match the CLI: no preferred path is labelled RELAY. This is not proof of reachability.
    if (peer.value("paths").isArray()) {
        if (preferred.isEmpty()) link = "RELAY";
        else if (peer.value("tunneled").isBool()) link = peer.value("tunneled").toBool() ? "RELAY" : "DIRECT";
    }
    QString latency = QStringLiteral("未知");
    const auto number = peer.value("latency");
    if (number.isDouble() && std::isfinite(number.toDouble()) && std::floor(number.toDouble()) == number.toDouble()) {
        latency = QString::number(number.toDouble(), 'f', 0);
        if (number.toDouble() < 0) latency = QStringLiteral("不可用 (%1)").arg(latency);
    }
    auto version = peer.value("version").toString();
    if (version.isEmpty() || version == "-1.-1.-1" || version == "-") version = QStringLiteral("未知");
    return {peer.value("address").toString(), peer.value("role").toString(), latency,
        preferred.isEmpty() ? addresses.join(", ") : preferred.value("address").toString(), version, link,
        age(preferred.value("lastSend"), nowMs), age(preferred.value("lastReceive"), nowMs)};
}

QString PeerPresentation::pathDetails(const QJsonObject &peer)
{
    QStringList details;
    for (const auto &value : peer.value("paths").toArray()) {
        if (!value.isObject()) continue;
        const auto path = value.toObject();
        details << QStringLiteral("%1\n首选：%2；活动：%3；过期：%4\nlastSend：%5；lastReceive：%6")
            .arg(path.value("address").toString(), flag(path.value("preferred")), flag(path.value("active")), flag(path.value("expired")),
                 path.value("lastSend").isDouble() ? QString::number(path.value("lastSend").toDouble(), 'f', 0) : QStringLiteral("未知"),
                 path.value("lastReceive").isDouble() ? QString::number(path.value("lastReceive").toDouble(), 'f', 0) : QStringLiteral("未知"));
    }
    return details.isEmpty() ? QStringLiteral("没有路径记录。") : details.join("\n\n");
}
