#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QTreeWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QDialog;

struct TableRow {
    QString id;
    QStringList cells; // Name is inserted separately as the first column.
    QJsonObject details;
};

// Adds mouse panning without taking away clicks, double clicks or wheel scrolling.
void enableDragScrolling(QAbstractItemView *view);

class DataTable : public QWidget {
    Q_OBJECT
public:
    DataTable(const QString &kind, const QStringList &headers, QWidget *parent = nullptr,
              bool names = true);
    void setRows(const QList<TableRow> &rows);
    void setAvailability(bool available, const QString &message);
    QString selectedId() const;
    QTreeWidget *tree() const { return m_tree; }
    QVBoxLayout *bodyLayout() const { return m_layout; }
    void renameSelected();
    void showDetails();
private:
    QString m_kind;
    bool m_names;
    QTreeWidget *m_tree;
    QLabel *m_status;
    QPushButton *m_detailsButton;
    QPushButton *m_nameButton = nullptr;
    QVBoxLayout *m_layout;
    QDialog *m_details = nullptr;
    bool m_available = false;
    QString m_message = QStringLiteral("正在读取…");
    void updateStatus();
};
