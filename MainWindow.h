#pragma once
#include "ElaWindow.h"
#include <QStringConverter>
#include <functional>
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QProcess;
class ZeroTierClient;
class QTreeWidget;
class QVBoxLayout;
class ServiceStartup;
class NetworkMonitor;
class ServiceControl;
class PeerCacheMaintenance;
class ZeroTierUninstaller;
class MainWindow : public ElaWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, ZeroTierClient *client = nullptr, ServiceStartup *startup = nullptr, ServiceControl *control = nullptr, PeerCacheMaintenance *cache = nullptr, ZeroTierUninstaller *uninstaller = nullptr);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void setupManagement();
    void setupHelp();
    void populatePage(QWidget *page);
    bool allowOperation(bool needsAdmin = true);
    void updateActions();
    void applyTheme();
    QWidget *createPage(const QString &title, QVBoxLayout **layout);
    void refreshPage(QWidget *page);
    void elevate();
    void service(const QString &action, std::function<void(bool, QString)> callback = {}, bool confirmed = false);
    void updateMoonDirectory(const QByteArray &info);
    void updateMoonCount();
    ZeroTierClient *m_client;
    QList<QWidget *> m_pages;
    void detect();
    void initializeService();
    void appendOutput(const QString &text);
    QLabel *m_status;
    QPlainTextEdit *m_output;
    ServiceStartup *m_startup;
    NetworkMonitor *m_networkMonitor;
    ServiceControl *m_serviceControl;
    PeerCacheMaintenance *m_cache;
    ZeroTierUninstaller *m_uninstaller;
};
