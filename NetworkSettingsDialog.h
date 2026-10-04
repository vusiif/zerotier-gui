#pragma once
#include <QDialog>
#include <QJsonObject>
#include <functional>
class ZeroTierClient;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;

class NetworkSettingsDialog : public QDialog {
    Q_OBJECT
public:
    NetworkSettingsDialog(ZeroTierClient *client, const QString &networkId, QWidget *parent = nullptr);
    bool operationBusy() const { return m_busy; }
protected:
    void reject() override;
    void closeEvent(QCloseEvent *event) override;
private:
    void setBusy(bool busy);
    void fetch(std::function<void(QJsonObject, QString)> callback);
    void display(const QJsonObject &object);
    void reload();
    void save();
    void writeNext(const QStringList &keys, const QJsonObject &wanted, int index, const QStringList &applied);
    void verify(const QJsonObject &wanted, const QStringList &applied, const QString &failure = {});
    void query();
    ZeroTierClient *m_client;
    QString m_id;
    QJsonObject m_snapshot;
    QList<QCheckBox *> m_checks;
    QComboBox *m_property;
    QLabel *m_status;
    QPlainTextEdit *m_result;
    QPushButton *m_reload, *m_save, *m_query, *m_close;
    bool m_busy = false;
};
