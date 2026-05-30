#include "MainWindow.h"
#include "tabs/ServiceTab.h"
#include "tabs/InfoTab.h"
#include "tabs/PeersTab.h"
#include "tabs/NetworkTab.h"
#include "tabs/MoonTab.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QFont>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle("ZeroTier GUI");
    resize(900, 650);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_tabWidget = new QTabWidget;
    m_tabCreated.fill(false, 5);
    m_tabWidget->addTab(new QWidget, "Service");
    m_tabWidget->addTab(new QWidget, "Node Info");
    m_tabWidget->addTab(new QWidget, "Peers");
    m_tabWidget->addTab(new QWidget, "Networks");
    m_tabWidget->addTab(new QWidget, "Moon");
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    m_outputConsole = new QTextEdit;
    m_outputConsole->setReadOnly(true);
    m_outputConsole->setPlaceholderText("Command output...");
    QFont monoFont("Consolas", 9);
    monoFont.setStyleHint(QFont::Monospace);
    m_outputConsole->setFont(monoFont);
    m_outputConsole->setMaximumHeight(180);
    m_outputConsole->document()->setMaximumBlockCount(MAX_OUTPUT_LINES);

    splitter->addWidget(m_tabWidget);
    splitter->addWidget(m_outputConsole);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    layout->addWidget(splitter);
    setCentralWidget(central);
}

void MainWindow::onTabChanged(int index)
{
    if (index >= 0 && index < m_tabCreated.size() && !m_tabCreated[index]) {
        createTabContent(index);
        m_tabCreated[index] = true;
    }
}

void MainWindow::createTabContent(int index)
{
    QWidget *placeholder = m_tabWidget->widget(index);
    QWidget *real = nullptr;

    switch (index) {
        case 0: real = new ServiceTab(this, placeholder); break;
        case 1: real = new InfoTab(this, placeholder); break;
        case 2: real = new PeersTab(this, placeholder); break;
        case 3: real = new NetworkTab(this, placeholder); break;
        case 4: real = new MoonTab(this, placeholder); break;
        default: return;
    }

    auto *layout = new QVBoxLayout(placeholder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(real);
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
}
