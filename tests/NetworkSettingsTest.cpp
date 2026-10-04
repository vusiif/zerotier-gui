#include "NetworkSettingsDialog.h"
#include "ZeroTierClient.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <iostream>

static void pump(int ms = 25) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }
static const QString id = "0123456789abcdef";
static QJsonObject network() {
    return {{"id", id}, {"allowManaged", true}, {"allowGlobal", false}, {"allowDefault", false},
        {"allowDNS", false}, {"name", QStringLiteral("测试网络")}, {"assignedAddresses", QJsonArray{"10.0.0.1/24"}}};
}
class FixtureClient : public ZeroTierClient {
public:
    bool admin = true, queryOK = true, corrupt = false, missing = false, hold = false, failReadback = false;
    QString failKey, ignoreKey, result = "10.0.0.1\n";
    QJsonObject state = network();
    QList<QStringList> commands;
    std::function<void(bool, QByteArray)> pending;
    bool hasAdminPrivileges() const override { return admin; }
    void run(const QStringList &args, std::function<void(bool, QByteArray)> callback) override {
        commands << args;
        bool ok = true; QByteArray bytes;
        if (args.first() == "-j") {
            ok = queryOK;
            bytes = corrupt ? QByteArray("bad json") : QJsonDocument(missing ? QJsonArray{} : QJsonArray{state}).toJson();
        } else if (args.first() == "set") {
            const auto key = args.last().section('=', 0, 0);
            ok = key != failKey;
            if (ok && key != ignoreKey) state[key] = args.last().endsWith("=1");
            if (failReadback) queryOK = false;
        } else bytes = result.toUtf8();
        if (hold) { pending = [callback, ok, bytes](bool, QByteArray) { callback(ok, bytes); }; return; }
        QTimer::singleShot(0, this, [callback, ok, bytes] { callback(ok, bytes); });
    }
    int sets() const { int n = 0; for (const auto &args : commands) if (args.first() == "set") ++n; return n; }
};
static void confirm(QMessageBox::StandardButton response = QMessageBox::Yes) {
    QTimer::singleShot(15, [response] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->button(response)->click();
    });
}
static QPushButton *button(NetworkSettingsDialog &dialog, const char *name) { return dialog.findChild<QPushButton *>(name); }
static QCheckBox *checkBox(NetworkSettingsDialog &dialog, const char *name) { return dialog.findChild<QCheckBox *>(name); }
static QString status(NetworkSettingsDialog &dialog) { return dialog.findChild<QLabel *>("networkSettingsStatus")->text(); }
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
#endif
    int failures = 0;
    auto check = [&](bool ok, const char *label) { if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; } };
    {
        FixtureClient client; NetworkSettingsDialog dialog(&client, " 0123456789ABCDEF "); dialog.show(); pump();
        check(checkBox(dialog, "allowManaged")->isChecked() && !checkBox(dialog, "allowDNS")->isChecked(), "load actual values and normalize ID");
        check(client.commands == QList<QStringList>{{"-j", "listnetworks"}}, "read before editable");
        button(dialog, "saveNetworkSettings")->click(); check(client.sets() == 0, "unchanged makes no writes");
        checkBox(dialog, "allowManaged")->setChecked(false); checkBox(dialog, "allowDNS")->setChecked(true);
        confirm(QMessageBox::No); button(dialog, "saveNetworkSettings")->click(); pump();
        check(client.sets() == 0 && !dialog.operationBusy(), "cancel saves nothing and releases busy");
        confirm(); button(dialog, "saveNetworkSettings")->click(); pump();
        check(client.commands.contains({"set", id, "allowManaged=0"}) && client.commands.contains({"set", id, "allowDNS=1"}), "only changed keys and correct arguments");
        check(status(dialog).contains(QStringLiteral("回读确认")) && !dialog.operationBusy(), "save success requires readback");
        check(client.commands.last() == QStringList{"-j", "listnetworks"}, "final read after writes");
        auto *property = dialog.findChild<QComboBox *>("networkProperty");
        property->setCurrentText("ip4"); button(dialog, "queryNetworkProperty")->click(); pump();
        check(client.commands.last() == QStringList{"get", id, "ip4"}, "get argument semantics");
        check(dialog.findChild<QPlainTextEdit *>("networkPropertyResult")->toPlainText() == "10.0.0.1", "get visible output");
        for (const auto &key : {"ip", "ip6", "ip6plane", "ip6prefix", "name"}) {
            property->setCurrentText(key); button(dialog, "queryNetworkProperty")->click(); pump();
            check(client.commands.last() == QStringList{"get", id, key}, "JSON properties and all aliases supported");
        }
        const int count = client.commands.size();
        for (const auto &invalid : {"unknown", "ip & echo", "-Tsecret", "name\ninfo"}) {
            property->setCurrentText(invalid); button(dialog, "queryNetworkProperty")->click();
        }
        check(client.commands.size() == count, "invalid or unknown properties never execute");
        property->setCurrentText("name"); client.result = "error, unknown property name";
        button(dialog, "queryNetworkProperty")->click(); pump();
        check(status(dialog).contains(QStringLiteral("查询失败")), "exit zero error text is failure");
        client.result = "unknown network ID, check that you are a member of the network";
        button(dialog, "queryNetworkProperty")->click(); pump();
        check(status(dialog).contains(QStringLiteral("查询失败")), "exit zero unknown network message is failure");
        client.result = ""; button(dialog, "queryNetworkProperty")->click(); pump();
        check(status(dialog).contains(QStringLiteral("无匹配值")), "empty IP is valid empty result");
    }
    {
        FixtureClient client; NetworkSettingsDialog dialog(&client, id); dialog.show(); pump();
        checkBox(dialog, "allowDNS")->setChecked(true); client.state["allowGlobal"] = true;
        confirm(); button(dialog, "saveNetworkSettings")->click(); pump();
        check(client.sets() == 0 && checkBox(dialog, "allowGlobal")->isChecked(), "external change stops save and reloads");
    }
    {
        FixtureClient client; NetworkSettingsDialog dialog(&client, id); dialog.show(); pump();
        checkBox(dialog, "allowGlobal")->setChecked(true); checkBox(dialog, "allowDefault")->setChecked(true); checkBox(dialog, "allowDNS")->setChecked(true);
        client.failKey = "allowDefault"; confirm(); button(dialog, "saveNetworkSettings")->click(); pump();
        check(client.sets() == 2 && client.state["allowGlobal"].toBool() && !client.state["allowDNS"].toBool(), "partial failure stops remaining writes");
        check(checkBox(dialog, "allowGlobal")->isChecked() && !checkBox(dialog, "allowDefault")->isChecked() && status(dialog).contains(QStringLiteral("保存失败")), "partial failure shows actual state");
    }
    {
        FixtureClient client; client.ignoreKey = "allowDNS";
        NetworkSettingsDialog dialog(&client, id); dialog.show(); pump();
        checkBox(dialog, "allowDNS")->setChecked(true); confirm(); button(dialog, "saveNetworkSettings")->click(); pump();
        check(status(dialog).contains(QStringLiteral("不一致")) && !checkBox(dialog, "allowDNS")->isChecked(), "exit zero without applied value is not success");
    }
    {
        FixtureClient client; NetworkSettingsDialog dialog(&client, id); dialog.show(); pump();
        checkBox(dialog, "allowDNS")->setChecked(true); client.failReadback = true;
        confirm(); button(dialog, "saveNetworkSettings")->click(); pump();
        check(status(dialog).contains(QStringLiteral("无法确认最终状态")) && !button(dialog, "saveNetworkSettings")->isEnabled(), "failed readback cannot claim success or allow stale writes");
        client.queryOK = true; button(dialog, "reloadNetworkSettings")->click(); pump();
        check(checkBox(dialog, "allowDNS")->isChecked() && button(dialog, "saveNetworkSettings")->isEnabled(), "reload restores actual value after failed readback");
    }
    {
        FixtureClient client; NetworkSettingsDialog dialog(&client, id); dialog.show(); pump();
        checkBox(dialog, "allowDNS")->setChecked(true); client.queryOK = false;
        confirm(); button(dialog, "saveNetworkSettings")->click(); pump();
        check(client.sets() == 0 && !button(dialog, "saveNetworkSettings")->isEnabled(), "failed preflight read performs no writes");
    }
    for (int mode = 0; mode < 6; ++mode) {
        FixtureClient client;
        if (mode == 0) client.queryOK = false;
        if (mode == 1) client.corrupt = true;
        if (mode == 2) client.missing = true;
        if (mode == 3) client.state.remove("allowDNS");
        if (mode == 4) client.state["allowDNS"] = "false";
        if (mode == 5) client.admin = false;
        NetworkSettingsDialog dialog(&client, id); dialog.show(); pump();
        check(!button(dialog, "saveNetworkSettings")->isEnabled(), "unreadable or unauthorized state cannot save");
        if (mode == 5) check(client.commands.isEmpty(), "denied permission executes nothing");
        client.queryOK = true; client.corrupt = false; client.missing = false; client.admin = true; client.state = network();
        button(dialog, "reloadNetworkSettings")->click(); pump();
        check(button(dialog, "saveNetworkSettings")->isEnabled(), "reload recovers after error");
    }
    {
        FixtureClient client; NetworkSettingsDialog dialog(&client, "bad"); dialog.show(); pump();
        check(client.commands.isEmpty() && !button(dialog, "saveNetworkSettings")->isEnabled(), "invalid ID rejected");
    }
    {
        FixtureClient client; client.hold = true;
        NetworkSettingsDialog dialog(&client, id); dialog.show(); pump(); dialog.close();
        check(dialog.isVisible() && dialog.operationBusy(), "cannot close during command");
        client.pending(true, {}); pump(); dialog.close(); check(!dialog.isVisible(), "can close after completion");
    }
    {
        FixtureClient client; client.hold = true;
        auto *dialog = new NetworkSettingsDialog(&client, id); dialog->show(); pump(); delete dialog;
        client.pending(true, {}); pump(); check(true, "destroyed dialog callbacks guarded");
    }
    {
        FixtureClient client; NetworkSettingsDialog dialog(&client, id); dialog.show(); pump();
        checkBox(dialog, "allowDNS")->setChecked(true);
        client.admin = false; confirm(); button(dialog, "saveNetworkSettings")->click(); pump();
        check(client.sets() == 0, "permission rechecked on save");
    }
    std::cout << "network settings failures=" << failures << '\n';
    return failures ? 1 : 0;
}
