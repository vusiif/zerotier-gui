#pragma once
#include <QWidget>
class MainWindow;
class QLabel;
class QTextBrowser;
class HelpTab : public QWidget {
    Q_OBJECT
public:
    explicit HelpTab(MainWindow *mainWindow, QWidget *parent = nullptr);
private:
    void query(const QString &argument);
    void exportDiagnostics();
    MainWindow *m_main;
    QLabel *m_status;
    QTextBrowser *m_content;
    bool m_busy = false;
};
