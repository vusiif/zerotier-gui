#pragma once

#include <QObject>
#include <QJsonDocument>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <memory>

class QWidget;
class QTemporaryDir;

namespace ZeroTier {
QString executable();
QString dataDirectory();
QString homeDirectory();
QString translate(const QString &value);
QString jsonText(const QJsonValue &value);
QString quotePowerShell(const QString &value);
QString packageScript(bool uninstall, const QString &winget);
struct PowerShellInvocation {
    std::shared_ptr<QTemporaryDir> directory;
    QString logPath;
    QStringList arguments;
};
PowerShellInvocation preparePowerShell(const QString &script);
void command(QObject *owner, const QStringList &arguments,
             std::function<void(bool, const QString &)> finished);
}

// Each poll owns one process. Slow requests never overlap or block the UI.
class JsonPoller : public QObject {
    Q_OBJECT
public:
    JsonPoller(QWidget *owner, const QString &command, bool object = false);
    ~JsonPoller() override;
    void refresh();
signals:
    void updated(const QJsonDocument &document);
    void availabilityChanged(bool available, const QString &message);
private:
    QWidget *m_owner;
    QString m_command;
    bool m_object;
    QProcess m_process;
    QTimer m_interval;
    QTimer m_timeout;
    void failed(const QString &message);
};
