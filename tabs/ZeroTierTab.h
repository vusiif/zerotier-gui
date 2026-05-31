#pragma once

#include <QWidget>

class QScrollArea;
class MainWindow;
class ServiceTab;
class InfoTab;
class PeersTab;
class NetworkTab;
class MoonTab;

class ZeroTierTab : public QWidget {
    Q_OBJECT
public:
    explicit ZeroTierTab(MainWindow *mainWindow, QWidget *parent = nullptr);

    // 提供给 MainWindow 定时刷新用
    void refreshAll();

private:
    void setupUi();

    MainWindow *m_main;
    QScrollArea *m_scrollArea;

    // 子模块（复用现有类）
    ServiceTab *m_serviceTab;
    InfoTab    *m_infoTab;
    PeersTab   *m_peersTab;
    NetworkTab *m_networkTab;
    MoonTab    *m_moonTab;
};
