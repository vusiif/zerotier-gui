#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <functional>
class QTimer;

class ServiceStartup : public QObject {
    Q_OBJECT
public:
    using Probe = std::function<QString()>;
    using Starter = std::function<QString()>;
    explicit ServiceStartup(QObject *parent = nullptr, Probe probe = {}, Starter starter = {},
                            int timeoutMs = 30000, int intervalMs = 200);
    void ensureRunning(std::function<void(bool, QString)> callback);
    bool busy() const { return bool(m_callback); }
signals:
    void busyChanged(bool busy);
private:
    void poll();
    void complete(bool ok, const QString &error = {});
    Probe m_probe;
    Starter m_starter;
    QTimer *m_timer;
    QElapsedTimer m_elapsed;
    int m_timeoutMs;
    bool m_requested = false;
    bool m_starting = false;
    bool m_nativeStarter;
    quint64 m_attempt = 0;
    std::function<void(bool, QString)> m_callback;
};
