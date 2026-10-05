#include "ElaIconButton.h"
#include "ElaAppBar.h"
#include "ElaToolButton.h"
#include <QResizeEvent>
#include "UiMotion.h"
#include <QVariantAnimation>
#include "MainWindow.h"
#include "DataTable.h"
#include "AppStyle.h"
#include "AppLog.h"
#include "ServiceControl.h"
#include "HelpTab.h"
#include <QProcess>
#include <QApplication>
#include <QHBoxLayout>
#include <QStyle>
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

MainWindow::MainWindow(QWidget *parent, ServiceControl *serviceControl) : QMainWindow(parent),
    m_serviceControl(serviceControl ? serviceControl : new ServiceControl(this))
{
    m_serviceControl->setParent(this);
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
    m_appBar = new ElaAppBar(this);
    m_appBar->setWindowButtonFlags(ElaAppBarType::NavigationButtonHint |
        ElaAppBarType::StayTopButtonHint | ElaAppBarType::ThemeChangeButtonHint |
        ElaAppBarType::MinimizeButtonHint | ElaAppBarType::MaximizeButtonHint | ElaAppBarType::CloseButtonHint);
    // Let closeEvent enforce the operation lock; Ela's default also closes the
    // native window handle, even when a QWidget close event is rejected.
    m_appBar->setIsDefaultClosed(false);
    connect(m_appBar, &ElaAppBar::closeButtonClicked, this, [this] { close(); });
    for (auto *button : m_appBar->findChildren<ElaToolButton *>()) button->setBorderRadius(0);
    auto *closeButton = m_appBar->findChild<ElaIconButton *>("closeWindowButton");
    closeButton->setBorderRadius(0);
    closeButton->setToolTip(QStringLiteral("关闭"));
    closeButton->setAccessibleName(QStringLiteral("关闭窗口"));
    auto *minimize = m_appBar->findChild<ElaToolButton *>("minimizeButton");
    auto *maximize = m_appBar->findChild<ElaToolButton *>("maximizeButton");
    minimize->setToolTip(QStringLiteral("最小化"));
    minimize->setAccessibleName(QStringLiteral("最小化窗口"));
    maximize->setToolTip(QStringLiteral("最大化或还原"));
    maximize->setAccessibleName(QStringLiteral("最大化或还原窗口"));
    auto *hamburger = m_appBar->findChild<ElaToolButton *>("sidebarToggle");
    auto *theme = m_appBar->findChild<ElaToolButton *>("themeToggle");
    auto *pin = m_appBar->findChild<ElaToolButton *>("alwaysOnTopToggle");
    hamburger->setCheckable(true);
    hamburger->setAccessibleName(QStringLiteral("折叠或展开侧栏"));
    hamburger->setToolTip(QStringLiteral("折叠侧栏"));
    theme->setCheckable(true);
    theme->setChecked(qApp->property("darkTheme").toBool());
    theme->setAccessibleName(QStringLiteral("切换深浅主题"));
    auto updateThemeText = [theme](bool dark) {
        theme->setToolTip(dark ? QStringLiteral("浅色主题") : QStringLiteral("深色主题"));
    };
    updateThemeText(theme->isChecked());
    connect(theme, &QToolButton::toggled, this, [this, updateThemeText](bool dark) {
        UiMotion::transition(this, [updateThemeText, dark] {
            setAppDarkTheme(*qApp, dark);
            updateThemeText(dark);
        }, true);
        QSettings().setValue("appearance/dark", dark);
    });
    pin->setCheckable(true);
    pin->setAccessibleName(QStringLiteral("窗口始终置顶"));
    pin->setToolTip(QStringLiteral("窗口始终置顶"));
    connect(m_appBar, &ElaAppBar::pIsStayTopChanged, this, [this, pin] {
        pin->setChecked(m_appBar->getIsStayTop());
        pin->setToolTip(m_appBar->getIsStayTop() ? QStringLiteral("取消置顶") : QStringLiteral("窗口始终置顶"));
        QSettings().setValue("window/alwaysOnTop", m_appBar->getIsStayTop());
    });
    m_appBar->setIsStayTop(QSettings().value("window/alwaysOnTop", false).toBool());
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
                            QStringLiteral("中转站 / Moon"), QStringLiteral("帮助")});
    const QString icons[] = {"settings", "info", "peers", "network", "moon", "help"};
    for (int row = 0; row < m_navigation->count(); ++row) {
        auto *item = m_navigation->item(row);
        item->setData(Qt::UserRole, item->text());
        item->setToolTip(item->text());
        item->setIcon(UiMotion::icon(icons[row]));
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
    m_pages->addWidget(new HelpTab(this));
    connect(m_navigation, &QListWidget::currentRowChanged, m_pages, [this](int row) {
        UiMotion::transition(m_pages, [this, row] { m_pages->setCurrentIndex(row); });
    });
    m_navigation->setCurrentRow(0);
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
    m_notice = new QLabel(central);
    m_notice->setObjectName("operationNotice");
    m_notice->setWordWrap(true);
    m_notice->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_notice->hide();
    m_noticeTimer = new QTimer(this);
    m_noticeTimer->setSingleShot(true);
    connect(m_noticeTimer, &QTimer::timeout, m_notice, &QWidget::hide);
    const auto geometry = QSettings().value("window/geometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    hamburger->setChecked(QSettings().value("window/sidebarCollapsed", false).toBool());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_operationBusy) {
        showNotice(QStringLiteral("请等待当前操作完成后关闭。"), 5000);
        event->ignore();
        return;
    }
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    if (!m_collapsed) settings.setValue("window/sidebar", m_horizontal->saveState());
    settings.setValue("window/sidebarWidth", m_collapsed ? m_sidebarWidth : m_horizontal->sizes().value(0));
    QMainWindow::closeEvent(event);
}

