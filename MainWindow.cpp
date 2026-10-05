#include "MainWindow.h"
#include "DataTable.h"
#include "AppStyle.h"
#include "AppLog.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QStyle>
#include <QStatusBar>
#include "ZeroTierClient.h"
#include "tabs/ServiceTab.h"
#include "tabs/InfoTab.h"
#include "tabs/PeersTab.h"
#include "tabs/NetworkTab.h"
#include "tabs/MoonTab.h"

#include <QCloseEvent>
#include <QFile>
#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("ZeroTier GUI"));
    resize(1120, 720);
    setMinimumSize(760, 480);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    auto *toolbar = new QHBoxLayout;
    auto *hamburger = new QToolButton;
    hamburger->setObjectName("sidebarToggle");
    hamburger->setText(QStringLiteral("☰"));
    hamburger->setAccessibleName(QStringLiteral("折叠或展开侧栏"));
    hamburger->setToolTip(QStringLiteral("折叠侧栏"));
    hamburger->setCheckable(true);
    toolbar->addWidget(hamburger);
    toolbar->addStretch();
    auto *theme = new QToolButton;
    theme->setObjectName("themeToggle");
    theme->setText(QStringLiteral("深色主题"));
    theme->setCheckable(true);
    theme->setChecked(qApp->property("darkTheme").toBool());
    theme->setAccessibleName(QStringLiteral("切换深浅主题"));
    auto updateThemeText = [theme](bool dark) {
        theme->setText(dark ? QStringLiteral("浅色主题") : QStringLiteral("深色主题"));
        theme->setToolTip(theme->text());
    };
    updateThemeText(theme->isChecked());
    connect(theme, &QToolButton::toggled, this, [updateThemeText](bool dark) {
        setAppDarkTheme(*qApp, dark);
        QSettings().setValue("appearance/dark", dark);
        updateThemeText(dark);
    });
    toolbar->addWidget(theme);
    auto *pin = new QToolButton;
    pin->setObjectName("alwaysOnTopToggle");
    pin->setText(QStringLiteral("置顶"));
    pin->setToolTip(QStringLiteral("窗口始终置顶"));
    pin->setAccessibleName(QStringLiteral("窗口始终置顶"));
    pin->setCheckable(true);
    connect(pin, &QToolButton::toggled, this, [this, pin](bool on) {
        const auto geometry = saveGeometry();
        const auto state = windowState();
        const bool visible = isVisible();
        setWindowFlag(Qt::WindowStaysOnTopHint, on);
        restoreGeometry(geometry);
        setWindowState(state);
        if (visible) show();
        pin->setText(on ? QStringLiteral("取消置顶") : QStringLiteral("置顶"));
        QSettings().setValue("window/alwaysOnTop", on);
    });
    pin->setChecked(QSettings().value("window/alwaysOnTop", false).toBool());
    toolbar->addWidget(pin);
    layout->addLayout(toolbar);
    connect(hamburger, &QToolButton::toggled, this, [this, hamburger](bool collapsed) {
        setSidebarCollapsed(collapsed);
        hamburger->setToolTip(collapsed ? QStringLiteral("展开侧栏") : QStringLiteral("折叠侧栏"));
    });
    m_horizontal = new QSplitter(Qt::Horizontal);
    m_horizontal->setObjectName("mainSplitter");
    m_horizontal->setChildrenCollapsible(false);
    m_horizontal->setHandleWidth(7);
    auto *sidebar = new QWidget;
    m_sidebar = sidebar;
    sidebar->setObjectName("sidebar");
    sidebar->setMinimumWidth(130);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(10, 14, 10, 10);
    auto *brand = new QLabel(QStringLiteral("ZeroTier"));
    brand->setObjectName("brand");
    m_brand = brand;
    sideLayout->addWidget(brand);
    m_subtitle = new QLabel(QStringLiteral("连接管理"));
    sideLayout->addWidget(m_subtitle);
    m_navigation = new QListWidget;
    m_navigation->setObjectName("navigation");
    m_navigation->addItems({QStringLiteral("服务"), QStringLiteral("节点信息"),
                            QStringLiteral("成员 / Peers"), QStringLiteral("网络 / Networks"),
                            QStringLiteral("中转站 / Moon")});
    const QStyle::StandardPixmap icons[] = {QStyle::SP_ComputerIcon, QStyle::SP_FileDialogInfoView,
        QStyle::SP_DirHomeIcon, QStyle::SP_DriveNetIcon, QStyle::SP_DialogApplyButton};
    for (int row = 0; row < m_navigation->count(); ++row) {
        auto *item = m_navigation->item(row);
        item->setData(Qt::UserRole, item->text());
        item->setToolTip(item->text());
        item->setIcon(style()->standardIcon(icons[row]));
    }
    m_navigation->setIconSize(QSize(20, 20));
    m_navigation->setMinimumWidth(0);
    m_navigation->setTextElideMode(Qt::ElideNone);
    enableDragScrolling(m_navigation);
    sideLayout->addWidget(m_navigation, 1);
    auto *hint = new QLabel(QStringLiteral("拖动分隔线调整侧栏宽度"));
    m_hint = hint;
    hint->setWordWrap(true);
    hint->setObjectName("sidebarHint");
    sideLayout->addWidget(hint);
    m_pages = new QStackedWidget;
    m_pages->setMinimumWidth(460);
    m_pages->addWidget(new ServiceTab(this));
    m_pages->addWidget(new InfoTab(this));
    m_pages->addWidget(new PeersTab(this));
    m_pages->addWidget(new NetworkTab(this));
    m_pages->addWidget(new MoonTab(this));
    connect(m_navigation, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_navigation->setCurrentRow(ZeroTier::executable().isEmpty() ? 0 : 3);
    m_horizontal->addWidget(sidebar);
    m_horizontal->addWidget(m_pages);
    m_horizontal->setStretchFactor(0, 0);
    m_horizontal->setStretchFactor(1, 1);
    m_horizontal->setSizes({190, 900});
    const auto splitterState = QSettings().value("window/sidebar").toByteArray();
    if (!splitterState.isEmpty()) m_horizontal->restoreState(splitterState);
    m_sidebarWidth = QSettings().value("window/sidebarWidth", 190).toInt();
    layout->addWidget(m_horizontal, 1);
    setCentralWidget(central);
    const auto geometry = QSettings().value("window/geometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    hamburger->setChecked(QSettings().value("window/sidebarCollapsed", false).toBool());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    if (!m_collapsed) settings.setValue("window/sidebar", m_horizontal->saveState());
    settings.setValue("window/sidebarWidth", m_collapsed ? m_sidebarWidth : m_horizontal->sizes().value(0));
    QMainWindow::closeEvent(event);
}

void MainWindow::runService(const QString &action, std::function<void(bool)> finished)
{
    QString script = "$ErrorActionPreference = 'Stop'\ntry {\n";
    if (action == "start") script += "Start-Service -Name ZeroTierOneService -ErrorAction Stop\n";
    else if (action == "stop") script += "Stop-Service -Name ZeroTierOneService -ErrorAction Stop\n";
    else if (action == "restart") script += "Restart-Service -Name ZeroTierOneService -ErrorAction Stop\n";
    else { if (finished) finished(false); return; }
    script += "exit 0\n} catch { Write-Output $_; exit 1 }";
    runElevatedScript(QStringLiteral("ZeroTier 服务：%1").arg(action), script, finished);
}

void MainWindow::runElevatedScript(const QString &label, const QString &script,
                                   std::function<void(bool)> finished)
{
#ifdef Q_OS_WIN
    const auto invocation = ZeroTier::preparePowerShell(script);
    if (!invocation.directory->isValid()) {
        appendOutput(QStringLiteral("无法创建操作日志\n"));
        if (finished) finished(false);
        return;
    }
    const QString parameters = invocation.arguments.join(' ');
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = L"powershell.exe";
    info.lpParameters = reinterpret_cast<LPCWSTR>(parameters.utf16());
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || !info.hProcess) {
        appendOutput(label + QStringLiteral("：未执行，提权被取消或启动失败\n"));
        if (finished) finished(false);
        return;
    }
    appendOutput(label + QStringLiteral("：执行中…\n"));
    auto handle = std::shared_ptr<void>(info.hProcess, [](void *value) { CloseHandle(value); });
    auto *timer = new QTimer(this);
    timer->setInterval(200);
    connect(timer, &QTimer::timeout, this, [this, timer, handle, label, invocation, finished] {
        if (WaitForSingleObject(handle.get(), 0) != WAIT_OBJECT_0) return;
        timer->stop();
        DWORD code = 1;
        GetExitCodeProcess(handle.get(), &code);
        QFile output(invocation.logPath);
        if (output.open(QIODevice::ReadOnly)) {
            // Detect UTF-8 and UTF-16 logs from supported PowerShell versions.
            QTextStream stream(&output);
            const QString text = stream.readAll().trimmed();
            if (!text.isEmpty()) appendOutput(text + '\n');
        }
        appendOutput(label + (code == 0 ? QStringLiteral("：完成\n") : QStringLiteral("：失败（退出码 %1）\n").arg(code)));
        if (finished) finished(code == 0);
        timer->deleteLater();
    });
    timer->start();
#else
    Q_UNUSED(script)
    appendOutput(label + QStringLiteral("：此操作仅支持 Windows\n"));
    if (finished) finished(false);
#endif
}

void MainWindow::appendOutput(const QString &text)
{
    AppLog::write(text);
}

void MainWindow::setSidebarCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) return;
    if (collapsed) {
        m_sidebarWidth = m_horizontal->sizes().value(0, 190);
        QSettings().setValue("window/sidebar", m_horizontal->saveState());
    }
    m_collapsed = collapsed;
    m_brand->setVisible(!collapsed);
    m_subtitle->setVisible(!collapsed);
    m_hint->setVisible(!collapsed);
    for (int row = 0; row < m_navigation->count(); ++row) {
        auto *item = m_navigation->item(row);
        item->setText(collapsed ? QString() : item->data(Qt::UserRole).toString());
    }
    m_sidebar->setMinimumWidth(collapsed ? 64 : 130);
    m_sidebar->setMaximumWidth(collapsed ? 64 : QWIDGETSIZE_MAX);
    const int width = collapsed ? 64 : qMax(130, m_sidebarWidth);
    m_horizontal->setSizes({width, qMax(460, m_horizontal->width() - width)});
    QSettings().setValue("window/sidebarCollapsed", collapsed);
}
