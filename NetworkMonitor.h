#pragma once
#include <QObject>
#include <QHash>
#include <QElapsedTimer>
#include <QJsonArray>
#include <functional>
class ZeroTierClient;
class QTimer;

class NetworkMonitor : public QObject {
    Q_OBJECT
public:
    explicit NetworkMonitor(ZeroTierClient *client, QObject *parent = nullptr,
                            std::function<bool()> allowed = {}, int intervalMs = 5000);
    void track(const QString &id);
    void forget(const QString &id);
    bool tracks(const QString &id) const;
signals:
    void statusChanged(QString id, QString status);
    void automaticallyLeft(QString id);
    void automaticLeaveFailed(QString id, QString message);
    void diagnostic(QString message);
private:
    struct Entry {
        quint64 generation;
        QString status;
        bool leavePending = false;
        int attempts = 0;
        qint64 retryAt = 0;
    };
    void poll();
    ZeroTierClient *m_client;
    QTimer *m_timer;
    std::function<bool()> m_allowed;
    QHash<QString, Entry> m_entries;
    QElapsedTimer m_clock;
    quint64 m_generation = 0;
    bool m_inFlight = false;
    int m_intervalMs;
};
