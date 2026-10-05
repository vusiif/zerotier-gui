#include "UiMotion.h"
#include <QApplication>
#include <QAbstractButton>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QVariantAnimation>
#include <QPaintEvent>
#include <QLabel>
#include <QPointer>
#include <QTimer>

namespace {
QString kind(const QString &text) {
    if (text.contains("用法")) return "help";
    if (text.contains("版本")) return "version";
    if (text.contains("详细")) return "details";
    if (text.contains("主题")) return "theme";
    if (text.contains("置顶")) return "pin";
    if (text == QStringLiteral("☰")) return "menu";
    if (text.contains("卸载") || text.contains("取消订阅") || text.contains("退出")) return "remove";
    if (text.contains("启动")) return "play";
    if (text.contains("停止")) return "stop";
    if (text.contains("重启") || text.contains("重建") || text.contains("重新")) return "refresh";
    if (text.contains("安装") || text.contains("导出")) return "download";
    if (text.contains("加入") || text.contains("订阅") || text.contains("添加")) return "add";
    if (text.contains("设置")) return "settings";
    if (text.contains("保存")) return "save";
    if (text.contains("编辑")) return "edit";
    if (text.contains("关闭") || text.contains("取消")) return "close";
    if (text.contains("查询") || text.contains("检测")) return "search";
    return "info";
}
class LineIcon : public QIconEngine {
    QString m_action;
public:
    explicit LineIcon(QString action) : m_action(std::move(action)) {}
    QIconEngine *clone() const override { return new LineIcon(m_action); }
    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State) override {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        p->translate(rect.x(), rect.y()); p->scale(rect.width()/24.0, rect.height()/24.0);
        QColor color = qApp->palette().color(mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active, QPalette::ButtonText);
        p->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        auto line=[&](int x,int y,int a,int b){ p->drawLine(x,y,a,b); };
        if (m_action == "menu") { for (int y : {6,12,18}) line(4,y,20,y); }
        else if (m_action == "play") { QPainterPath path; path.moveTo(7,4); path.lineTo(20,12); path.lineTo(7,20); path.closeSubpath(); p->drawPath(path); }
        else if (m_action == "stop") p->drawRoundedRect(QRectF(5,5,14,14),2,2);
        else if (m_action == "add" || m_action == "remove" || m_action == "close") {
            if(m_action == "close") { line(6,6,18,18); line(18,6,6,18); }
            else if (m_action == "network") { p->drawRoundedRect(QRectF(8,2,8,6),1,1); line(12,8,12,12); line(5,12,19,12); line(5,12,5,16); line(19,12,19,16); p->drawRect(QRectF(2,16,6,5)); p->drawRect(QRectF(16,16,6,5)); }
        else if (m_action == "peers") { p->drawEllipse(QRectF(8,3,8,8)); p->drawArc(QRectF(4,12,16,13),0,180*16); }
        else if (m_action == "moon") { QPainterPath shape; shape.moveTo(18,4); shape.cubicTo(2,-1,0,22,16,21); shape.cubicTo(6,16,9,7,18,4); p->drawPath(shape); }
        else if (m_action == "details" || m_action == "version") { p->drawRoundedRect(QRectF(5,3,14,18),2,2); line(8,8,16,8); line(8,12,16,12); line(8,16,13,16); }
        else if (m_action == "help") { p->drawEllipse(QRectF(3,3,18,18)); p->drawArc(QRectF(9,7,6,6),0,180*16); line(15,10,12,14); p->drawPoint(12,17); }
        else { p->drawEllipse(QRectF(3,3,18,18)); line(7,12,17,12); if(m_action=="add") line(12,7,12,17); }
        } else if (m_action == "refresh") { p->drawArc(QRectF(4,4,16,16),35*16,285*16); line(19,3,20,9); line(20,9,14,8); }
        else if (m_action == "download") { line(12,3,12,15); line(7,10,12,15); line(12,15,17,10); line(4,17,4,21); line(4,21,20,21); line(20,21,20,17); }
        else if (m_action == "search") { p->drawEllipse(QRectF(3,3,13,13)); line(15,15,21,21); }
        else if (m_action == "edit") { line(5,17,17,5); line(17,5,20,8); line(20,8,8,20); line(8,20,4,21); line(4,21,5,17); }
        else if (m_action == "pin") { line(8,4,16,4); line(9,4,9,10); line(15,4,15,10); line(9,10,6,14); line(6,14,18,14); line(18,14,15,10); line(12,14,12,22); }
        else if (m_action == "theme") { p->drawEllipse(QRectF(7,7,10,10)); for(int i=0;i<8;++i) { p->save(); p->translate(12,12); p->rotate(i*45); p->drawLine(0,-8,0,-10); p->restore(); } }
        else if (m_action == "settings") { p->drawEllipse(QRectF(8,8,8,8)); for(int i=0;i<8;++i) {p->save();p->translate(12,12);p->rotate(i*45);p->drawLine(0,-7,0,-10);p->restore();} }
        else if (m_action == "save") { p->drawRoundedRect(QRectF(4,3,16,18),2,2); p->drawRect(QRectF(8,3,8,6)); p->drawRect(QRectF(8,14,8,7)); }
        else if (m_action == "network") { p->drawRoundedRect(QRectF(8,2,8,6),1,1); line(12,8,12,12); line(5,12,19,12); line(5,12,5,16); line(19,12,19,16); p->drawRect(QRectF(2,16,6,5)); p->drawRect(QRectF(16,16,6,5)); }
        else if (m_action == "peers") { p->drawEllipse(QRectF(8,3,8,8)); p->drawArc(QRectF(4,12,16,13),0,180*16); }
        else if (m_action == "moon") { QPainterPath shape; shape.moveTo(18,4); shape.cubicTo(2,-1,0,22,16,21); shape.cubicTo(6,16,9,7,18,4); p->drawPath(shape); }
        else if (m_action == "details" || m_action == "version") { p->drawRoundedRect(QRectF(5,3,14,18),2,2); line(8,8,16,8); line(8,12,16,12); line(8,16,13,16); }
        else if (m_action == "help") { p->drawEllipse(QRectF(3,3,18,18)); p->drawArc(QRectF(9,7,6,6),0,180*16); line(15,10,12,14); p->drawPoint(12,17); }
        else { p->drawEllipse(QRectF(3,3,18,18)); line(12,11,12,17); p->drawPoint(12,7); }
        p->restore();
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap image(size); image.fill(Qt::transparent); QPainter painter(&image); paint(&painter,QRect(QPoint(),size),mode,state); return image;
    }
};
class ButtonMotion : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *object, QEvent *event) override {
        auto *button=qobject_cast<QAbstractButton *>(object);
        if (!button || button->inherits("ElaToolButton") || button->inherits("ElaIconButton")) return false;
        if (event->type()==QEvent::Polish && button->icon().isNull() && !button->text().isEmpty()) {
            button->setIcon(UiMotion::icon(kind(button->text())));
            button->setIconSize(QSize(18,18));
        }
        if (event->type()!=QEvent::Enter && event->type()!=QEvent::Leave && event->type()!=QEvent::EnabledChange) return false;
        if (button->icon().isNull()) return false;
        auto *animation=button->findChild<QVariantAnimation *>("hoverMotion", Qt::FindDirectChildrenOnly);
        if (!animation) {
            animation=new QVariantAnimation(button); animation->setObjectName("hoverMotion"); animation->setDuration(150);
            animation->setEasingCurve(QEasingCurve::OutCubic);
            connect(animation,&QVariantAnimation::valueChanged,button,[button](const QVariant &value){
                const double t=value.toDouble(); button->setProperty("hoverProgress",t);
                if (!button->isEnabled() || t < .001) { button->setStyleSheet({}); return; }
                QColor base=qApp->palette().color(QPalette::Button);
                QColor target=qApp->property("darkTheme").toBool()?QColor("#344a5b"):QColor("#e3eff8");
                QColor mix=QColor::fromRgbF(base.redF()*(1-t)+target.redF()*t,base.greenF()*(1-t)+target.greenF()*t,base.blueF()*(1-t)+target.blueF()*t);
                button->setStyleSheet(QString("background-color: %1;").arg(mix.name()));
            });
        }
        animation->stop(); animation->setStartValue(button->property("hoverProgress").toDouble());
        animation->setEndValue(event->type()==QEvent::Enter && button->isEnabled()?1.0:0.0); animation->start();
        return false;
    }
};
}
QIcon UiMotion::icon(const QString &action) { return QIcon(new LineIcon(action)); }
void UiMotion::install(QApplication &app) {
    if (app.property("uiMotionInstalled").toBool()) return;
    app.setProperty("uiMotionInstalled",true); app.installEventFilter(new ButtonMotion(&app));
}
namespace {
// Each frame blends two cached pixmaps. Never re-render a widget hierarchy or
// repolish a stylesheet inside the animation's frame callback.
class TransitionLayer final : public QWidget {
    QPixmap m_before, m_after;
    qreal m_progress = 0;
    bool m_theme;
public:
    TransitionLayer(QWidget *surface, QPixmap before, QPixmap after, bool theme)
        : QWidget(surface), m_before(std::move(before)), m_after(std::move(after)), m_theme(theme) {
        setObjectName("transitionSnapshot");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(surface->rect());
        surface->installEventFilter(this);
        auto *animation = new QVariantAnimation(this);
        animation->setObjectName("surfaceTransition");
        animation->setDuration(theme ? 360 : 300);
        animation->setEasingCurve(QEasingCurve::InOutCubic);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_progress = value.toReal();
            setProperty("transitionProgress", m_progress);
            update();
        });
        connect(animation, &QVariantAnimation::finished, this, [this] {
            hide();
            deleteLater();
        });
        show();
        raise();
        animation->start();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Window));
        // Restrained travel gives pages direction without moving their real
        // controls, scroll position, or click targets.
        const qreal travel = m_theme ? 0 : 12;
        painter.setOpacity(1);
        painter.drawPixmap(QPointF(-travel * m_progress, 0), m_before);
        painter.setOpacity(m_progress);
        painter.drawPixmap(QPointF(travel * (1 - m_progress), 0), m_after);
    }
    bool eventFilter(QObject *object, QEvent *event) override {
        if (object == parentWidget() && (event->type() == QEvent::Resize || event->type() == QEvent::Hide)) {
            hide();
            deleteLater();
        }
        return QWidget::eventFilter(object, event);
    }
};
}
void UiMotion::transition(QWidget *surface, const std::function<void()> &change, bool themeChange) {
    if (!surface || !surface->isVisible() || !surface->updatesEnabled()) { change(); return; }
    // Capture the displayed intermediate frame before removing an interrupted
    // transition, so repeated clicks continue from what the user sees.
    const QPixmap before = surface->grab();
    if (auto *previous = surface->findChild<QWidget *>("transitionSnapshot", Qt::FindDirectChildrenOnly))
        delete previous;
    surface->setUpdatesEnabled(false);
    change();
    surface->setUpdatesEnabled(true);
    const QPixmap after = surface->grab();
    new TransitionLayer(surface, before, after, themeChange);
}
