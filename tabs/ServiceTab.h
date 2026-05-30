#pragma once

#include <QWidget>

class QProcess;
class MainWindow;

class ServiceTab : public QWidget {
    Q_OBJECT

public:
    explicit ServiceTab(MainWindow *mainWindow, QWidget *parent = nullptr);

private:
    MainWindow *m_main;
    QProcess *m_proc;
};
