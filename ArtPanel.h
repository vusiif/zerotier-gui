#pragma once
#include <QWidget>
#include <QPixmap>

// Only visible panels retain decoded artwork; the original photos stay untouched.
class ArtPanel : public QWidget {
public:
    ArtPanel(const QString &collection, const QString &caption, QWidget *parent = nullptr);
    void setCaption(const QString &caption);
protected:
    void paintEvent(QPaintEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    QString m_collection;
    QString m_caption;
    QString m_loadedPath;
    QPixmap m_image;
};
