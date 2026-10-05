#pragma once
#include <QObject>
#include <functional>
class ServiceControl;
class PeerCacheMaintenance : public QObject {
    Q_OBJECT
public:
    using Probe = std::function<QString()>;
    PeerCacheMaintenance(ServiceControl *control, QObject *parent = nullptr, Probe probe = {});
    void run(const QString &home, std::function<void(bool, QString)> callback);
    static QString validate(const QString &home);
    static QString moveAside(const QString &home, QString *backup);
private:
    void finish(bool ok, const QString &message);
    ServiceControl *m_control;
    Probe m_probe;
    std::function<void(bool, QString)> m_callback;
};
