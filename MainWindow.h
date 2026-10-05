#pragma once

#include <QMainWindow>
#include <functional>

class QListWidget;
class QStackedWidget;
class QSplitter;
class QWidget;
class QLabel;
class ServiceControl;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, ServiceControl *serviceControl = nullptr);
    void runService(const QString &action, std::function<void(bool)> finished = {});
    void runElevatedScript(const QString &label, const QString &script,
                           std::function<void(bool)> finished = {});
    void appendOutput(const QString &text);
    bool beginOperation();
    void endOperation();
    bool operationBusy() const { return m_operationBusy; }
private:
    void setupUi();
    QListWidget *m_navigation;
    QStackedWidget *m_pages;
    QSplitter *m_horizontal;
    QWidget *m_sidebar;
    QLabel *m_brand, *m_subtitle, *m_hint;
    bool m_collapsed = false;
    int m_sidebarWidth = 190;
    bool m_operationBusy = false;
    ServiceControl *m_serviceControl;
    void setSidebarCollapsed(bool collapsed);
    void closeEvent(QCloseEvent *event) override;
};
