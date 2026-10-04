#pragma once
#include <QObject>
#include <QStringList>
#include <functional>
class QProcess;
class QTimer;
class ZeroTierClient : public QObject {
    Q_OBJECT
public:
    explicit ZeroTierClient(QObject *parent = nullptr, const QString &program = {}, int timeoutMs = 15000);
    static QString executable();
    static QString dataDirectory();
    static QStringList nativeArguments(const QString &program, const QStringList &arguments);
    static QString serviceStatus();
    static bool isAdministrator();
    virtual bool hasAdminPrivileges() const { return isAdministrator(); }
    QString activeDataDirectory() const { return m_homeDirectory.isEmpty() ? dataDirectory() : m_homeDirectory; }
    void setHomeDirectory(const QString &directory) { m_homeDirectory = directory; }
    bool busy() const;
    QString lastError() const { return m_lastError; }
    virtual void run(const QStringList &arguments, std::function<void(bool, QByteArray)> callback);
signals:
    void log(const QString &text);
    void busyChanged(bool busy);
private:
    QProcess *m_process;
    QTimer *m_timeout;
    QByteArray m_buffer;
    QByteArray m_error;
    QString m_program;
    QString m_homeDirectory;
    int m_timeoutMs;
    bool m_overflow = false;
    QString m_lastError;
    std::function<void(bool, QByteArray)> m_callback;
    void complete(bool success);
};
