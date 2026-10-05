#pragma once
#include "../DataTable.h"
class MainWindow;
class JsonPoller;
class QLineEdit;
class QPushButton;
class ZeroTierClient;
class NetworkMonitor;
class NetworkTab : public DataTable {
    Q_OBJECT
public:
    explicit NetworkTab(MainWindow *mainWindow, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
private:
    void joinNetwork();
    void leaveNetwork();
    void operate(const QString &action, const QString &id);
    void editSettings();
    MainWindow *m_main;
    JsonPoller *m_poller;
    QLineEdit *m_input;
    QPushButton *m_join;
    QPushButton *m_leave;
    QPushButton *m_settings;
    ZeroTierClient *m_client;
    NetworkMonitor *m_monitor;
    bool m_settingsOpen = false;
    bool m_operating = false;
};
