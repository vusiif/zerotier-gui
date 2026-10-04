#include "ZeroTierClient.h"
#include "MainWindow.h"
#include "NetworkSettingsDialog.h"
#include "PeerPresentation.h"
#include "DiagnosticReport.h"
#include "PeerCacheMaintenance.h"
#include "ZeroTierUninstaller.h"
#include <QDesktopServices>
#include <QUrl>
#include "ArtPanel.h"
#include "ServiceStartup.h"
#include "NetworkMonitor.h"
#include "MoonFiles.h"
#include "ServiceControl.h"
#include "ElaPushButton.h"
#include "ElaTheme.h"
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QMessageBox>
#include <QTextCursor>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProgressBar>
#include <QDateTime>
#include <QMap>
#include <QCryptographicHash>
#include <QApplication>
#include <QPalette>

static QString cliPath() { return ZeroTierClient::executable(); }

MainWindow::MainWindow(QWidget *parent, ZeroTierClient *client, ServiceStartup *startup, ServiceControl *control, PeerCacheMaintenance *cache, ZeroTierUninstaller *uninstaller) : ElaWindow(parent)
{
    setWindowTitle("ZeroTier GUI");
    resize(960, 700);
    setUserInfoCardVisible(false);
    setWindowButtonFlag(ElaAppBarType::RouteBackButtonHint, false);
    setWindowButtonFlag(ElaAppBarType::RouteForwardButtonHint, false);
    setStackSwitchMode(ElaWindowType::None);
    setThemeChangeTime(0);
    m_client = client ? client : new ZeroTierClient(this);
    m_client->setParent(this);
    m_status = new QLabel(QStringLiteral("正在检查 ZeroTier 状态…"));
    m_status->setWordWrap(true);
    m_output = new QPlainTextEdit;
    m_output->setObjectName("applicationLog");
    m_output->setReadOnly(true);
    m_output->document()->setMaximumBlockCount(300);
    connect(m_client, &ZeroTierClient::log, this, &MainWindow::appendOutput);
    connect(m_client, &ZeroTierClient::busyChanged, this, [this](bool busy) {
        updateActions();
        if (!busy) QTimer::singleShot(0, this, [this] {
            for (auto *page : m_pages) if (page->property("refreshPending").toBool()) {
                page->setProperty("refreshPending", false);
                if (page->property("ElaPageKey").toString() == getCurrentNavigationPageKey()) refreshPage(page);
            }
        });
    });
    m_serviceControl = control ? control : new ServiceControl(this);
    m_serviceControl->setParent(this);
    m_cache = cache ? cache : new PeerCacheMaintenance(m_serviceControl, this); m_cache->setParent(this);
    m_uninstaller = uninstaller ? uninstaller : new ZeroTierUninstaller(this); m_uninstaller->setParent(this);
    connect(m_uninstaller, &ZeroTierUninstaller::log, this, &MainWindow::appendOutput);
    setupManagement();
    m_startup = startup ? startup : new ServiceStartup(this);
    m_startup->setParent(this);
    connect(m_startup, &ServiceStartup::busyChanged, this, [this](bool busy) {
        setProperty("startupBusy", busy); updateActions();
    });
    m_networkMonitor = new NetworkMonitor(m_client, this, [this] {
        return m_client->hasAdminPrivileges() && !property("serviceBusy").toBool()
            && !(property("startupBusy").toBool() || property("importBusy").toBool() || property("settingsBusy").toBool() || property("maintenanceBusy").toBool() || property("installationRemoved").toBool()) && !QApplication::activeModalWidget();
    });
    connect(m_networkMonitor, &NetworkMonitor::diagnostic, this, &MainWindow::appendOutput);
    connect(m_networkMonitor, &NetworkMonitor::automaticLeaveFailed, this, [this](const QString &, const QString &message) {
        for (auto *page : m_pages) if (page->property("command").toString() == "listnetworks")
            if (auto *label = page->findChild<QLabel *>("networkTrackingStatus")) label->setText(message);
    });
    connect(m_networkMonitor, &NetworkMonitor::statusChanged, this, [this](const QString &id, const QString &state) {
        for (auto *page : m_pages) if (page->property("command").toString() == "listnetworks") {
            if (auto *label = page->findChild<QLabel *>("networkTrackingStatus")) {
                QString text = state;
                if (state == "ACCESS_DENIED") text += QStringLiteral("（等待网络管理员授权）");
                else if (state == "NOT_FOUND") text += QStringLiteral("（网络不存在，正在自动退出）");
                else if (state == "OK") text += QStringLiteral("（网络配置已就绪）");
                label->setText(QStringLiteral("网络 %1：%2").arg(id, text));
            }
            if (auto *tree = page->findChild<QTreeWidget *>()) {
                const auto items = tree->findItems(id, Qt::MatchExactly, 0);
                if (!items.isEmpty()) items.first()->setText(2, state);
            }
        }
    });
    connect(m_networkMonitor, &NetworkMonitor::automaticallyLeft, this, [this](const QString &id) {
        for (auto *page : m_pages) if (page->property("command").toString() == "listnetworks") {
            if (auto *label = page->findChild<QLabel *>("networkTrackingStatus"))
                label->setText(QStringLiteral("网络 %1 不存在，已自动退出。").arg(id));
            if (page->property("ElaPageKey").toString() == getCurrentNavigationPageKey())
                QTimer::singleShot(0, this, [this, page] { refreshPage(page); });
        }
    });
    QTimer::singleShot(0, this, &MainWindow::initializeService);
}
void MainWindow::initializeService()
{
    if (!m_client->hasAdminPrivileges()) { detect(); return; }
    m_status->setText(QStringLiteral("正在确认并启动 ZeroTier 服务…"));
    m_startup->ensureRunning([this](bool ok, QString error) {
        if (ok) detect();
        else { m_status->setText(error); appendOutput(error + '\n'); }
    });
}
void MainWindow::appendOutput(const QString &text)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text.right(16384));
    if (m_output->document()->characterCount() > 131072) {
        QTextCursor cursor(m_output->document());
        cursor.setPosition(0);
        cursor.setPosition(m_output->document()->characterCount() - 65536, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
}
void MainWindow::detect()
{
    if (m_client->busy() || (property("serviceBusy").toBool() || property("startupBusy").toBool() || property("importBusy").toBool() || property("settingsBusy").toBool() || property("maintenanceBusy").toBool() || property("installationRemoved").toBool())) return;
    const auto path = cliPath();
    const auto serviceState = ZeroTierClient::serviceStatus();
    for (auto *page : m_pages) page->setEnabled(!path.isEmpty() || serviceState != "missing");
    if (path.isEmpty()) {
        m_status->setText(QStringLiteral("检测到服务或无法查询服务，但未找到 ZeroTier 程序。请在设置页指定程序位置。")); return;
    }
    if (serviceState == "stopped") {
        m_status->setText(QStringLiteral("ZeroTier 已安装，但服务已停止。请在服务页启动。")); return;
    }
    if (serviceState == "pending") {
        m_status->setText(QStringLiteral("ZeroTier 服务正在切换状态，请稍后重新检测。")); return;
    }
    if (!m_client->hasAdminPrivileges()) {
        m_status->setText(QStringLiteral("ZeroTier 已安装。读取与管理节点需要管理员权限，请使用管理页面的提权入口。")); return;
    }
    m_status->setText(QStringLiteral("正在读取 ZeroTier 节点状态…"));
    m_client->run({"-j", "info"}, [this](bool ok, QByteArray bytes) {
        const auto doc = QJsonDocument::fromJson(bytes);
        if (!ok || !doc.isObject()) {
            m_status->setText(QStringLiteral("ZeroTier 已安装，但节点接口读取失败。请检查服务、数据目录及日志。"));
            appendOutput(QString::fromLocal8Bit(bytes)); return;
        }
        updateMoonDirectory(bytes);
        const auto info = doc.object();
        m_status->setText(QStringLiteral("节点：%1\n版本：%2\n连接状态：%3").arg(info.value("address").toString(), info.value("version").toString(),
            info.value("online").toBool() ? QStringLiteral("在线") : QStringLiteral("离线，正在尝试连接")));
    });
}
#include "ElaLineEdit.h"
#include <QTreeWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QFileDialog>
#include <QSaveFile>
#include <QFile>
#include <QCloseEvent>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

static void fillJson(QTreeWidget *tree, const QJsonValue &value)
{
    tree->clear();
    const bool object = value.isObject();
    tree->setHeaderLabels(object ? QStringList{QStringLiteral("属性"), QStringLiteral("值")} : QStringList{QStringLiteral("条目"), QStringLiteral("详情")});
    auto display = [](const QJsonValue &v) {
        if (v.isObject()) return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        if (v.isArray()) return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        return v.toVariant().toString();
    };
    if (object) {
        const auto obj = value.toObject();
        for (auto i = obj.begin(); i != obj.end(); ++i) new QTreeWidgetItem(tree, {i.key(), display(i.value())});
    } else {
        for (const auto &v : value.toArray()) {
            const auto o = v.toObject();
            QString id = o.value("nwid").toString(o.value("address").toString());
            if (id.isEmpty()) id = o.value("id").toVariant().toString();
            new QTreeWidgetItem(tree, {id, display(v)});
        }
    }
    tree->resizeColumnToContents(0);
}
QWidget *MainWindow::createPage(const QString &title, QVBoxLayout **layout)
{
    auto *page = new QWidget;
    auto *box = new QVBoxLayout(page);
    box->setContentsMargins(32, 24, 32, 24);
    auto *label = new QLabel(title);
    auto font = label->font(); font.setPointSize(22); font.setBold(true); label->setFont(font);
    box->addWidget(label);
    const bool overview = title == QStringLiteral("概览");
    const bool settings = title == QStringLiteral("设置与日志") || title == QStringLiteral("帮助与诊断");
    const QMap<QString, QString> captions{
        {QStringLiteral("概览"), QStringLiteral("连接你的世界")},
        {QStringLiteral("网络"), QStringLiteral("让距离不再成为阻碍")},
        {QStringLiteral("Peers"), QStringLiteral("发现与你相连的节点")},
        {QStringLiteral("Moon"), QStringLiteral("为连接点亮一盏灯")},
        {QStringLiteral("服务"), QStringLiteral("保持连接，随时启程")},
        {QStringLiteral("设置与日志"), QStringLiteral("按你的习惯运行")},
        {QStringLiteral("帮助与诊断"), QStringLiteral("找到答案，保持连接")}};
    auto *art = new ArtPanel(overview || settings ? "hero" : "detail", captions.value(title), page);
    art->setProperty("defaultCaption", captions.value(title));
    art->setFixedHeight(overview ? 220 : (settings ? 100 : 140));
    box->addWidget(art);
    auto *permission = new ElaPushButton(QStringLiteral("以管理员身份重新打开"));
    permission->setObjectName("permissionButton");
    permission->setVisible(!m_client->hasAdminPrivileges());
    permission->setMaximumWidth(300);
    box->addWidget(permission, 0, Qt::AlignLeft);
    connect(permission, &QPushButton::clicked, this, &MainWindow::elevate);
    const QMap<QString, ElaIconType::IconName> icons{
        {QStringLiteral("概览"), ElaIconType::House},
        {QStringLiteral("网络"), ElaIconType::NetworkWired},
        {QStringLiteral("Peers"), ElaIconType::Users},
        {QStringLiteral("Moon"), ElaIconType::Moon},
        {QStringLiteral("服务"), ElaIconType::Server},
        {QStringLiteral("设置与日志"), ElaIconType::Gear}};
    addPageNode(title, page, icons.value(title, ElaIconType::CircleInfo));
    m_pages.append(page);
    *layout = box;
    return page;
}
void MainWindow::setupManagement()
{
    const QStringList titles = {QStringLiteral("概览"), QStringLiteral("网络"), QStringLiteral("Peers"), QStringLiteral("Moon"), QStringLiteral("服务")};
    const QStringList commands = {"info", "listnetworks", "listpeers", "listmoons", "service"};
    for (int index = 0; index < titles.size(); ++index) {
        QVBoxLayout *layout;
        auto *page = createPage(titles[index], &layout);
        if (index == 0) layout->addWidget(m_status);
        if (index == 3) {
            auto *files = new QLabel(QStringLiteral("待读取服务数据目录"));
            files->setObjectName("moonFilesStatus"); files->setWordWrap(true);
            layout->addWidget(files);
        }
        page->setProperty("command", commands[index]);
        page->setProperty("pageIndex", index);
    }
    QVBoxLayout *settingsLayout;
    auto *settingsPage = createPage(QStringLiteral("设置与日志"), &settingsLayout);
    m_pages.removeOne(settingsPage);
    auto *cliSelect = new ElaPushButton(QStringLiteral("指定 ZeroTier 程序位置"));
    cliSelect->setProperty("managementAction", true);
    settingsLayout->addWidget(cliSelect);
    connect(cliSelect, &QPushButton::clicked, this, [this] {
        if (m_client->busy()) return;
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 zerotier-one 或 zerotier-cli 程序"), {}, "ZeroTier (*.exe)");
        if (path.isEmpty()) return;
        const auto name = QFileInfo(path).fileName().toLower();
        if (name != "zerotier-one_x64.exe" && name != "zerotier-one_x86.exe" && name != "zerotier-one.exe" && name != "zerotier-cli.exe" && name != "zerotier-cli_x64.exe") {
            QMessageBox::warning(this, QStringLiteral("程序不匹配"), QStringLiteral("请选择 ZeroTier 原生程序。")); return;
        }
        QSettings("ZeroTierGui", "ZeroTierGui").setValue("cliPath", path);
        detect();
    });
    auto *directorySelect = new ElaPushButton(QStringLiteral("指定 ZeroTier 数据目录"));
    directorySelect->setProperty("managementAction", true);
    settingsLayout->addWidget(directorySelect);
    connect(directorySelect, &QPushButton::clicked, this, [this] {
        if (m_client->busy()) return;
        auto path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择服务使用的 ZeroTier 数据目录"), ZeroTierClient::dataDirectory());
        if (path.isEmpty()) return;
        if (!QFileInfo::exists(path + "/identity.public")) {
            QMessageBox::warning(this, QStringLiteral("目录不匹配"), QStringLiteral("此目录没有 identity.public，请选择服务实际使用的数据目录。")); return;
        }
        QSettings("ZeroTierGui", "ZeroTierGui").setValue("dataDirectory", path);
        m_client->setHomeDirectory({});
        detect();
    });
    auto *resetPaths = new ElaPushButton(QStringLiteral("恢复自动查找与默认数据目录"));
    resetPaths->setProperty("managementAction", true);
    settingsLayout->addWidget(resetPaths);
    connect(resetPaths, &QPushButton::clicked, this, [this] {
        if (m_client->busy()) return;
        QSettings settings("ZeroTierGui", "ZeroTierGui");
        settings.remove("cliPath"); settings.remove("dataDirectory"); m_client->setHomeDirectory({}); detect();
    });
    settingsLayout->addWidget(m_output, 1);
    auto *clear = new ElaPushButton(QStringLiteral("清空日志")); settingsLayout->addWidget(clear);
    connect(clear, &QPushButton::clicked, m_output, &QPlainTextEdit::clear);
    auto *exportLog = new ElaPushButton(QStringLiteral("导出日志"));
    settingsLayout->addWidget(exportLog);
    connect(exportLog, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("保存日志"), "zerotier-gui.log", "Log (*.log)");
        if (path.isEmpty()) return;
        QSaveFile file(path);
        const auto bytes = m_output->toPlainText().toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            QMessageBox::warning(this, QStringLiteral("保存失败"), file.errorString());
    });
    auto *uninstall = new ElaPushButton(QStringLiteral("卸载 ZeroTier（winget）"));
    uninstall->setObjectName("uninstallZeroTier"); uninstall->setProperty("managementAction", true); settingsLayout->addWidget(uninstall);
    connect(uninstall, &QPushButton::clicked, this, [this] {
        if (!allowOperation()) return;
        if (QMessageBox::question(this, QStringLiteral("卸载 ZeroTier"), QStringLiteral("卸载 ZeroTier 将中断所有 ZeroTier 网络连接。程序不会主动删除数据目录，但无法保证卸载器保留配置。是否通过 winget 卸载 ZeroTier，并接受来源协议？")) != QMessageBox::Yes) return;
        if (!allowOperation()) return;
        setProperty("maintenanceBusy", true); updateActions(); appendOutput(QStringLiteral("正在卸载 ZeroTier…\n"));
        m_uninstaller->run([this](bool ok, QString message) {
            if (ok) { setProperty("installationRemoved", true); for (auto *page : m_pages) page->setEnabled(false); }
            setProperty("maintenanceBusy", false); updateActions();
            m_status->setText(message); appendOutput(message + '\n');
            QMessageBox::information(this, ok ? QStringLiteral("卸载完成") : QStringLiteral("卸载未完成"), message);
        });
    });
    setupHelp();
    connect(this, &ElaWindow::navigationNodeClicked, this, [this](ElaNavigationType::NavigationNodeType, const QString &key) {
        for (auto *page : m_pages) if (page->property("ElaPageKey").toString() == key && page->isEnabled()) {
            populatePage(page);
            if (m_client->busy()) page->setProperty("refreshPending", true);
            else refreshPage(page);
        }
    });
    auto *timer = new QTimer(this); timer->setInterval(10000);
    connect(timer, &QTimer::timeout, this, [this] {
        if (!isVisible() || isMinimized() || m_client->busy() || (property("startupBusy").toBool() || property("importBusy").toBool() || property("settingsBusy").toBool() || property("maintenanceBusy").toBool() || property("installationRemoved").toBool())) return;
        for (auto *page : m_pages) if (page->property("ElaPageKey").toString() == getCurrentNavigationPageKey() && page->isEnabled()) refreshPage(page);
    });
    timer->start();
    QSettings settings("ZeroTierGui", "ZeroTierGui");
    eTheme->setThemeMode(settings.value("dark", false).toBool() ? ElaThemeType::Dark : ElaThemeType::Light);
    applyTheme();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this](ElaThemeType::ThemeMode mode) {
        applyTheme();
        QSettings("ZeroTierGui", "ZeroTierGui").setValue("dark", mode == ElaThemeType::Dark);
    });
}
void MainWindow::setupHelp()
{
    QVBoxLayout *layout;
    auto *page = createPage(QStringLiteral("帮助与诊断"), &layout);
    page->setObjectName("helpPage"); m_pages.removeOne(page);
    auto *guide = new QLabel(QStringLiteral("先在概览确认服务状态，再到网络页输入 16 位网络 ID 加入。ACCESS_DENIED 通常需要网络管理员授权；NOT_FOUND 会自动退出本次加入的网络。网络设置可查询 IP 并调整地址、路由和 DNS。Moon 文件导入后需重启服务。"));
    guide->setWordWrap(true); layout->addWidget(guide);
    const auto linkMarkup = QStringLiteral("项目源码与反馈：<a style=\"color:%1\" href=\"https://github.com/vusiif/zerotier-gui\">GitHub · vusiif/zerotier-gui</a>&nbsp;&nbsp;|&nbsp;&nbsp;<a style=\"color:%1\" href=\"https://gitee.com/vusiif/zerotier-gui\">Gitee · vusiif/zerotier-gui</a>");
    auto *links = new QLabel(linkMarkup.arg("#0067b8")); links->setProperty("linkMarkup", linkMarkup);
    links->setObjectName("projectLinks"); links->setWordWrap(true); links->setTextFormat(Qt::RichText);
    links->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(links, &QLabel::linkActivated, this, [this](const QString &url) {
        if (url != "https://github.com/vusiif/zerotier-gui" && url != "https://gitee.com/vusiif/zerotier-gui") return;
        if (!QDesktopServices::openUrl(QUrl(url))) QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("无法打开浏览器，请复制仓库链接。"));
    });
    layout->addWidget(links);
    auto *actions = new QHBoxLayout;
    auto *help = new ElaPushButton(QStringLiteral("CLI 帮助")); help->setObjectName("readCliHelp");
    auto *version = new ElaPushButton(QStringLiteral("CLI 版本")); version->setObjectName("readCliVersion");
    auto *dump = new ElaPushButton(QStringLiteral("生成诊断报告")); dump->setObjectName("generateDiagnostics");
    for (auto *button : {help, version, dump}) { button->setProperty("managementAction", true); actions->addWidget(button); }
    layout->addLayout(actions);
    auto *status = new QLabel(QStringLiteral("选择帮助、版本或诊断操作。"));
    status->setObjectName("helpStatus"); status->setTextFormat(Qt::PlainText); status->setWordWrap(true); layout->addWidget(status);
    auto *output = new QPlainTextEdit; output->setObjectName("helpOutput"); output->setReadOnly(true);
    layout->addWidget(output, 1);
    auto *exportReport = new ElaPushButton(QStringLiteral("导出诊断报告"));
    exportReport->setObjectName("exportDiagnostics"); exportReport->setProperty("managementAction", true);
    exportReport->setEnabled(false); layout->addWidget(exportReport);
    auto query = [this, page, status, output](const QString &argument) {
        if (!allowOperation(false)) return;
        status->setText(QStringLiteral("正在读取…"));
        m_client->run({argument}, [this, page, status, output](bool ok, QByteArray bytes) {
            if (!ok) { status->setText(m_client->lastError()); return; }
            QString text = QString::fromUtf8(bytes);
            if (text.contains(QChar::ReplacementCharacter)) text = QString::fromLocal8Bit(bytes);
            output->setPlainText(text);
            status->setText(page->property("diagnosticReport").toString().isEmpty() ? QStringLiteral("读取完成。") : QStringLiteral("读取完成；之前生成的诊断报告仍可导出。"));
        });
    };
    connect(help, &QPushButton::clicked, this, [query] { query("-h"); });
    connect(version, &QPushButton::clicked, this, [query] { query("-v"); });
    connect(dump, &QPushButton::clicked, this, [this, page, status, output, exportReport] {
        if (!allowOperation()) return;
        if (QMessageBox::question(this, QStringLiteral("生成诊断报告"), QStringLiteral("报告包含 IP、节点及本地配置信息，分享前请检查内容。ZeroTier CLI 可能覆盖桌面的 zerotier_dump.txt。是否继续？")) != QMessageBox::Yes) return;
        if (!allowOperation()) return;
        // Discard old reports before a new attempt so a failure cannot export stale data.
        page->setProperty("diagnosticReport", QString{}); exportReport->setEnabled(false);
        output->clear(); status->setText(QStringLiteral("正在生成诊断报告…"));
        const auto started = QDateTime::currentMSecsSinceEpoch();
        m_client->run({"dump"}, [this, page, status, output, exportReport, started](bool ok, QByteArray bytes) {
            if (!ok) { status->setText(m_client->lastError()); return; }
            const auto report = DiagnosticReport::read(bytes, started);
            if (!report.error.isEmpty()) { status->setText(report.error); return; }
            page->setProperty("diagnosticReport", report.text);
            output->setPlainText(report.text); exportReport->setEnabled(true);
            status->setText(QStringLiteral("报告已生成。导出的是完整报告，请检查内容后再分享。"));
        });
    });
    connect(exportReport, &QPushButton::clicked, this, [this, page, status] {
        const auto report = page->property("diagnosticReport").toString();
        if (report.isEmpty() || !allowOperation(false)) return;
        const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("保存诊断报告"), "zerotier-diagnostics.txt", "Text (*.txt)");
        if (path.isEmpty()) return;
        QString error;
        if (!DiagnosticReport::save(path, report, &error)) status->setText(QStringLiteral("导出失败：%1").arg(error));
        else status->setText(QStringLiteral("诊断报告已导出到 %1").arg(path));
    });
}
void MainWindow::populatePage(QWidget *page)
{
    if (page->property("loaded").toBool()) return;
    page->setProperty("loaded", true);
    const int index = page->property("pageIndex").toInt();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    auto *status = new QLabel(QStringLiteral("点击刷新，或在页面打开时自动读取。"));
    status->setObjectName("resultStatus"); status->setWordWrap(true); layout->addWidget(status);
    auto *refresh = new ElaPushButton(QStringLiteral("刷新"));
    refresh->setMaximumWidth(140); layout->addWidget(refresh, 0, Qt::AlignLeft);
    connect(refresh, &QPushButton::clicked, this, [this, page] { refreshPage(page); });
    if (index == 2) {
        auto *hint = new QLabel(QStringLiteral("DIRECT / RELAY 与 CLI 的链路分类一致，不代表节点当前可达。发送 / 接收列为距首选路径上次收发的毫秒数；悬停路径可查看全部记录。"));
        hint->setWordWrap(true); layout->addWidget(hint);
        auto *clearPeers = new ElaPushButton(QStringLiteral("清理 Peers 缓存并重启服务"));
        clearPeers->setObjectName("clearPeerCache"); layout->addWidget(clearPeers);
        auto *maintenanceStatus = new QLabel; maintenanceStatus->setObjectName("peerMaintenanceStatus");
        maintenanceStatus->setTextFormat(Qt::PlainText); maintenanceStatus->setWordWrap(true); layout->addWidget(maintenanceStatus);
        connect(clearPeers, &QPushButton::clicked, this, [this, page] {
            if (!allowOperation()) return;
            setProperty("maintenanceBusy", true); updateActions();
            auto done = [this, page](bool ok, const QString &message) {
                setProperty("maintenanceBusy", false); updateActions(); appendOutput(message + '\n');
                page->findChild<QLabel *>("peerMaintenanceStatus")->setText(message);
                if (ok) refreshPage(page);
            };
            m_client->run({"-j", "info"}, [this, done](bool ok, QByteArray bytes) {
                if (!ok) { done(false, m_client->lastError()); return; }
                QString error; const auto home = MoonFiles::homeDirectory(bytes, &error);
                if (home.isEmpty()) { done(false, error); return; }
                error = PeerCacheMaintenance::validate(home);
                if (!error.isEmpty()) { done(false, error); return; }
                m_client->setHomeDirectory(home);
                if (QMessageBox::question(this, QStringLiteral("清理 Peers 缓存"), QStringLiteral("将暂时停止服务，把以下缓存目录改名备份，再启动服务：\n%1\n\n这会中断连接，不删除身份、网络或 Moon 配置。是否继续？").arg(QDir(home).filePath("peers.d"))) != QMessageBox::Yes) {
                    done(false, QStringLiteral("已取消清理缓存。")); return;
                }
                m_cache->run(home, done);
            });
        });
    }
    if (index == 1 || index == 3) {
        if (index == 1) {
            auto *tracking = new QLabel(QStringLiteral("加入后将持续跟踪网络状态；不存在的网络会自动退出。"));
            tracking->setObjectName("networkTrackingStatus"); tracking->setWordWrap(true);
            layout->addWidget(tracking);
        }
        auto *input = new ElaLineEdit;
        input->setObjectName(index == 1 ? "networkIdInput" : "moonIdInput");
        input->setPlaceholderText(index == 1 ? QStringLiteral("16 位十六进制网络 ID") : QStringLiteral("Moon world ID（十六进制）"));
        layout->addWidget(input);
        auto *seed = new ElaLineEdit;
        seed->setObjectName("moonSeedInput");
        if (index == 3) { seed->setPlaceholderText(QStringLiteral("Seed 节点 ID（10 位十六进制）")); layout->addWidget(seed); }
        else { delete seed; seed = nullptr; }
        auto *actions = new QHBoxLayout;
        for (int operation = 0; operation < 2; ++operation) {
            auto *button = new ElaPushButton(operation == 0 ? QStringLiteral("加入") : QStringLiteral("离开")); actions->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, page, input, seed, index, operation] {
                if (!allowOperation()) return;
                auto id = input->text().trimmed();
                auto pattern = index == 1 ? "^[0-9a-fA-F]{16}$" : "^[0-9a-fA-F]{1,16}$";
                if (!QRegularExpression(pattern).match(id).hasMatch()) {
                    QMessageBox::warning(this, QStringLiteral("输入有误"), QStringLiteral("请输入有效的十六进制 ID。")); return;
                }
                if (operation == 1 && QMessageBox::question(this, QStringLiteral("确认离开"), QStringLiteral("确定离开 %1？").arg(id)) != QMessageBox::Yes) return;
                QStringList args{index == 1 ? (operation == 0 ? "join" : "leave") : (operation == 0 ? "orbit" : "deorbit"), id};
                if (index == 3 && operation == 0) {
                    auto seedId = seed->text().trimmed();
                    if (!QRegularExpression("^[0-9a-fA-F]{10}$").match(seedId).hasMatch()) { QMessageBox::warning(this, QStringLiteral("输入有误"), QStringLiteral("Seed 需要 10 位十六进制节点 ID。")); return; }
                    args << seedId;
                }
                m_client->run(args, [this, page, index, operation, id](bool ok, QByteArray bytes) {
                    appendOutput(QString::fromLocal8Bit(bytes));
                    auto *label = page->findChild<QLabel *>("resultStatus");
                    label->setText(ok ? QStringLiteral("操作完成。") : m_client->lastError());
                    if (ok) {
                        if (index == 1) {
                            if (operation == 0) m_networkMonitor->track(id);
                            else m_networkMonitor->forget(id);
                        }
                        refreshPage(page);
                    }
                });
            });
        }
        layout->addLayout(actions);
        if (index == 1) {
            auto *settings = new ElaPushButton(QStringLiteral("网络设置 / 属性查询"));
            settings->setObjectName("openNetworkSettings"); layout->addWidget(settings);
            connect(settings, &QPushButton::clicked, this, [this, page, input] {
                if (!allowOperation()) return;
                const auto id = input->text().trimmed().toLower();
                if (!QRegularExpression("^[0-9a-f]{16}$").match(id).hasMatch()) {
                    QMessageBox::warning(this, QStringLiteral("输入有误"), QStringLiteral("请选择网络或输入 16 位十六进制网络 ID。")); return;
                }
                setProperty("settingsBusy", true); updateActions();
                NetworkSettingsDialog dialog(m_client, id, this);
                dialog.exec();
                setProperty("settingsBusy", false); updateActions(); refreshPage(page);
            });
        }
    }
    if (index == 3) {
        auto *import = new ElaPushButton(QStringLiteral("导入 .moon 文件")); layout->addWidget(import);
        connect(import, &QPushButton::clicked, this, [this, page] {
            if (!allowOperation()) return;
            auto path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 Moon 文件"), {}, "Moon (*.moon)");
            if (path.isEmpty()) return;
            setProperty("importBusy", true); updateActions();
            page->findChild<QLabel *>("resultStatus")->setText(QStringLiteral("正在确认服务数据目录…"));
            m_client->run({"-j", "info"}, [this, page, path](bool ok, QByteArray bytes) {
                QString error;
                const auto home = ok ? MoonFiles::homeDirectory(bytes, &error) : QString{};
                if (!ok) error = m_client->lastError();
                if (home.isEmpty()) {
                    setProperty("importBusy", false); updateActions();
                    page->findChild<QLabel *>("resultStatus")->setText(error);
                    appendOutput(error + '\n'); return;
                }
                m_client->setHomeDirectory(home);
                const auto destination = QDir(home).filePath("moons.d/" + QFileInfo(path).fileName());
                const bool exists = QFile::exists(destination);
                if (exists && QMessageBox::question(this, QStringLiteral("覆盖文件"), QStringLiteral("同名文件已存在，是否覆盖？")) != QMessageBox::Yes) {
                    setProperty("importBusy", false); updateActions();
                    page->findChild<QLabel *>("resultStatus")->setText(QStringLiteral("已取消覆盖，原文件保留。")); return;
                }
                error = MoonFiles::importFile(path, home, exists);
                if (!error.isEmpty()) {
                    setProperty("importBusy", false); updateActions();
                    page->findChild<QLabel *>("resultStatus")->setText(error);
                    QMessageBox::warning(this, QStringLiteral("导入失败"), error); return;
                }
                updateMoonCount();
                appendOutput(QStringLiteral("Moon 文件已保存：%1\n").arg(destination));
                if (QMessageBox::question(this, QStringLiteral("重启使 Moon 生效"),
                        QStringLiteral("文件已导入。现在重启 ZeroTier 服务以加载 Moon？这会暂时断开网络。")) != QMessageBox::Yes) {
                    setProperty("importBusy", false); updateActions();
                    page->findChild<QLabel *>("resultStatus")->setText(QStringLiteral("文件已导入，尚未重启；请稍后在服务页重启。")); return;
                }
                setProperty("importBusy", false); updateActions();
                page->findChild<QLabel *>("resultStatus")->setText(QStringLiteral("文件已导入，正在重启服务…"));
                service("restart", [this, page](bool restarted, QString reason) {
                    page->findChild<QLabel *>("resultStatus")->setText(restarted ? QStringLiteral("服务已重启，正在重新读取 Moon 列表。")
                        : QStringLiteral("文件已导入，但重启失败：%1；文件已保留，请手动重试。").arg(reason));
                    if (restarted) refreshPage(page);
                }, true);
            });
        });
    }
    if (index == 4) {
        auto *actions = new QHBoxLayout;
        for (const auto &action : {"start", "stop", "restart"}) {
            const QMap<QString, QString> names{{"start", QStringLiteral("启动服务")}, {"stop", QStringLiteral("停止服务")}, {"restart", QStringLiteral("重启服务")}};
            auto *button = new ElaPushButton(names.value(QString::fromLatin1(action))); actions->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, action] { service(QString::fromLatin1(action)); });
        }
        layout->addLayout(actions);
    }
    // Allocate result models only when a page is actually visited.
    layout->addStretch();
    for (auto *button : page->findChildren<QPushButton *>()) {
        button->setProperty("managementAction", true);
    }
    updateActions();
}
void MainWindow::refreshPage(QWidget *page)
{
    populatePage(page);
    if (m_client->busy() || (property("serviceBusy").toBool() || property("startupBusy").toBool() || property("importBusy").toBool() || property("settingsBusy").toBool() || property("maintenanceBusy").toBool() || property("installationRemoved").toBool())) return;
    if (page->property("command").toString() == "service") {
        const QMap<QString, QString> names{{"running", QStringLiteral("运行中")}, {"stopped", QStringLiteral("已停止")}, {"missing", QStringLiteral("未安装")}, {"pending", QStringLiteral("正在切换状态")}, {"unknown", QStringLiteral("无法查询")}};
        page->findChild<QLabel *>("resultStatus")->setText(QStringLiteral("ZeroTier 服务：%1").arg(names.value(ZeroTierClient::serviceStatus())));
        return;
    }
    if (!m_client->hasAdminPrivileges()) {
        page->findChild<QLabel *>("resultStatus")->setText(QStringLiteral("需要管理员权限，请点击上方提权入口。")); return;
    }
    auto *tree = page->findChild<QTreeWidget *>();
    if (!tree) {
        tree = new QTreeWidget(page); tree->setRootIsDecorated(false); tree->setUniformRowHeights(true);
        tree->setSelectionBehavior(QAbstractItemView::SelectRows);
        tree->setSelectionMode(QAbstractItemView::SingleSelection);
        page->layout()->addWidget(tree);
        if (auto *input = page->findChild<QLineEdit *>("networkIdInput")) {
            connect(tree, &QTreeWidget::itemSelectionChanged, input, [tree, input] {
                if (tree->currentItem()) input->setText(tree->currentItem()->text(0));
            });
        }
    }
    auto *label = page->findChild<QLabel *>("resultStatus"); label->setText(QStringLiteral("正在读取…"));
    m_client->run({"-j", page->property("command").toString()}, [this, tree, label](bool ok, QByteArray bytes) {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(bytes, &error);
        const auto command = tree->parentWidget()->property("command").toString();
        const bool correctShape = command == "info" ? doc.isObject() : doc.isArray();
        if (!ok || error.error != QJsonParseError::NoError || !correctShape) {
            label->setText(!ok ? m_client->lastError() : QStringLiteral("返回数据不是有效 JSON，请查看日志。"));
            tree->setProperty("lastDigest", QByteArray{});
            appendOutput(QString::fromLocal8Bit(bytes).left(4096)); return;
        }
        if (command == "info") updateMoonDirectory(bytes);
        label->setText(QStringLiteral("更新于 %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        const auto digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
        const auto nowMs = QDateTime::currentMSecsSinceEpoch();
        if (command == "listpeers" && tree->property("lastDigest").toByteArray() == digest) {
            // Relative times still change when the service returns identical JSON.
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *item = tree->topLevelItem(i);
                const auto columns = PeerPresentation::columns(item->data(0, Qt::UserRole).toJsonObject(), nowMs);
                item->setText(6, columns[6]); item->setText(7, columns[7]);
            }
        }
        if (tree->property("lastDigest").toByteArray() == digest) return;
        tree->setProperty("lastDigest", digest);
        const auto selectedId = tree->currentItem() ? tree->currentItem()->text(0) : QString{};
        if (doc.isArray() && (command == "listnetworks" || command == "listpeers")) {
            tree->clear();
            tree->setHeaderLabels(command == "listnetworks"
                ? QStringList{QStringLiteral("网络 ID"), QStringLiteral("名称"), QStringLiteral("状态"), QStringLiteral("类型"), QStringLiteral("IP 地址")}
                : QStringList{QStringLiteral("节点 ID"), QStringLiteral("角色"), QStringLiteral("延迟 ms"), QStringLiteral("路径"), QStringLiteral("版本"), QStringLiteral("链路"), QStringLiteral("距发送 ms"), QStringLiteral("距接收 ms")});
            for (const auto &value : doc.array()) {
                const auto object = value.toObject();
                if (command == "listnetworks") {
                    QStringList addresses;
                    for (const auto &ip : object.value("assignedAddresses").toArray()) addresses << ip.toString();
                    new QTreeWidgetItem(tree, {object.value("nwid").toString(), object.value("name").toString(), object.value("status").toString(), object.value("type").toString(), addresses.join(", ")});
                } else {
                    auto *item = new QTreeWidgetItem(tree, PeerPresentation::columns(object, nowMs));
                    item->setData(0, Qt::UserRole, object);
                    item->setToolTip(3, PeerPresentation::pathDetails(object));
                    item->setToolTip(5, QStringLiteral("按首选路径与 tunneled 字段分类；不代表当前可达。"));
                }
            }
            if (command == "listpeers") {
                for (int column = 0; column < tree->columnCount(); ++column) tree->resizeColumnToContents(column);
            } else tree->resizeColumnToContents(0);
        } else fillJson(tree, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()));
        const bool empty = doc.isArray() && doc.array().isEmpty();
        tree->setVisible(!empty);
        if (auto *artWidget = tree->parentWidget()->findChild<QWidget *>("pageArtwork")) {
            auto *art = static_cast<ArtPanel *>(artWidget);
            art->setCaption(empty ? QStringLiteral("等待下一次连接") : art->property("defaultCaption").toString());
            art->setFixedHeight(empty ? 200 : 140);
        }
        if (empty) label->setText(QStringLiteral("当前没有条目，更新于 %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        if (!selectedId.isEmpty()) {
            const auto items = tree->findItems(selectedId, Qt::MatchExactly, 0);
            if (!items.isEmpty()) tree->setCurrentItem(items.first());
        }
    });
}
void MainWindow::elevate()
{
#ifdef Q_OS_WIN
    const auto exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const auto dir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    SHELLEXECUTEINFOW info{}; info.cbSize = sizeof(info); info.lpVerb = L"runas";
    info.lpFile = reinterpret_cast<LPCWSTR>(exe.utf16()); info.lpDirectory = reinterpret_cast<LPCWSTR>(dir.utf16()); info.nShow = SW_SHOWNORMAL;
    if (m_client->busy() || (property("serviceBusy").toBool() || property("startupBusy").toBool() || property("importBusy").toBool() || property("settingsBusy").toBool() || property("maintenanceBusy").toBool() || property("installationRemoved").toBool())) { QMessageBox::information(this, QStringLiteral("请稍候"), QStringLiteral("请等待当前操作完成。")); return; }
    if (ShellExecuteExW(&info)) close(); else appendOutput(QStringLiteral("提权未完成或已取消。\n"));
#endif
}
void MainWindow::service(const QString &action, std::function<void(bool, QString)> callback, bool confirmed)
{
    if (!allowOperation()) return;
    if (ServiceControl::script(action).isEmpty()) return;
    if (!confirmed && action != "start" && QMessageBox::question(this, QStringLiteral("确认服务操作"),
            QStringLiteral("此操作会暂时断开 ZeroTier 网络，是否继续？")) != QMessageBox::Yes) return;
    setProperty("serviceBusy", true); updateActions();
    m_serviceControl->run(action, [this, callback](bool ok, QString error) {
        setProperty("serviceBusy", false); updateActions();
        appendOutput(ok ? QStringLiteral("服务操作完成。\n") : error + '\n');
        for (auto *page : m_pages) if (page->property("loaded").toBool() && page->property("command").toString() == "service") refreshPage(page);
        if (callback) callback(ok, error);
        if (ok) QTimer::singleShot(0, this, &MainWindow::detect);
    });
}
void MainWindow::updateMoonDirectory(const QByteArray &info)
{
    QString error;
    const auto home = MoonFiles::homeDirectory(info, &error);
    if (!home.isEmpty()) { m_client->setHomeDirectory(home); updateMoonCount(); }
}
void MainWindow::updateMoonCount()
{
    const auto home = m_client->activeDataDirectory();
    for (auto *page : m_pages) if (auto *label = page->findChild<QLabel *>("moonFilesStatus"))
        label->setText(QStringLiteral("本地 Moon 文件：%1 个\n数据目录：%2").arg(MoonFiles::count(home)).arg(home));
}

bool MainWindow::allowOperation(bool needsAdmin)
{
    if (property("installationRemoved").toBool()) {
        QMessageBox::information(this, QStringLiteral("ZeroTier 已卸载"), QStringLiteral("请退出并重新打开 GUI，进入安装流程。")); return false;
    }
    if (m_client->busy() || (property("serviceBusy").toBool() || property("startupBusy").toBool() || property("importBusy").toBool() || property("settingsBusy").toBool() || property("maintenanceBusy").toBool() || property("installationRemoved").toBool())) {
        QMessageBox::information(this, QStringLiteral("请稍候"), QStringLiteral("请等待当前操作完成后重试。")); return false;
    }
    if (needsAdmin && !m_client->hasAdminPrivileges()) {
        QMessageBox::information(this, QStringLiteral("需要权限"), QStringLiteral("请使用页面上方的管理员入口重新打开。")); return false;
    }
    return true;
}
void MainWindow::updateActions()
{
    const bool idle = !m_client->busy() && !(property("serviceBusy").toBool() || property("startupBusy").toBool() || property("importBusy").toBool() || property("settingsBusy").toBool() || property("maintenanceBusy").toBool() || property("installationRemoved").toBool());
    for (auto *button : findChildren<QPushButton *>()) {
        if (button->property("managementAction").toBool()) {
            const bool ready = button->objectName() != "exportDiagnostics"
                || !button->parentWidget()->property("diagnosticReport").toString().isEmpty();
            button->setEnabled(idle && ready);
        }
    }
}

void MainWindow::applyTheme()
{
    const bool dark = eTheme->getThemeMode() == ElaThemeType::Dark;
    QPalette palette;
    const QColor background = dark ? QColor("#202020") : QColor("#f5f5f5");
    const QColor base = dark ? QColor("#292929") : QColor("#ffffff");
    const QColor text = dark ? QColor("#f2f2f2") : QColor("#202020");
    const QColor muted = dark ? QColor("#b0b0b0") : QColor("#666666");
    palette.setColor(QPalette::Window, background);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, dark ? QColor("#333333") : QColor("#eeeeee"));
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, base);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::PlaceholderText, muted);
    palette.setColor(QPalette::Highlight, QColor("#0078d4"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, base);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Link, dark ? QColor("#7cbcff") : QColor("#0067b8"));
    palette.setColor(QPalette::LinkVisited, dark ? QColor("#c3a0ef") : QColor("#7040a0"));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, muted);
    QApplication::setPalette(palette);
    setPalette(palette);
    if (auto *links = findChild<QLabel *>("projectLinks"))
        links->setText(links->property("linkMarkup").toString().arg(palette.color(QPalette::Link).name()));
    // Ela paints its own controls; scope native-widget rules to the data views.
    const QString border = dark ? "#484848" : "#d5d5d5";
    setStyleSheet(QString(
        "QTreeWidget, QPlainTextEdit { color: %1; background-color: %2; border: 1px solid %3; border-radius: 4px; }"
        "QTreeWidget::item:selected { color: white; background-color: #0078d4; }"
        "QHeaderView::section { color: %1; background-color: %2; border: 0; border-bottom: 1px solid %3; padding: 6px; }"
        "QProgressBar { color: %1; background-color: %2; border: 1px solid %3; }"
        "QProgressBar::chunk { background-color: #0078d4; }").arg(text.name(), base.name(), border));
    update();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_client->busy() || property("serviceBusy").toBool() || property("startupBusy").toBool() || property("importBusy").toBool() || property("settingsBusy").toBool() || property("maintenanceBusy").toBool()) {
        QMessageBox::information(this, QStringLiteral("操作进行中"), QStringLiteral("请等待当前安装或管理操作完成后关闭窗口。"));
        event->ignore(); return;
    }
    ElaWindow::closeEvent(event);
}
