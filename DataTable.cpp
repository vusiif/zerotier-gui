#include "DataTable.h"
#include "ZeroTierClient.h"

#include <QApplication>
#include <QDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
class DragScrollFilter : public QObject {
public:
    explicit DragScrollFilter(QAbstractItemView *view) : QObject(view), m_view(view) {}
protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_pressed = true;
                m_dragging = false;
                m_origin = mouse->pos();
                m_horizontal = m_view->horizontalScrollBar()->value();
                m_vertical = m_view->verticalScrollBar()->value();
            }
        } else if (event->type() == QEvent::MouseMove && m_pressed) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            const QPoint delta = mouse->pos() - m_origin;
            if (!m_dragging && delta.manhattanLength() >= QApplication::startDragDistance()) {
                m_dragging = true;
                m_view->viewport()->setCursor(Qt::ClosedHandCursor);
            }
            if (m_dragging) {
                m_view->horizontalScrollBar()->setValue(m_horizontal - delta.x());
                m_view->verticalScrollBar()->setValue(m_vertical - delta.y());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const bool dragged = m_dragging;
            m_pressed = m_dragging = false;
            m_view->viewport()->unsetCursor();
            if (dragged) return true;
        } else if (event->type() == QEvent::UngrabMouse) {
            m_pressed = m_dragging = false;
            m_view->viewport()->unsetCursor();
        }
        return false;
    }
private:
    QAbstractItemView *m_view;
    bool m_pressed = false;
    bool m_dragging = false;
    QPoint m_origin;
    int m_horizontal = 0;
    int m_vertical = 0;
};

void appendJson(QTreeWidgetItem *parent, const QString &key, const QJsonValue &value)
{
    auto *item = new QTreeWidgetItem(parent);
    item->setText(0, ZeroTier::translate(key));
    item->setToolTip(0, key);
    if (value.isObject()) {
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) appendJson(item, it.key(), it.value());
    } else if (value.isArray()) {
        const auto array = value.toArray();
        item->setText(1, QStringLiteral("%1 项").arg(array.size()));
        for (int i = 0; i < array.size(); ++i) appendJson(item, QString::number(i + 1), array[i]);
    } else {
        item->setText(1, ZeroTier::jsonText(value));
        item->setToolTip(1, value.toVariant().toString());
    }
}
}

void enableDragScrolling(QAbstractItemView *view)
{
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->viewport()->installEventFilter(new DragScrollFilter(view));
    view->setToolTip(QStringLiteral("按住鼠标左键拖动可查看更多内容；表头可拖动调整列宽与顺序"));
}

DataTable::DataTable(const QString &kind, const QStringList &headers, QWidget *parent, bool names)
    : QWidget(parent), m_kind(kind), m_names(names)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(8);
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(kind + "Table");
    m_tree->setHeaderLabels(headers);
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->header()->setSectionsMovable(true);
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setMinimumSectionSize(80);
    for (int i = 0; i < headers.size(); ++i) m_tree->setColumnWidth(i, i < 2 ? 180 : 135);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(names ? 1 : 0, Qt::AscendingOrder);
    enableDragScrolling(m_tree);
    m_layout->addWidget(m_tree, 1);

    auto *footer = new QHBoxLayout;
    m_status = new QLabel;
    m_status->setObjectName("tableStatus");
    footer->addWidget(m_status, 1);
    if (names) {
        m_nameButton = new QPushButton(QStringLiteral("编辑名称"));
        m_nameButton->setObjectName("renameButton");
        m_nameButton->setEnabled(false);
        footer->addWidget(m_nameButton);
        connect(m_nameButton, &QPushButton::clicked, this, &DataTable::renameSelected);
        connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int column) {
            if (column == 0) renameSelected();
            else showDetails();
        });
    }
    m_detailsButton = new QPushButton(QStringLiteral("详细信息"));
    m_detailsButton->setObjectName("detailsButton");
    m_detailsButton->setEnabled(false);
    footer->addWidget(m_detailsButton);
    m_layout->addLayout(footer);
    connect(m_detailsButton, &QPushButton::clicked, this, &DataTable::showDetails);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this] {
        const bool single = m_tree->selectedItems().size() == 1;
        m_detailsButton->setEnabled(single);
        if (m_nameButton) m_nameButton->setEnabled(single);
    });
    const auto state = QSettings().value("tables/" + kind + "/header").toByteArray();
    if (!state.isEmpty()) m_tree->header()->restoreState(state);
    connect(m_tree->header(), &QHeaderView::sectionResized, this, [this] {
        QSettings().setValue("tables/" + m_kind + "/header", m_tree->header()->saveState());
    });
    connect(m_tree->header(), &QHeaderView::sectionMoved, this, [this] {
        QSettings().setValue("tables/" + m_kind + "/header", m_tree->header()->saveState());
    });
    updateStatus();
}

