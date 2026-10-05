#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.contains("uninstall") || arguments.contains("install")) {
        std::puts("Test package command");
        return qEnvironmentVariableIntValue("ZT_TEST_WINGET_CODE");
    }
    const auto mode = qEnvironmentVariable("ZT_TEST_MODE");
    if (mode == "slow") QThread::msleep(6500);
    if (mode == "error") { std::puts("service unavailable"); return 1; }
    if (mode == "malformed") { std::puts("invalid json"); return 0; }
    QJsonDocument document;
    if (arguments.contains("info")) {
        document = QJsonDocument(QJsonObject{{"address", "0123456789"}, {"version", "1.16.0"},
            {"online", true}, {"tcpFallbackActive", false}, {"extraField", "preserved"}});
    } else if (arguments.contains("listnetworks")) {
        QJsonArray networks;
        for (int i = 0; i < 40; ++i) {
            networks.append(QJsonObject{{"nwid", QString("8056c2e21c%1").arg(i, 6, 16, QChar('0'))},
                {"name", QStringLiteral("团队网络")}, {"status", i % 3 ? "OK" : "ACCESS_DENIED"},
                {"type", "PRIVATE"}, {"assignedAddresses", QJsonArray{QString("10.24.0.%1/24").arg(i + 1)}},
                {"routes", QJsonArray{QJsonObject{{"target", "10.24.0.0/24"}, {"via", QJsonValue::Null}}}},
                {"mtu", 2800}});
        }
        document = QJsonDocument(networks);
    } else if (arguments.contains("listpeers")) {
        QJsonArray peers;
        for (int i = 0; i < 35; ++i)
            peers.append(QJsonObject{{"address", QString("%1").arg(i + 1, 10, 16, QChar('0'))},
                {"role", i == 0 ? "PLANET" : i == 1 ? "MOON" : "LEAF"}, {"latency", 12 + i},
                {"paths", i % 2 ? QJsonArray{} : QJsonArray{QJsonObject{{"address", "192.0.2.1/9993"}, {"active", true}}}},
                {"versionMajor", 1}, {"versionMinor", 16}, {"versionRev", 0}});
        document = QJsonDocument(peers);
    } else if (arguments.contains("listmoons")) {
        document = QJsonDocument(QJsonArray{QJsonObject{{"id", "0000000123456789"},
            {"roots", QJsonArray{QJsonObject{{"identity", "0123456789:0:public-key"},
                {"stableEndpoints", QJsonArray{"192.0.2.10/9993"}}}}}, {"timestamp", 12345678}}});
    } else {
        std::puts("200 command OK");
        return 0;
    }
    const auto bytes = document.toJson(QJsonDocument::Compact);
    std::fwrite(bytes.constData(), 1, bytes.size(), stdout);
    return 0;
}
