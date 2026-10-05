#pragma once
#include <QWidget>
class MainWindow;
class JsonPoller;
class QLabel;
class QPushButton;
class PeerCacheMaintenance;
class ServiceTab : public QWidget {
    Q_OBJECT
public:
    explicit ServiceTab(MainWindow *mainWindow, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
private:
    void updateInstallation();
    void changeService(const QString &action);
    void packageAction(bool uninstall);
    void updateButtons();
    void clearPeerCache();
    MainWindow *m_main;
    JsonPoller *m_poller;
    QLabel *m_installation;
    QLabel *m_service;
    QLabel *m_feedback;
    QPushButton *m_install;
    QPushButton *m_uninstall;
    QPushButton *m_start;
    QPushButton *m_stop;
    QPushButton *m_restart;
    QPushButton *m_cache;
    PeerCacheMaintenance *m_cacheMaintenance;
    bool m_installed = false;
    bool m_busy = false;
};