void MainWindow::runService(const QString &action, std::function<void(bool)> finished)
{
    if (!beginOperation()) { if (finished) finished(false); return; }
    m_serviceControl->run(action, [this, action, finished](bool ok, const QString &error) {
        endOperation();
        const auto message = ok ? QStringLiteral("服务 %1 操作完成。").arg(action) : error;
        appendOutput(message);
        showNotice(message, ok ? 5000 : 15000);
        if (finished) finished(ok);
    });
}

bool MainWindow::beginOperation()
{
    if (m_operationBusy) {
        showNotice(QStringLiteral("其他操作正在执行，请稍后重试。"), 5000);
        return false;
    }
    m_operationBusy = true;
    return true;
}

void MainWindow::endOperation() { m_operationBusy = false; }

void MainWindow::runElevatedScript(const QString &label, const QString &script,
                                   std::function<void(bool)> finished)
{
    if (!beginOperation()) { if (finished) finished(false); return; }
    const auto invocation = ZeroTier::preparePowerShell(script);
    if (!invocation.directory->isValid()) {
        endOperation();
        appendOutput(QStringLiteral("无法创建操作临时目录。"));
        if (finished) finished(false);
        return;
    }
    auto *process = new QProcess(this);
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    timeout->setInterval(600000);
    auto completed = std::make_shared<bool>(false);
    auto complete = [this, process, timeout, completed, label, invocation, finished](bool ok) {
        if (*completed) return;
        *completed = true;
        timeout->stop();
        endOperation();
        QFile file(invocation.logPath);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream stream(&file);
            appendOutput(stream.read(16384));
        }
        const auto message = label + (ok ? QStringLiteral("：完成") : QStringLiteral("：未完成，请检查权限、网络或服务状态。"));
        appendOutput(message);
        showNotice(message, 15000);
        if (finished) finished(ok);
        process->deleteLater();
    };
    connect(timeout, &QTimer::timeout, process, [process, complete] { process->kill(); complete(false); });
    connect(process, &QProcess::errorOccurred, process, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete(false);
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
        [complete](int code, QProcess::ExitStatus status) { complete(code == 0 && status == QProcess::NormalExit); });
    // The application's manifest already requests administrator privileges.
    process->start(qEnvironmentVariable("SystemRoot") + "/System32/WindowsPowerShell/v1.0/powershell.exe", invocation.arguments);
    timeout->start();
}

void MainWindow::appendOutput(const QString &text)
{
    AppLog::write(text);
}

void MainWindow::setSidebarCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) return;
    if (collapsed) {
        if (!m_sidebarAnimation || m_sidebarAnimation->state() != QAbstractAnimation::Running)
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
    if (!m_sidebarAnimation) {
        m_sidebarAnimation = new QVariantAnimation(this);
        m_sidebarAnimation->setDuration(220);
        m_sidebarAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_sidebarAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            const int width = value.toInt();
            m_horizontal->setSizes({width, qMax(460, m_horizontal->width() - width)});
        });
        connect(m_sidebarAnimation, &QVariantAnimation::finished, this, [this] {
            m_sidebar->setMinimumWidth(m_collapsed ? 64 : 130);
            m_sidebar->setMaximumWidth(m_collapsed ? 64 : QWIDGETSIZE_MAX);
        });
    }
    m_sidebarAnimation->stop();
    const int from = m_horizontal->sizes().value(0);
    m_sidebar->setMinimumWidth(64);
    m_sidebar->setMaximumWidth(QWIDGETSIZE_MAX);
    const int width = collapsed ? 64 : qMax(130, m_sidebarWidth);
    if (isVisible()) {
        m_sidebarAnimation->setStartValue(from);
        m_sidebarAnimation->setEndValue(width);
        m_sidebarAnimation->start();
    } else {
        m_horizontal->setSizes({width, qMax(460, m_horizontal->width() - width)});
        m_sidebar->setMinimumWidth(collapsed ? 64 : 130);
        m_sidebar->setMaximumWidth(collapsed ? 64 : QWIDGETSIZE_MAX);
    }
    QSettings().setValue("window/sidebarCollapsed", collapsed);
}

void MainWindow::showNotice(const QString &message, int duration)
{
    if (!m_notice || message.isEmpty()) return;
    m_notice->setText(message);
    m_notice->setFixedWidth(qMin(600, qMax(200, centralWidget()->width() - 40)));
    m_notice->adjustSize();
    m_notice->move((centralWidget()->width() - m_notice->width()) / 2, 8);
    m_notice->show();
    m_notice->raise();
    m_noticeTimer->start(duration > 0 ? duration : 15000);
}
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_notice && m_notice->isVisible()) {
        m_notice->setFixedWidth(qMin(600, qMax(200, centralWidget()->width() - 40)));
        m_notice->adjustSize();
        m_notice->move((centralWidget()->width() - m_notice->width()) / 2, 8);
    }
}
#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (m_appBar) {
        const int handled = m_appBar->takeOverNativeEvent(eventType, message, result);
        if (handled != -1) return handled != 0;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif
