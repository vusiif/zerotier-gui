#pragma once
#include "../DataTable.h"
class MainWindow;
class JsonPoller;
class QPushButton;
class MoonTab : public DataTable {
    Q_OBJECT
public:
    explicit MoonTab(MainWindow *mainWindow, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
private:
    void addMoon();
    MainWindow *m_main;
    JsonPoller *m_poller;
    QPushButton *m_add;
};
