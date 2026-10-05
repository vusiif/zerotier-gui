#include "MainWindow.h"
#include "DataTable.h"
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
    m_horizontal = new QSplitter(Qt::Horizontal);
    m_horizontal->setObjectName("mainSplitter");
    m_horizontal->setChildrenCollapsible(false);
    m_horizontal->setHandleWidth(7);
    auto *sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setMinimumWidth(130);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(10, 14, 10, 10);
    auto *brand = new QLabel(QStringLiteral("ZeroTier"));
    brand->setObjectName("brand");
    sideLayout->addWidget(brand);
    sideLayout->addWidget(new QLabel(QStringLiteral("连接管理")));
    m_navigation = new QListWidget;
    m_navigation->setObjectName("navigation");
    m_navigation->addItems({QStringLiteral("服务"), QStringLiteral("节点信息"),
                            QStringLiteral("成员 / Peers"), QStringLiteral("网络 / Networks"),
                            QStringLiteral("中转站 / Moon")});
    m_navigation->setMinimumWidth(0);
    m_navigation->setTextElideMode(Qt::ElideNone);
    enableDragScrolling(m_navigation);
    sideLayout->addWidget(m_navigation, 1);
    auto *hint = new QLabel(QStringLiteral("拖动分隔线调整侧栏宽度"));
    hint->setWordWrap(true);
    hint->setObjectName("sidebarHint");
    sideLayout->addWidget(hint);
    m_pages = new QStackedWidget;
    m_pages->setMinimumWidth(460);
    m_outputConsole = new QTextEdit;
    m_outputConsole->setObjectName("outputConsole");
    m_outputConsole->setReadOnly(true);
    m_outputConsole->setFont(QFont("Consolas", 9));
    m_outputConsole->setPlaceholderText(QStringLiteral("操作结果会显示在这里"));
    m_outputConsole->document()->setMaximumBlockCount(300);
    m_outputConsole->setMinimumHeight(50);
    m_outputConsole->setMaximumHeight(150);
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
    layout->addWidget(m_horizontal, 1);
    auto *consoleToggle = new QToolButton;
    consoleToggle->setText(QStringLiteral("操作日志"));
    consoleToggle->setCheckable(true);
    consoleToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    consoleToggle->setArrowType(Qt::RightArrow);
    m_outputConsole->hide();
    connect(consoleToggle, &QToolButton::toggled, this, [this, consoleToggle](bool visible) {
        m_outputConsole->setVisible(visible);
        consoleToggle->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
    });
    layout->addWidget(consoleToggle, 0, Qt::AlignLeft);
    layout->addWidget(m_outputConsole);
    setCentralWidget(central);
    const auto geometry = QSettings().value("window/geometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/sidebar", m_horizontal->saveState());
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
    const bool atBottom = m_outputConsole->verticalScrollBar()->value() == m_outputConsole->verticalScrollBar()->maximum();
    QTextCursor cursor(m_outputConsole->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    if (atBottom) m_outputConsole->verticalScrollBar()->setValue(m_outputConsole->verticalScrollBar()->maximum());
}
