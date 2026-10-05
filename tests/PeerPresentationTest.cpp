#include "PeerPresentation.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *name) { if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } };
    const qint64 now = 1800000000000LL;
    QJsonObject preferred{{"address", "192.0.2.2/9993"}, {"preferred", true}, {"active", true}, {"expired", false},
        {"lastSend", now - 1500}, {"lastReceive", now - 2500}};
    QJsonObject other{{"address", "2001:db8::1/9993"}, {"preferred", false}, {"lastSend", now - 10}};
    QJsonObject peer{{"address", "abcdef1234"}, {"role", "LEAF"}, {"latency", 12}, {"version", "1.16.2"},
        {"tunneled", false}, {"paths", QJsonArray{other, preferred}}};
    auto row = PeerPresentation::columns(peer, now);
    check(row.size() == 8 && row[0] == "abcdef1234" && row[2] == "12" && row[4] == "1.16.2", "columns preserve existing metadata");
    check(row[3] == "192.0.2.2/9993" && row[5] == "DIRECT" && row[6] == "1500" && row[7] == "2500", "preferred path supplies link and age");
    check(PeerPresentation::columns(peer, now + 1000)[6] == "2500", "unchanged payload age advances with time");
    check(PeerPresentation::pathDetails(peer).contains("2001:db8::1/9993") && PeerPresentation::pathDetails(peer).contains(QStringLiteral("活动：是")), "all paths retained in tooltip");
    peer["tunneled"] = true;
    check(PeerPresentation::columns(peer, now)[5] == "RELAY", "tunneled preferred path uses relay");
    peer.remove("tunneled");
    check(PeerPresentation::columns(peer, now)[5] == QStringLiteral("未知"), "missing tunnel flag not guessed as direct");
    peer["tunneled"] = false; preferred["active"] = false; preferred["expired"] = true;
    peer["paths"] = QJsonArray{preferred};
    check(PeerPresentation::columns(peer, now)[5] == "DIRECT" && PeerPresentation::pathDetails(peer).contains(QStringLiteral("过期：是")), "CLI classification preserved with inactive expired path explicitly disclosed");
    peer["paths"] = QJsonArray{other};
    row = PeerPresentation::columns(peer, now);
    check(row[5] == "RELAY" && row[6] == QStringLiteral("未知") && row[3] == "2001:db8::1/9993", "no preferred path matches CLI relay without borrowing other path time");
    peer["paths"] = QJsonArray{};
    check(PeerPresentation::columns(peer, now)[5] == "RELAY", "empty paths match CLI relay");
    peer.remove("paths");
    check(PeerPresentation::columns(peer, now)[5] == QStringLiteral("未知"), "missing paths are unknown");
    for (const QJsonValue value : {QJsonValue(), QJsonValue("123"), QJsonValue(-1), QJsonValue(1.5), QJsonValue(1e20)}) {
        preferred["lastSend"] = value; peer["paths"] = QJsonArray{preferred};
        check(PeerPresentation::columns(peer, now)[6] == QStringLiteral("未知"), "invalid timestamp is unknown");
    }
    preferred["lastSend"] = 0; preferred["lastReceive"] = now + 1; peer["paths"] = QJsonArray{preferred};
    row = PeerPresentation::columns(peer, now);
    check(row[6] == QStringLiteral("无记录") && row[7] == QStringLiteral("时钟异常"), "zero and future timestamps distinct");
    peer["latency"] = -488; peer["version"] = "-1.-1.-1";
    row = PeerPresentation::columns(peer, now);
    check(row[2] == QStringLiteral("不可用 (-488)") && row[4] == QStringLiteral("未知"), "negative latency and unknown version explicit");
    peer.remove("latency"); check(PeerPresentation::columns(peer, now)[2] == QStringLiteral("未知"), "missing latency not zero");
    std::cout << "peer presentation failures=" << failures << '\n';
    return failures ? 1 : 0;
}
