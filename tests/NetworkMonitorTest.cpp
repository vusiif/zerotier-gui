#include "NetworkMonitor.h"
#include "ZeroTierClient.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

static void pump(int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
class FixtureClient : public ZeroTierClient {
public:
    QByteArray data = "[]";
    bool queryOK = true, leaveOK = true, hold = false;
    QList<QStringList> commands;
    std::function<void(bool, QByteArray)> pending;
    void run(const QStringList &args, std::function<void(bool, QByteArray)> callback) override {
        commands << args;
        if (hold) { pending = std::move(callback); return; }
        const bool query = args.first() == "-j";
        const auto bytes = query ? data : QByteArray("200 leave OK");
        const bool ok = query ? queryOK : leaveOK;
        QTimer::singleShot(0, this, [callback, bytes, ok] { callback(ok, bytes); });
    }
    int leaves(const QString &id) const { return commands.count(QStringList{"leave", id}); }
};
static QByteArray row(const QString &id, const QString &status) {
    return QJsonDocument(QJsonArray{QJsonObject{{"nwid", id}, {"status", status}}}).toJson();
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *name) { if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } };
    const QString id = "0123456789abcdef", other = "fedcba9876543210";
    {
        FixtureClient client; NetworkMonitor monitor(&client, nullptr, {}, 10);
        int left = 0; QObject::connect(&monitor, &NetworkMonitor::automaticallyLeft, [&](QString value) { check(value == id, "correct network left"); ++left; });
        client.data = row(id, "NOT_FOUND"); monitor.track(" 0123456789ABCDEF "); pump(60);
        check(left == 1 && client.leaves(id) == 1 && !monitor.tracks(id), "NOT_FOUND exits once and stops tracking");
        check(client.leaves(other) == 0, "existing unrelated networks untouched");
    }
    for (const auto &status : {"OK", "ACCESS_DENIED", "REQUESTING_CONFIGURATION", "PORT_ERROR", "CLIENT_TOO_OLD"}) {
        FixtureClient client; NetworkMonitor monitor(&client, nullptr, {}, 10);
        QString observed; QObject::connect(&monitor, &NetworkMonitor::statusChanged, [&](QString, QString value) { observed = value; });
        client.data = row(id, status); monitor.track(id); pump(35);
        check(observed == status && monitor.tracks(id) && !client.leaves(id), "non-NOT_FOUND status retained and refreshed");
        check(client.commands.size() >= 2 && client.commands.size() < 8, "polling continues at bounded interval");
    }
    for (const auto &payload : {QByteArray("bad JSON"), QByteArray("{}"), QByteArray("[]"), row(other, "NOT_FOUND"), QByteArray("[{\"nwid\":\"0123456789abcdef\"}]")}) {
        FixtureClient client; NetworkMonitor monitor(&client, nullptr, {}, 10);
        client.data = payload; monitor.track(id); pump(35);
        check(monitor.tracks(id) && client.leaves(id) == 0, "missing or invalid status never exits network");
    }
    {
        FixtureClient client; NetworkMonitor monitor(&client, nullptr, {}, 10);
        client.queryOK = false; client.data = row(id, "NOT_FOUND"); monitor.track(id); pump(30);
        check(client.leaves(id) == 0, "failed query cannot trigger leave");
        client.queryOK = true; pump(40); check(client.leaves(id) == 1, "query retry recovers");
    }
    {
        FixtureClient client; NetworkMonitor monitor(&client, nullptr, {}, 10);
        int failuresShown = 0;
        QObject::connect(&monitor, &NetworkMonitor::automaticLeaveFailed, [&](QString, QString message) {
            ++failuresShown; check(!message.isEmpty(), "leave failure explained to GUI");
        });
        client.leaveOK = false; client.data = row(id, "NOT_FOUND"); monitor.track(id); pump(150);
        check(client.leaves(id) == 3 && failuresShown == 3 && monitor.tracks(id), "failed leave retries at most three times");
        client.leaveOK = true; monitor.track(id); pump(40);
        check(client.leaves(id) == 4 && !monitor.tracks(id), "new join resets retry budget");
    }
    {
        FixtureClient client; bool allowed = false;
        NetworkMonitor monitor(&client, nullptr, [&] { return allowed; }, 10);
        client.data = row(id, "NOT_FOUND"); monitor.track(id); pump(30);
        check(client.commands.isEmpty(), "busy service or modal gate pauses polling");
        allowed = true; pump(40); check(client.leaves(id) == 1, "polling resumes when allowed");
    }
    {
        FixtureClient client; NetworkMonitor monitor(&client, nullptr, {}, 10);
        client.hold = true; monitor.track(id); pump(30);
        check(client.commands.size() == 1, "pending query cannot overlap");
        auto callback = std::move(client.pending);
        monitor.forget(id); monitor.track(id);
        callback(true, row(id, "NOT_FOUND")); pump(5);
        check(client.leaves(id) == 0, "old query cannot exit a newly rejoined network");
        monitor.forget(id);
        if (client.pending) { auto last = std::move(client.pending); last(true, row(id, "NOT_FOUND")); }
        pump(20); check(client.leaves(id) == 0, "manual leave prevents delayed auto-leave");
    }
    {
        FixtureClient client; NetworkMonitor monitor(&client, nullptr, {}, 10);
        monitor.track("invalid;whoami"); pump(20);
        check(client.commands.isEmpty(), "invalid tracking ID rejected");
    }
    std::cout << (failures ? "Network monitor checks failed" : "Network monitor checks passed") << '\n';
    return failures ? 1 : 0;
}
