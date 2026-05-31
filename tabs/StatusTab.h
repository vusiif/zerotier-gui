#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QProcess;
class MainWindow;

class StatusTab : public QWidget {
    Q_OBJECT
public:
    explicit StatusTab(MainWindow *mainWindow, QWidget *parent = nullptr);

public slots:
    void refresh();

signals:
    void openZeroTierTabRequested();

private:
    void updateUI(const QJsonObject &infoJson,
                  int networkCount,
                  int peerCount);
    void showNotInstalled();

    MainWindow *m_main;
    QLabel *m_statusIndicator;      // 大号状态文字 + 图标
    QLabel *m_versionLabel;
    QLabel *m_addressLabel;
    QLabel *m_portLabel;
    QLabel *m_networkCountLabel;
    QLabel *m_peerCountLabel;
    QPushButton *m_gotoZtBtn;
    QProcess *m_proc;               // 用于 info 命令
    QProcess *m_netProc;            // 用于 listnetworks 命令
    QProcess *m_peerProc;           // 用于 listpeers 命令
};
