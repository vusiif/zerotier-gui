#pragma once
#include <QTemporaryDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "ArtPanel.h"
#include "ServiceStartup.h"
#include "ServiceControl.h"
#include "NetworkSettingsDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include "PeerCacheMaintenance.h"
#include "ZeroTierUninstaller.h"

class PrivilegedFixtureClient : public ZeroTierClient {
public:
    explicit PrivilegedFixtureClient(const QString &program) : ZeroTierClient(nullptr, program, 2000) {}
    bool hasAdminPrivileges() const override { return true; }
};

class FixtureServiceControl : public ServiceControl {
public:
    QStringList actions;
    bool succeeds = true;
    QString state = "running";
    void run(const QString &action, std::function<void(bool, QString)> callback) override {
        actions << action;
        const bool ok = succeeds;
        if (ok) state = action == "stop" ? "stopped" : "running";
        QTimer::singleShot(0, this, [callback, ok] { callback(ok, ok ? QString{} : QStringLiteral("模拟重启失败")); });
    }
};
class FixtureUninstaller : public ZeroTierUninstaller {
public:
    int calls = 0; bool succeeds = false;
    void run(std::function<void(bool, QString)> callback) override {
        ++calls; const bool ok = succeeds;
        QTimer::singleShot(20, this, [callback, ok] { callback(ok, ok ? QStringLiteral("模拟卸载完成") : QStringLiteral("模拟卸载失败")); });
    }
};
// All child commands execute this test binary. They cannot change ZeroTier.
static int functionalChecks()
{
    int failed = 0;
    auto check = [&](bool ok, const char *name) {
        if (!ok) { ++failed; std::cerr << "FAIL: " << name << '\n'; }
    };
    QTemporaryDir directory;
    const QString payload = directory.filePath("payload.json");
    const QString history = directory.filePath("commands.jsonl");
    qputenv("ZT_TEST_PAYLOAD", payload.toUtf8());
    qputenv("ZT_TEST_HISTORY", history.toUtf8());
    auto write = [&](const QByteArray &data) { QFile f(payload); check(f.open(QIODevice::WriteOnly), "fixture writable"); f.write(data); };
    write("{}");
    {
        auto *starting = new ServiceStartup(nullptr, [] { return QString("pending"); }, [] { return QString{}; }, 50, 2);
        MainWindow pendingWindow(nullptr, new PrivilegedFixtureClient(QCoreApplication::applicationFilePath()), starting);
        pendingWindow.show(); pump(10);
        check(pendingWindow.property("startupBusy").toBool(), "startup marks GUI busy while waiting");
        for (auto *b : pendingWindow.findChildren<QPushButton *>())
            if (b->property("managementAction").toBool()) check(!b->isEnabled(), "startup disables management actions");
        pump(70);
        check(!pendingWindow.property("startupBusy").toBool(), "startup timeout clears GUI busy");
        check(pendingWindow.findChild<QPlainTextEdit *>("applicationLog")->toPlainText().contains(QStringLiteral("超时")), "startup failure visible in log");
        pendingWindow.close();
    }
    QSettings settings("ZeroTierGui", "ZeroTierGui");
    const bool hadDirectory = settings.contains("dataDirectory");
    const auto oldDirectory = settings.value("dataDirectory");
    settings.setValue("dataDirectory", directory.path());
    auto *client = new PrivilegedFixtureClient(QCoreApplication::applicationFilePath());
    auto *control = new FixtureServiceControl;
    auto *cacheMaintenance = new PeerCacheMaintenance(control, nullptr, [control] { return control->state; });
    auto *uninstaller = new FixtureUninstaller;
    MainWindow window(nullptr, client, nullptr, control, cacheMaintenance, uninstaller);
    window.show(); pump(200);
    auto wait = [&] {
        for (int i = 0; i < 100 && client->busy(); ++i) pump(20);
        check(!client->busy(), "fixture command completed"); pump(30);
    };
    wait();
    auto page = [&](int index) -> QWidget * {
        for (auto *w : window.findChildren<QWidget *>())
            if (w->property("pageIndex").isValid() && w->property("pageIndex").toInt() == index) return w;
        return nullptr;
    };
    auto open = [&](int index) { auto *p = page(index); p->setEnabled(true); window.navigation(p->property("ElaPageKey").toString()); wait(); return p; };
    auto button = [&](QWidget *p, const QString &text) -> QPushButton * {
        for (auto *b : p->findChildren<QPushButton *>()) if (b->text() == text) return b;
        check(false, "action button exists"); return nullptr;
    };
    auto refresh = [&](QWidget *p) { button(p, QStringLiteral("刷新"))->click(); wait(); };
    auto records = [&] {
        QList<QJsonArray> entries;
        QFile f(history); if (f.open(QIODevice::ReadOnly))
            for (const auto &line : f.readAll().split('\n')) if (!line.isEmpty()) entries << QJsonDocument::fromJson(line).array();
        return entries;
    };
    auto hasCommand = [&](const QStringList &args) {
        for (const auto &entry : records()) {
            QStringList actual; for (const auto &v : entry) actual << v.toString();
            if (actual == args) return true;
        }
        return false;
    };
    QTimer answer;
    QString selectedFile;
    auto response = QMessageBox::No;
    QObject::connect(&answer, &QTimer::timeout, [&] {
        if (auto *w = QApplication::activeModalWidget()) {
            if (auto *message = qobject_cast<QMessageBox *>(w)) {
                if (message->windowTitle() == QStringLiteral("重启使 Moon 生效"))
                    check(window.property("importBusy").toBool(), "restart confirmation keeps background queries paused");
                if (auto *choice = message->button(response)) choice->click();
                else if (auto *ok = message->button(QMessageBox::Ok)) ok->click();
            }
            else if (auto *dialog = qobject_cast<QFileDialog *>(w); dialog && !selectedFile.isEmpty()) {
                dialog->selectFile(selectedFile);
                if (auto *filename = dialog->findChild<QLineEdit *>("fileNameEdit")) filename->setText(selectedFile);
                if (!dialog->selectedFiles().isEmpty()) QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
            }
        }
    });
    answer.start(20);
    const QByteArray network = R"([{"nwid":"0123456789abcdef","name":"测试网络","status":"OK","type":"PRIVATE","assignedAddresses":["10.0.0.2/24","fd00::2/64"]},{"nwid":"fedcba9876543210","name":"second","status":"OK"}])";
    write(network);
    auto *net = open(1);
    auto *tree = net->findChild<QTreeWidget *>();
    check(tree && tree->topLevelItemCount() == 2, "network JSON populates rows");
    check(tree->topLevelItem(0)->text(1) == QStringLiteral("测试网络"), "Unicode network name retained");
    check(tree->topLevelItem(0)->text(4).contains("fd00::2"), "IPv4 and IPv6 addresses displayed");
    tree->setCurrentItem(tree->topLevelItem(0));
    auto *input = net->findChild<QLineEdit *>("networkIdInput");
    check(input->text() == "0123456789abcdef", "selected network fills ID input");
    auto *oldItem = tree->topLevelItem(0); refresh(net);
    check(tree->topLevelItem(0) == oldItem, "unchanged JSON preserves model items");
    auto changed = network; changed.replace("second", "renamed"); write(changed); refresh(net);
    check(tree->currentItem() && tree->currentItem()->text(0) == "0123456789abcdef", "selection restored after update");
    const auto beforeInvalid = records().size();
    input->setText("bad; whoami"); button(net, QStringLiteral("加入"))->click();
    check(records().size() == beforeInvalid, "invalid network ID never executes");
    input->setText(" 0123456789abcdef "); button(net, QStringLiteral("加入"))->click(); wait();
    check(hasCommand({"join", "0123456789abcdef"}), "join passes trimmed ID as separate argument");
    const auto beforeCancel = records().size();
    button(net, QStringLiteral("离开"))->click();
    check(records().size() == beforeCancel, "cancelled leave never executes");
    response = QMessageBox::Yes; button(net, QStringLiteral("离开"))->click(); wait();
    check(hasCommand({"leave", "0123456789abcdef"}), "confirmed leave arguments"); response = QMessageBox::No;
    write("[]"); refresh(net);
    check(tree->topLevelItemCount() == 0 && tree->isHidden(), "empty network list hides empty table");
    check(net->findChild<QWidget *>("pageArtwork")->height() == 200, "empty list expands illustration");
    write("malformed JSON"); refresh(net);
    check(net->findChild<QLabel *>("resultStatus")->text().contains("JSON"), "malformed JSON reported");
    write("{}"); refresh(net);
    check(net->findChild<QLabel *>("resultStatus")->text().contains("JSON"), "wrong JSON shape rejected");
    write(network); refresh(net); check(!tree->isHidden() && tree->topLevelItemCount() == 2, "valid data recovers after errors");
    write(R"([{"id":"0123456789abcdef","name":"测试网络","allowManaged":true,"allowGlobal":false,"allowDefault":false,"allowDNS":false}])");
    input->setText("0123456789abcdef");
    QTimer settingsDriver; int settingsPhase = 0, settingsTicks = 0;
    QObject::connect(&settingsDriver, &QTimer::timeout, [&] {
        auto *dialog = window.findChild<NetworkSettingsDialog *>();
        if (!dialog) return;
        if (++settingsTicks > 350 && !dialog->operationBusy()) { check(false, "settings integration timeout"); dialog->close(); return; }
        if (dialog->operationBusy()) return;
        if (settingsPhase == 0) {
            check(window.property("settingsBusy").toBool(), "settings locks background operations for whole dialog");
            auto *toggle = dialog->findChild<QCheckBox *>("allowDNS");
            check(toggle->isEnabled(), "real subprocess settings loaded"); toggle->setChecked(true);
            settingsPhase = 1; response = QMessageBox::Yes;
            dialog->findChild<QPushButton *>("saveNetworkSettings")->click();
        } else if (settingsPhase == 1) {
            check(dialog->findChild<QLabel *>("networkSettingsStatus")->text().contains(QStringLiteral("回读确认")), "real subprocess save readback");
            check(hasCommand({"set", "0123456789abcdef", "allowDNS=1"}), "GUI set process arguments");
            settingsPhase = 2;
            dialog->findChild<QComboBox *>("networkProperty")->setCurrentText("ip4");
            dialog->findChild<QPushButton *>("queryNetworkProperty")->click();
        } else if (settingsPhase == 2) {
            settingsPhase = 3;
            check(dialog->findChild<QPlainTextEdit *>("networkPropertyResult")->toPlainText() == "10.0.0.2", "GUI get process result");
            check(hasCommand({"get", "0123456789abcdef", "ip4"}), "GUI get process arguments");
            const auto oldTheme = eTheme->getThemeMode();
            eTheme->setThemeMode(ElaThemeType::Dark); pump(30);
            check(dialog->palette().color(QPalette::WindowText).lightness() > 160, "settings dark text visible");
            check(dialog->grab().save(QDir::currentPath() + "/ui-dark-network-settings.png"), "capture dark settings");
            eTheme->setThemeMode(ElaThemeType::Light); pump(30);
            check(dialog->palette().color(QPalette::WindowText).lightness() < 80, "settings light text visible");
            check(dialog->grab().save(QDir::currentPath() + "/ui-light-network-settings.png"), "capture light settings");
            eTheme->setThemeMode(oldTheme); settingsPhase = 3; dialog->close();
        }
    });
    settingsDriver.start(20); button(net, QStringLiteral("网络设置 / 属性查询"))->click(); settingsDriver.stop();
    response = QMessageBox::No; wait();
    check(settingsPhase == 3 && !window.property("settingsBusy").toBool(), "settings close releases management gate");
    write(R"([{"address":"abcdef1234","role":"LEAF","latency":12,"version":"1.16","paths":[{"address":"192.0.2.1/9993"}]}])");
    auto *peers = open(2)->findChild<QTreeWidget *>();
    check(peers && peers->topLevelItem(0)->text(2) == "12" && peers->topLevelItem(0)->text(3) == "192.0.2.1/9993", "peer latency and paths rendered");
    const auto peerTime = QDateTime::currentMSecsSinceEpoch();
    const QJsonObject bestPath{{"address", "192.0.2.2/9993"}, {"preferred", true}, {"active", true}, {"expired", false},
        {"lastSend", peerTime - 1500}, {"lastReceive", 0}};
    const QJsonObject peerObject{{"address", "abcdef1234"}, {"role", "LEAF"}, {"latency", -488}, {"version", "-1.-1.-1"},
        {"tunneled", false}, {"paths", QJsonArray{QJsonObject{{"address", "2001:db8::1/9993"}}, bestPath}}};
    write(QJsonDocument(QJsonArray{peerObject}).toJson()); refresh(page(2));
    auto *peerItem = peers->topLevelItem(0); peers->setCurrentItem(peerItem);
    check(peers->columnCount() == 8 && peerItem->text(5) == "DIRECT" && peerItem->text(3) == "192.0.2.2/9993", "GUI shows preferred path and direct link");
    check(peerItem->text(2).contains(QStringLiteral("不可用")) && peerItem->text(7) == QStringLiteral("无记录"), "GUI unknown receive and unusable latency");
    check(peerItem->toolTip(3).contains("2001:db8::1/9993"), "GUI tooltip retains other paths");
    const auto oldAge = peerItem->text(6).toLongLong(); pump(60); refresh(page(2));
    check(peers->topLevelItem(0) == peerItem && peers->currentItem() == peerItem && peerItem->text(6).toLongLong() > oldAge, "unchanged peer JSON updates relative ages without rebuilding selection");
    auto relayed = peerObject; relayed["tunneled"] = true;
    write(QJsonDocument(QJsonArray{relayed}).toJson()); refresh(page(2));
    check(peers->topLevelItem(0)->text(5) == "RELAY", "GUI tunnel uses relay classification");
    window.setNavigationBarDisplayMode(ElaNavigationType::Compact); pump(30);
    check(window.grab().save(QDir::currentPath() + "/ui-peers-details.png"), "capture peers detail columns");
    window.setNavigationBarDisplayMode(ElaNavigationType::Maximal); pump(30);
    write(R"([{"id":"1234","roots":[{"identity":"example","stableEndpoints":["192.0.2.2/9993"]}]}])");
    auto *moon = open(3);
    check(moon->findChild<QTreeWidget *>()->topLevelItem(0)->text(1).contains("stableEndpoints"), "nested Moon JSON rendered");
    auto *moonInput = moon->findChild<QLineEdit *>("moonIdInput");
    auto *seed = moon->findChild<QLineEdit *>("moonSeedInput");
    moonInput->setText("1234"); seed->setText("bad");
    const auto beforeSeed = records().size(); button(moon, QStringLiteral("加入"))->click();
    check(records().size() == beforeSeed, "invalid Moon seed never executes");
    seed->setText("abcdef1234"); button(moon, QStringLiteral("加入"))->click(); wait();
    check(hasCommand({"orbit", "1234", "abcdef1234"}), "orbit includes world and seed IDs");
    response = QMessageBox::Yes; button(moon, QStringLiteral("离开"))->click(); wait(); response = QMessageBox::No;
    check(hasCommand({"deorbit", "1234"}), "deorbit arguments");
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto actualHome = directory.filePath("service-home");
    QDir().mkpath(actualHome);
    qputenv("ZT_TEST_HOME", actualHome.toUtf8());
    selectedFile = directory.filePath("sample.moon");
    { QFile f(selectedFile); check(f.open(QIODevice::WriteOnly), "Moon fixture writable"); f.write("moon-fixture"); }
    button(moon, QStringLiteral("导入 .moon 文件"))->click(); wait();
    QFile imported(actualHome + "/moons.d/sample.moon");
    check(imported.open(QIODevice::ReadOnly) && imported.readAll() == "moon-fixture", "Moon import writes exact bytes to reported homeDir");
    check(!QFile::exists(directory.filePath("moons.d/sample.moon")), "reported homeDir overrides configured guess");
    check(control->actions.isEmpty(), "cancel restart preserves file without restarting");
    imported.close();
    { QFile f(selectedFile); check(f.open(QIODevice::WriteOnly), "Moon fixture writable"); f.write("changed"); }
    button(moon, QStringLiteral("导入 .moon 文件"))->click(); wait();
    check(imported.open(QIODevice::ReadOnly), "imported Moon readable"); check(imported.readAll() == "moon-fixture", "cancel overwrite preserves existing Moon file"); imported.close();
    response = QMessageBox::Yes; button(moon, QStringLiteral("导入 .moon 文件"))->click(); wait(); response = QMessageBox::No;
    check(imported.open(QIODevice::ReadOnly), "imported Moon readable");
    const auto overwritten = imported.readAll();
    check(overwritten == "changed", "confirmed Moon overwrite succeeds"); imported.close();
    check(control->actions == QStringList{"restart"}, "confirmed import restarts service once");
    selectedFile = directory.filePath("empty.moon");
    { QFile f(selectedFile); check(f.open(QIODevice::WriteOnly), "empty Moon fixture writable"); }
    button(moon, QStringLiteral("导入 .moon 文件"))->click(); wait();
    check(!QFile::exists(actualHome + "/moons.d/empty.moon"), "empty Moon file rejected");
    selectedFile = directory.filePath("large.moon");
    { QFile f(selectedFile); check(f.open(QIODevice::WriteOnly), "large Moon fixture writable"); f.resize(1024 * 1024 + 1); }
    button(moon, QStringLiteral("导入 .moon 文件"))->click(); wait();
    check(!QFile::exists(actualHome + "/moons.d/large.moon"), "oversized Moon file rejected");
    selectedFile = directory.filePath("failed-restart.moon");
    { QFile f(selectedFile); check(f.open(QIODevice::WriteOnly), "restart fixture writable"); f.write("retained"); }
    control->succeeds = false; response = QMessageBox::Yes;
    button(moon, QStringLiteral("导入 .moon 文件"))->click(); wait(); response = QMessageBox::No;
    check(QFile::exists(actualHome + "/moons.d/failed-restart.moon"), "restart failure retains imported file");
    check(moon->findChild<QLabel *>("resultStatus")->text().contains(QStringLiteral("重启失败")), "restart failure explicitly shown");
    check(moon->findChild<QLabel *>("moonFilesStatus")->text().contains("2"), "local Moon count refreshed");
    qputenv("ZT_TEST_HOME", directory.filePath("nonexistent-home").toUtf8());
    selectedFile = directory.filePath("invalid-home.moon");
    { QFile f(selectedFile); check(f.open(QIODevice::WriteOnly), "invalid home fixture writable"); f.write("not-imported"); }
    button(moon, QStringLiteral("导入 .moon 文件"))->click(); wait();
    check(!QFile::exists(actualHome + "/moons.d/invalid-home.moon"), "invalid homeDir cannot silently fall back");
    check(control->actions.size() == 2, "invalid file or home does not restart service");
    check(window.grab().save(QDir::currentPath() + "/ui-moon-import.png"), "capture Moon directory and import result");
    qunsetenv("ZT_TEST_HOME");
    auto *peerPage = open(2);
    control->succeeds = true;
    qputenv("ZT_TEST_HOME", actualHome.toUtf8());
    QDir().mkpath(actualHome + "/peers.d");
    { QFile f(actualHome + "/peers.d/cache"); check(f.open(QIODevice::WriteOnly), "peer cache fixture writable"); f.write("cached"); }
    const auto actionsBeforeCache = control->actions.size();
    button(peerPage, QStringLiteral("清理 Peers 缓存并重启服务"))->click(); wait();
    check(QFile::exists(actualHome + "/peers.d/cache") && control->actions.size() == actionsBeforeCache, "cache cancel leaves service and files unchanged");
    response = QMessageBox::Yes;
    button(peerPage, QStringLiteral("清理 Peers 缓存并重启服务"))->click(); wait(); response = QMessageBox::No;
    const auto cacheBackups = QDir(actualHome).entryList({"peers.d.backup-*"}, QDir::Dirs);
    check(cacheBackups.size() == 1 && QFile::exists(actualHome + "/" + cacheBackups.first() + "/cache"), "GUI cache cleanup retains backup in actual homeDir");
    check(control->actions.mid(actionsBeforeCache) == QStringList{"stop", "start"} && !window.property("maintenanceBusy").toBool(), "GUI cache workflow stops and starts mock service and releases busy");
    check(window.grab().save(QDir::currentPath() + "/ui-peer-cache-maintenance.png"), "capture cache backup result");
    qunsetenv("ZT_TEST_HOME");
    auto *service = open(4);
    button(service, QStringLiteral("停止服务"))->click(); button(service, QStringLiteral("重启服务"))->click();
    check(!window.property("serviceBusy").toBool(), "cancel service mutations leaves service untouched");
    auto *logs = window.findChild<QPlainTextEdit *>("applicationLog");
    auto *settingsPage = logs->parentWidget();
    window.navigation(settingsPage->property("ElaPageKey").toString()); pump(40);
    selectedFile = directory.filePath("export.log");
    button(settingsPage, QStringLiteral("导出日志"))->click();
    QFile exported(selectedFile);
    check(exported.open(QIODevice::ReadOnly) && exported.readAll().contains("zerotier-cli"), "logs export real command history");
    button(settingsPage, QStringLiteral("清空日志"))->click();
    check(logs->toPlainText().isEmpty(), "clear logs empties document");
    selectedFile.clear();
    auto *helpPage = window.findChild<QWidget *>("helpPage");
    check(helpPage, "help page exists"); window.navigation(helpPage->property("ElaPageKey").toString()); pump(80);
    auto *links = helpPage->findChild<QLabel *>("projectLinks");
    check(links->text().contains("https://github.com/vusiif/zerotier-gui") && links->text().contains("https://gitee.com/vusiif/zerotier-gui"), "both verified project repository links in help");
    auto *helpOutput = helpPage->findChild<QPlainTextEdit *>("helpOutput");
    auto *exportDiagnostics = helpPage->findChild<QPushButton *>("exportDiagnostics");
    check(!exportDiagnostics->isEnabled(), "no report cannot export");
    button(helpPage, QStringLiteral("CLI 帮助"))->click(); wait();
    check(hasCommand({"-h"}) && helpOutput->toPlainText().contains("Available switches"), "GUI CLI help captures stderr on exit zero");
    button(helpPage, QStringLiteral("CLI 版本"))->click(); wait();
    check(hasCommand({"-v"}) && helpOutput->toPlainText().contains("1.16.2-fixture"), "GUI CLI version");
    const auto beforeDumpCancel = records().size();
    button(helpPage, QStringLiteral("生成诊断报告"))->click();
    check(records().size() == beforeDumpCancel, "cancel diagnosis executes nothing");
    qputenv("ZT_TEST_DUMP", directory.filePath("zerotier_dump.txt").toUtf8()); response = QMessageBox::Yes;
    button(helpPage, QStringLiteral("生成诊断报告"))->click(); wait();
    check(hasCommand({"dump"}) && exportDiagnostics->isEnabled() && helpOutput->toPlainText().contains(QStringLiteral("测试节点")), "Windows file-based dump preview");
    const auto fullReport = helpPage->property("diagnosticReport").toString();
    selectedFile = directory.filePath("diagnostics.txt");
    exportDiagnostics->click();
    QFile diagnosticExport(selectedFile);
    check(diagnosticExport.open(QIODevice::ReadOnly) && diagnosticExport.readAll() == fullReport.toUtf8(), "complete diagnosis exported as UTF8");
    diagnosticExport.close(); selectedFile.clear();
    button(helpPage, QStringLiteral("CLI 帮助"))->click(); wait();
    check(helpPage->property("diagnosticReport").toString() == fullReport && exportDiagnostics->isEnabled(), "help output does not replace cached diagnostic report");
    qputenv("ZT_TEST_DUMP_MODE", "inline"); button(helpPage, QStringLiteral("生成诊断报告"))->click(); wait();
    check(exportDiagnostics->isEnabled() && helpOutput->toPlainText().startsWith("platform:"), "inline diagnostic preview");
    const auto priorTheme = eTheme->getThemeMode(); eTheme->setThemeMode(ElaThemeType::Dark); pump(80);
    check(links->palette().color(QPalette::WindowText).lightness() > 160, "help dark text readable");
    check(links->palette().color(QPalette::Link).lightness() > 160, "dark repository links readable");
    check(links->text().contains("color:#7cbcff"), "rich text anchors use explicit dark color");
    check(window.grab().save(QDir::currentPath() + "/ui-help-dark.png"), "capture dark help");
    eTheme->setThemeMode(ElaThemeType::Light); pump(80);
    check(window.grab().save(QDir::currentPath() + "/ui-help-light.png"), "capture light help");
    eTheme->setThemeMode(priorTheme);
    for (const auto &mode : {"fail", "bad"}) {
        qputenv("ZT_TEST_DUMP_MODE", mode); button(helpPage, QStringLiteral("生成诊断报告"))->click(); wait();
        check(!exportDiagnostics->isEnabled() && helpPage->property("diagnosticReport").toString().isEmpty(), "failed dump discards old report and disables export");
    }
    qunsetenv("ZT_TEST_DUMP"); qunsetenv("ZT_TEST_DUMP_MODE");
    response = QMessageBox::No; answer.stop();
    write(R"({"address":"abcdef1234","online":true,"version":"fixture"})");
    auto *overview = open(0)->findChild<QTreeWidget *>();
    check(overview && overview->topLevelItemCount() == 3, "info object renders attributes");
    // Drive the real GUI join action, then leave the network page and minimize it.
    const QString absentId = "1111222233334444";
    write(R"([{"nwid":"1111222233334444","status":"NOT_FOUND"}])");
    open(1); input->setText(absentId);
    button(net, QStringLiteral("加入"))->click(); wait();
    window.navigation(settingsPage->property("ElaPageKey").toString());
    window.showMinimized();
    for (int i = 0; i < 70 && !hasCommand({"leave", absentId}); ++i) pump(100);
    wait();
    check(hasCommand({"leave", absentId}), "GUI automatically leaves NOT_FOUND after navigation and minimization");
    check(net->findChild<QLabel *>("networkTrackingStatus")->text().contains(QStringLiteral("已自动退出")), "automatic leave result remains visible in network status");
    window.showNormal(); window.navigation(settingsPage->property("ElaPageKey").toString());
    pump(40); check(window.grab().save(QDir::currentPath() + "/ui-settings-uninstall.png"), "capture uninstall entry");
    answer.start(20);
    button(settingsPage, QStringLiteral("卸载 ZeroTier（winget）"))->click();
    check(uninstaller->calls == 0, "uninstall cancellation never invokes winget");
    response = QMessageBox::Yes;
    button(settingsPage, QStringLiteral("卸载 ZeroTier（winget）"))->click();
    check(window.property("maintenanceBusy").toBool(), "uninstall locks management during operation"); pump(80);
    check(uninstaller->calls == 1 && !window.property("installationRemoved").toBool() && !window.property("maintenanceBusy").toBool(), "uninstall failure leaves management usable");
    uninstaller->succeeds = true; button(settingsPage, QStringLiteral("卸载 ZeroTier（winget）"))->click(); pump(80);
    check(window.property("installationRemoved").toBool(), "confirmed removal disables installed workflows");
    for (auto *action : window.findChildren<QPushButton *>())
        if (action->property("managementAction").toBool()) check(!action->isEnabled(), "management actions disabled after uninstall");
    answer.stop();
    window.close();
    check(!window.isVisible(), "window can close after successful uninstall");
    if (hadDirectory) settings.setValue("dataDirectory", oldDirectory); else settings.remove("dataDirectory");
    qunsetenv("ZT_TEST_PAYLOAD"); qunsetenv("ZT_TEST_HISTORY");
    std::cout << (failed ? "Functional checks failed" : "Functional checks passed") << '\n';
    return failed;
}
