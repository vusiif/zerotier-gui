#include "UiMotion.h"
#include "AppStyle.h"
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QVariant>
#include <QStyleFactory>

void applyAppStyle(QApplication &app)
{
    UiMotion::install(app);
    setAppDarkTheme(app, QSettings().value("appearance/dark", false).toBool());
}

void setAppDarkTheme(QApplication &app, bool dark)
{
    UiMotion::install(app);
    app.setProperty("darkTheme", dark);
    // Keep dialog text legible even when Windows uses a dark system theme.
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f4f7fa"));
    palette.setColor(QPalette::WindowText, QColor("#20394c"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f1f6fa"));
    palette.setColor(QPalette::Text, QColor("#20394c"));
    palette.setColor(QPalette::Button, QColor("#f7fafc"));
    palette.setColor(QPalette::ButtonText, QColor("#20394c"));
    palette.setColor(QPalette::Highlight, QColor("#d8eaf6"));
    palette.setColor(QPalette::HighlightedText, QColor("#123f60"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#788895"));
    if (dark) {
        palette.setColor(QPalette::Window, QColor("#202830"));
        palette.setColor(QPalette::WindowText, QColor("#e6edf4"));
        palette.setColor(QPalette::Base, QColor("#182129"));
        palette.setColor(QPalette::AlternateBase, QColor("#25313b"));
        palette.setColor(QPalette::Text, QColor("#e6edf4"));
        palette.setColor(QPalette::Button, QColor("#2a3742"));
        palette.setColor(QPalette::ButtonText, QColor("#e6edf4"));
        palette.setColor(QPalette::Highlight, QColor("#354f66"));
        palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#92a2af"));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#92a2af"));
        palette.setColor(QPalette::Link, QColor("#8bcaff"));
    }
    app.setPalette(palette);
    QString css = QStringLiteral(R"CSS(
        QWidget { font-family: "Microsoft YaHei UI", "Segoe UI"; font-size: 13px; }
        QWidget#sidebar { background: #eaf1f7; border-radius: 6px; }
        QLabel#brand { font-size: 23px; font-weight: 600; color: #21486b; }
        QLabel#serviceTitle { font-size: 24px; font-weight: 600; color: #21486b; }
        QLabel#sidebarHint, QLabel#tableStatus { color: #607687; font-size: 12px; }
        QListWidget#navigation { background: transparent; border: none; }
        QListWidget#navigation::item { padding: 12px 8px; border-radius: 4px; }
        QListWidget#navigation::item:selected { background: #d0e4f3; color: #123f60; }
        QTreeWidget { border: 1px solid #d7e1e9; background: white; alternate-background-color: #f1f6fa; }
        QTreeWidget::item { padding: 7px 8px; }
        QTreeWidget::item:selected { background: #d8eaf6; color: #123f60; }
        QHeaderView::section { background: #eaf1f7; color: #294c66; padding: 9px 8px; border: none; border-right: 1px solid #d7e1e9; }
        QPushButton, QToolButton { color: #20394c; background: #f7fafc; border: 1px solid #b9cbd8; border-radius: 8px; padding: 8px 14px; }
        QPushButton:hover, QToolButton:hover { background: #e3eff8; border-color: #5889ad; }
        QPushButton:pressed, QToolButton:pressed { background: #d0e4f3; }
        QPushButton:focus, QToolButton:focus { border: 1px solid #176993; }
        QPushButton:disabled { color: #788895; background: #eff3f6; border-color: #d5dfe6; }
        QMessageBox QPushButton { color: #20394c; background: #f7fafc; min-width: 70px; }
        QLineEdit { background: white; color: #20394c; border: 1px solid #b9cbd8; border-radius: 4px; padding: 7px; }
        QSplitter::handle { background: #d7e1e9; }
        QSplitter::handle:hover { background: #7aa6c5; }
    )CSS");
    if (dark) {
        const QList<QPair<QString, QString>> colors = {
            {"#20394c", "#e6edf4"}, {"#eaf1f7", "#25313b"}, {"#21486b", "#acd6fa"},
            {"#607687", "#acbbc7"}, {"#d0e4f3", "#354f66"}, {"#123f60", "#ffffff"},
            {"#d7e1e9", "#425766"}, {"#f1f6fa", "#25313b"}, {"#d8eaf6", "#354f66"},
            {"#294c66", "#d8e8f5"}, {"#f7fafc", "#2a3742"}, {"#b9cbd8", "#526c80"},
            {"#e3eff8", "#344a5b"}, {"#5889ad", "#8bcaff"}, {"#176993", "#8bcaff"},
            {"#788895", "#92a2af"}, {"#eff3f6", "#26323d"}, {"#d5dfe6", "#425766"},
            {"#7aa6c5", "#8bcaff"}
        };
        for (const auto &color : colors) css.replace(color.first, color.second);
        css.replace("background: white", "background: #182129");
    }
    app.setStyleSheet(css);
}
