#include "ArtPanel.h"
#include <QApplication>
#include <QVariant>
#include <QPainter>
#include <QPainterPath>
#include <QImageReader>
#include <QHideEvent>
#include <QEvent>

ArtPanel::ArtPanel(const QString &collection, const QString &caption, QWidget *parent)
    : QWidget(parent), m_collection(collection), m_caption(caption)
{
    setObjectName("pageArtwork");
    setFixedHeight(150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

}
void ArtPanel::setCaption(const QString &caption)
{
    m_caption = caption;
    update();
}
bool ArtPanel::event(QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange) {
        m_image = QPixmap();
        m_loadedPath.clear();
        update();
    }
    return QWidget::event(event);
}
void ArtPanel::hideEvent(QHideEvent *event)
{
    m_image = QPixmap(); m_loadedPath.clear();
    QWidget::hideEvent(event);
}
void ArtPanel::paintEvent(QPaintEvent *)
{
    const bool dark = qApp->property("darkTheme").toBool();
    const QString path = ":/art/" + m_collection + (dark ? "-dark.jpg" : "-light.jpg");
    if (m_loadedPath != path) {
        QImageReader reader(path);
        const auto size = reader.size();
        // Limit decoded RAM even on a large or high DPI window.
        reader.setScaledSize(size.scaled(960, 600, Qt::KeepAspectRatio));
        m_image = QPixmap::fromImage(reader.read());
        m_loadedPath = path;
        setProperty("artResource", path);
        setProperty("artLoaded", !m_image.isNull());
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip; clip.addRoundedRect(QRectF(rect()), 14, 14);
    painter.setClipPath(clip);
    const QColor background = dark ? QColor("#202537") : QColor("#eaf0fb");
    painter.fillRect(rect(), background);
    if (!m_image.isNull()) {
        const QRectF target(width() / 4.0, 0, width() * .75, height());
        const qreal scale = qMax(target.width() / m_image.width(), target.height() / m_image.height());
        const QSizeF sourceSize(target.width() / scale, target.height() / scale);
        // Portrait artwork keeps the upper portion in view, rather than cropping to the waist.
        const QRectF source((m_image.width() - sourceSize.width()) / 2,
                            (m_image.height() - sourceSize.height()) * .15,
                            sourceSize.width(), sourceSize.height());
        painter.drawPixmap(target, m_image, source);
    }
    QLinearGradient fade(0, 0, width(), 0);
    fade.setColorAt(0, background);
    fade.setColorAt(.32, background);
    auto transparent = background; transparent.setAlpha(0);
    fade.setColorAt(.75, transparent);
    painter.fillRect(rect(), fade);
    painter.setPen(dark ? QColor("#f4f6ff") : QColor("#23324c"));
    auto heading = font(); heading.setPointSize(17); heading.setBold(true); painter.setFont(heading);
    painter.drawText(QRect(24, 24, qMax(1, width() / 2 - 24), height() - 48), Qt::AlignVCenter | Qt::TextWordWrap, m_caption);
}
