#pragma once
#include <QObject>
#include <QStringConverter>
#include <functional>
class QProcess;
class QTimer;
class ZeroTierUninstaller : public QObject {
    Q_OBJECT
public:
    using Detector = std::function<bool()>;
    ZeroTierUninstaller(QObject *parent = nullptr, const QString &program = {}, Detector detector = {}, int timeoutMs = 600000);
    virtual void run(std::function<void(bool, QString)> callback);
    static QStringList arguments();
signals:
    void log(const QString &text);
private:
    void readOutput();
    void complete(bool ok, const QString &message);
    QProcess *m_process;
    QTimer *m_timer;
    QString m_program;
    Detector m_detector;
    QStringDecoder m_decoder{QStringDecoder::Utf8};
    int m_timeout;
    bool m_timedOut = false;
    std::function<void(bool, QString)> m_callback;
};
