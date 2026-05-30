#pragma once

#include <QWidget>

class QTreeWidget;
class QLineEdit;
class QProcess;
class MainWindow;

class NetworkTab : public QWidget {
    Q_OBJECT

public:
    explicit NetworkTab(MainWindow *mainWindow, QWidget *parent = nullptr);

private:
    void refresh();
    void joinNetwork();
    void leaveNetwork();

    MainWindow *m_main;
    QTreeWidget *m_tree;
    QLineEdit *m_input;
    QProcess *m_proc;
};
