#pragma once
#include <QIcon>
class QApplication;
class QWidget;
namespace UiMotion {
void install(QApplication &app);
QIcon icon(const QString &action);
void transition(QWidget *surface);
}
