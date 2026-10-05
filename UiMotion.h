#pragma once
#include <QIcon>
#include <functional>
class QApplication;
class QWidget;
namespace UiMotion {
void install(QApplication &app);
QIcon icon(const QString &action);
// Capture both states before starting the animation; change executes exactly once.
void transition(QWidget *surface, const std::function<void()> &change, bool themeChange = false);
}
