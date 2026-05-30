#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QTextEdit>
#include <QVector>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void runService(const QString &action);
    void appendOutput(const QString &text);

private:
    void setupUi();
    void onTabChanged(int index);
    void createTabContent(int index);

    QTabWidget *m_tabWidget;
    QTextEdit *m_outputConsole;
    QVector<bool> m_tabCreated;
    static constexpr int MAX_OUTPUT_LINES = 300;
};
