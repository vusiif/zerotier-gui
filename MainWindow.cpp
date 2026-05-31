#include "MainWindow.h"
#include "tabs/StatusTab.h"
#include "tabs/ZeroTierTab.h"

#include <QVBoxLayout>
#include <QFont>
#include <QTextCursor>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    // 全局定时器：每 10 秒刷新 StatusTab 和 ZeroTierTab
    m_globalTimer = new QTimer(this);
    m_globalTimer->setInterval(REFRESH_INTERVAL_MS);
    connect(m_globalTimer, &QTimer::timeout, this, [this]() {
        m_statusTab->refresh();
        m_zeroTierTab->refreshAll();
    });
    m_globalTimer->start();
}

void MainWindow::setupUi()
{
    setWindowTitle("ZeroTier GUI");
    resize(900, 650);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // 1. Tab 区域（占据主要空间）
    m_tabWidget = new QTabWidget;

    // 直接创建两个标签页
    m_statusTab = new StatusTab(this);
    m_zeroTierTab = new ZeroTierTab(this);

    m_tabWidget->addTab(m_statusTab, "首页");
    m_tabWidget->addTab(m_zeroTierTab, "ZeroTier");

    // 连接 StatusTab 的"打开 ZeroTier 管理"按钮信号
    connect(m_statusTab, &StatusTab::openZeroTierTabRequested, this, [this]() {
        switchToTab(1);
    });

    layout->addWidget(m_tabWidget, 1);  // stretch factor = 1

    // 2. 控制台切换按钮
    m_consoleToggleBtn = new QPushButton("展开控制台 ▼");
    m_consoleToggleBtn->setFlat(true);
    m_consoleToggleBtn->setMaximumHeight(28);
    m_consoleToggleBtn->setCursor(Qt::PointingHandCursor);
    connect(m_consoleToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleConsole);
    layout->addWidget(m_consoleToggleBtn);

    // 3. 输出控制台（默认隐藏）
    m_outputConsole = new QTextEdit;
    m_outputConsole->setReadOnly(true);
    m_outputConsole->setPlaceholderText("Command output...");
    QFont monoFont("Consolas", 9);
    monoFont.setStyleHint(QFont::Monospace);
    m_outputConsole->setFont(monoFont);
    m_outputConsole->setMaximumHeight(200);
    m_outputConsole->document()->setMaximumBlockCount(MAX_OUTPUT_LINES);
    m_outputConsole->setVisible(false);     // ← 默认隐藏
    layout->addWidget(m_outputConsole);

    setCentralWidget(central);
}

void MainWindow::runService(const QString &action)
{
#ifdef Q_OS_WIN
    SHELLEXECUTEINFOA sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "runas";
    sei.lpFile = "net";
    QByteArray params = (action + " ZeroTierOneService").toLocal8Bit();
    sei.lpParameters = params.data();
    sei.nShow = SW_HIDE;

    if (ShellExecuteExA(&sei) && sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);
        appendOutput(QString("[net %1 ZeroTierOneService] exit: %2\n").arg(action).arg(exitCode));
    } else {
        appendOutput("Failed to execute: " + action + "\n");
    }
#else
    Q_UNUSED(action)
#endif
}

void MainWindow::appendOutput(const QString &text)
{
    m_outputConsole->moveCursor(QTextCursor::End);
    m_outputConsole->insertPlainText(text);

    // 如果控制台隐藏且有新输出，按钮上给提示
    if (!m_outputConsole->isVisible()) {
        m_consoleToggleBtn->setStyleSheet("color: #e67e22; font-weight: bold;");
        m_consoleToggleBtn->setText("展开控制台 ▼ (有新输出)");
    }
}

void MainWindow::toggleConsole()
{
    bool visible = !m_outputConsole->isVisible();
    m_outputConsole->setVisible(visible);
    m_consoleToggleBtn->setText(visible ? "收起控制台 ▲" : "展开控制台 ▼");

    if (visible) {
        // 展开时滚动到底部
        m_outputConsole->moveCursor(QTextCursor::End);
    }

    // 展开后恢复按钮样式
    m_consoleToggleBtn->setStyleSheet("");
}

void MainWindow::switchToTab(int index)
{
    if (index >= 0 && index < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(index);
    }
}
