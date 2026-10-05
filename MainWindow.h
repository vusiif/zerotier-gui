#pragma once

#include <QMainWindow>
#include <functional>

class QListWidget;
class QStackedWidget;
class QSplitter;
class QTextEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void runService(const QString &action, std::function<void(bool)> finished = {});
    void runElevatedScript(const QString &label, const QString &script,
                           std::function<void(bool)> finished = {});
    void appendOutput(const QString &text);
private:
    void setupUi();
    QListWidget *m_navigation;
    QStackedWidget *m_pages;
    QSplitter *m_horizontal;
    QTextEdit *m_outputConsole;
    void closeEvent(QCloseEvent *event) override;
};
