#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QTimer>

class StatusTab;
class ZeroTierTab;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void runService(const QString &action);
    void appendOutput(const QString &text);
    void switchToTab(int index);        // 供 StatusTab 跳转到 ZeroTier 标签页

private:
    void setupUi();
    void toggleConsole();               // 控制台显隐切换

    QTabWidget   *m_tabWidget;
    QTextEdit    *m_outputConsole;
    QPushButton  *m_consoleToggleBtn;   // 控制台切换按钮

    StatusTab    *m_statusTab;          // 首页
    ZeroTierTab  *m_zeroTierTab;        // ZT 整合页
    QTimer       *m_globalTimer;        // 全局定时刷新

    static constexpr int MAX_OUTPUT_LINES = 500;
    static constexpr int REFRESH_INTERVAL_MS = 10000;  // 10 秒
};
