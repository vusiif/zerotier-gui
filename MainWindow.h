#pragma once

#include <QMainWindow>
#include <functional>

class QListWidget;
class QStackedWidget;
class QSplitter;
class QWidget;
class QLabel;
class ServiceControl;
class ElaAppBar;
class QTimer;
class QVariantAnimation;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, ServiceControl *serviceControl = nullptr);
    void runService(const QString &action, std::function<void(bool)> finished = {});
    void runElevatedScript(const QString &label, const QString &script,
                           std::function<void(bool)> finished = {});
    void showNotice(const QString &message, int duration = 5000);
    void appendOutput(const QString &text);
    bool beginOperation();
    void endOperation();
    bool operationBusy() const { return m_operationBusy; }
    ServiceControl *serviceControl() const { return m_serviceControl; }
private:
    void setupUi();
    ElaAppBar *m_appBar = nullptr;
    QLabel *m_notice = nullptr;
    QTimer *m_noticeTimer = nullptr;
protected:
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
    void resizeEvent(QResizeEvent *event) override;
private:
    QListWidget *m_navigation;
    QStackedWidget *m_pages;
    QSplitter *m_horizontal;
    QWidget *m_sidebar;
    QLabel *m_brand, *m_subtitle, *m_hint;
    bool m_collapsed = false;
    QVariantAnimation *m_sidebarAnimation = nullptr;
    int m_sidebarWidth = 190;
    bool m_operationBusy = false;
    ServiceControl *m_serviceControl;
    void setSidebarCollapsed(bool collapsed);
    void closeEvent(QCloseEvent *event) override;
};