void DataTable::setRows(const QList<TableRow> &rows)
{
    // Reconcile by stable ID; keep the same item for every surviving row.
    const int horizontal = m_tree->horizontalScrollBar()->value();
    const int vertical = m_tree->verticalScrollBar()->value();
    const QSignalBlocker blocker(m_tree);
    m_tree->setUpdatesEnabled(false);
    const bool sorting = m_tree->isSortingEnabled();
    m_tree->setSortingEnabled(false);
    QHash<QString, QTreeWidgetItem *> existing;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *item = m_tree->topLevelItem(i);
        existing.insert(item->data(0, Qt::UserRole).toString(), item);
    }
    QSettings settings;
    for (const auto &row : rows) {
        if (row.id.isEmpty()) continue;
        auto *item = existing.take(row.id);
        if (!item) item = new QTreeWidgetItem(m_tree);
        item->setData(0, Qt::UserRole, row.id);
        item->setData(0, Qt::UserRole + 1, row.details);
        QStringList cells = row.cells;
        if (m_names) cells.prepend(settings.value("names/" + m_kind + "/" + row.id, row.id).toString());
        for (int i = 0; i < cells.size(); ++i) {
            if (item->text(i) != cells[i]) item->setText(i, cells[i]);
            item->setToolTip(i, cells[i]);
        }
    }
    for (auto *item : existing) delete item;
    m_tree->setSortingEnabled(sorting);
    m_tree->horizontalScrollBar()->setValue(horizontal);
    m_tree->verticalScrollBar()->setValue(vertical);
    m_tree->setUpdatesEnabled(true);
    const bool single = m_tree->selectedItems().size() == 1;
    m_detailsButton->setEnabled(single);
    if (m_nameButton) m_nameButton->setEnabled(single);
    updateStatus();
}

void DataTable::setAvailability(bool available, const QString &message)
{
    m_available = available;
    m_message = message;
    updateStatus();
}

void DataTable::updateStatus()
{
    const QString count = QStringLiteral("%1 项").arg(m_tree->topLevelItemCount());
    m_status->setText(count + " · " + m_message + (!m_available && m_tree->topLevelItemCount() ? "（保留上次数据）" : ""));
}

QString DataTable::selectedId() const
{
    const auto selected = m_tree->selectedItems();
    return selected.size() == 1 ? selected.first()->data(0, Qt::UserRole).toString() : QString();
}

void DataTable::renameSelected()
{
    if (!m_names || m_tree->selectedItems().size() != 1) return;
    const QString id = selectedId();
    const QString key = "names/" + m_kind + "/" + id;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("编辑名称"),
        QStringLiteral("为 %1 设置本机名称（留空恢复为 ID）：").arg(id), QLineEdit::Normal,
        QSettings().value(key, id).toString(), &ok).trimmed();
    if (!ok) return;
    QSettings settings;
    if (name.isEmpty() || name == id) settings.remove(key);
    else settings.setValue(key, name);
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *item = m_tree->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == id) {
            item->setText(0, name.isEmpty() ? id : name);
            item->setToolTip(0, item->text(0));
            break;
        }
    }
}

void DataTable::showDetails()
{
    const auto selected = m_tree->selectedItems();
    if (selected.size() != 1) return;
    if (m_details) { m_details->close(); delete m_details; }
    const auto *item = selected.first();
    auto object = item->data(0, Qt::UserRole + 1).toJsonObject();
    if (m_names) object.insert("localName", item->text(0));
    m_details = new QDialog(this, Qt::Tool);
    m_details->setObjectName("detailsPopup");
    m_details->setWindowTitle(QStringLiteral("详细信息 — %1").arg(item->text(0)));
    m_details->resize(680, 480);
    auto *layout = new QVBoxLayout(m_details);
    auto *tabs = new QTabWidget;
    auto *fields = new QTreeWidget;
    fields->setHeaderLabels({QStringLiteral("字段"), QStringLiteral("值")});
    fields->header()->resizeSection(0, 210);
    for (auto it = object.begin(); it != object.end(); ++it) appendJson(fields->invisibleRootItem(), it.key(), it.value());
    fields->expandToDepth(1);
    enableDragScrolling(fields);
    tabs->addTab(fields, QStringLiteral("完整信息"));
    auto *json = new QPlainTextEdit;
    json->setObjectName("detailsJson");
    json->setReadOnly(true);
    json->setPlainText(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented)));
    tabs->addTab(json, QStringLiteral("原始 JSON"));
    layout->addWidget(tabs);
    auto *close = new QPushButton(QStringLiteral("关闭"));
    connect(close, &QPushButton::clicked, m_details, &QDialog::close);
    layout->addWidget(close, 0, Qt::AlignRight);
    m_details->show();
}
