#pragma once
#include <QObject>
#include <functional>
class QProcess;
class QTimer;
class ServiceControl : public QObject {
    Q_OBJECT
public:
    explicit ServiceControl(QObject *parent = nullptr);
    virtual void run(const QString &action, std::function<void(bool, QString)> callback);
    static QString script(const QString &action);
private:
    QProcess *m_process;
    QTimer *m_timeout;
    QByteArray m_output;
    bool m_timedOut = false;
    std::function<void(bool, QString)> m_callback;
    void complete(bool ok, const QString &reason = {});
};
