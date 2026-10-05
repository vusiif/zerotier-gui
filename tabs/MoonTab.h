#pragma once
#include "../DataTable.h"
class MainWindow;
class JsonPoller;
class QPushButton;
class QLineEdit;
class MoonTab : public DataTable {
    Q_OBJECT
public:
    explicit MoonTab(MainWindow *mainWindow, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
private:
    void addMoon();
    void subscribe(bool remove);
    MainWindow *m_main;
    JsonPoller *m_poller;
    QPushButton *m_add;
    QLineEdit *m_world, *m_seed;
    bool m_busy = false;
};
