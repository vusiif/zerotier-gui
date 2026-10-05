#pragma once
#include <QDialog>
#include <functional>

// A local check: no subprocess, network request or installation side effect.
bool zeroTierInstalled();

class InstallWindow : public QDialog {
public:
    explicit InstallWindow(std::function<bool()> detector = zeroTierInstalled, QWidget *parent = nullptr);
    void reject() override;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void detect();
    void install();
    void setBusy(bool busy);
    std::function<bool()> m_detector;
    class QLabel *m_status;
    class QPushButton *m_install;
    class QPushButton *m_detect;
    class QProgressBar *m_progress;
    class QProcess *m_process;
    class QTimer *m_timeout;
};
