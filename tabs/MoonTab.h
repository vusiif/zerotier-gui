#pragma once

#include <QWidget>

class QListWidget;
class QProcess;
class MainWindow;

class MoonTab : public QWidget {
    Q_OBJECT

public:
    explicit MoonTab(MainWindow *mainWindow, QWidget *parent = nullptr);

private:
    void addMoon();
    void refreshMoonList();

    MainWindow *m_main;
    QListWidget *m_moonList;
    QProcess *m_proc;
};
