#pragma once

#include <QWidget>

class QTreeWidget;
class QProcess;
class MainWindow;

class PeersTab : public QWidget {
    Q_OBJECT

public:
    explicit PeersTab(MainWindow *mainWindow, QWidget *parent = nullptr);
    void refresh();

private:
    MainWindow *m_main;
    QTreeWidget *m_tree;
    QProcess *m_proc;
};
