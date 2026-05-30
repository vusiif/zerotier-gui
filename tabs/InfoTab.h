#pragma once

#include <QWidget>

class QTreeWidget;
class QProcess;
class MainWindow;

class InfoTab : public QWidget {
    Q_OBJECT

public:
    explicit InfoTab(MainWindow *mainWindow, QWidget *parent = nullptr);

private:
    void refresh();

    MainWindow *m_main;
    QTreeWidget *m_tree;
    QProcess *m_proc;
};
