// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

/*
 * The custom painted parts of the panel, drawn with QPainter like the
 * widget version was.  Colours come from QML (GxdePalette) so they follow
 * the light / dark theme; the alphas are the original ones.
 */

/** One row of the staging list: check box, icon, name, type, pin, close. */
class StagingRowItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString name MEMBER m_name NOTIFY appearanceChanged)
    Q_PROPERTY(QString typeLabel MEMBER m_typeLabel NOTIFY appearanceChanged)
    Q_PROPERTY(QVariant icon MEMBER m_icon NOTIFY appearanceChanged)
    Q_PROPERTY(bool checked MEMBER m_checked NOTIFY appearanceChanged)
    Q_PROPERTY(bool pinned MEMBER m_pinned NOTIFY appearanceChanged)
    /* hover coming from a drag, which the item itself does not see */
    Q_PROPERTY(bool highlighted MEMBER m_highlighted NOTIFY appearanceChanged)
    Q_PROPERTY(qreal removeProgress MEMBER m_removeProgress NOTIFY appearanceChanged)
    Q_PROPERTY(QColor foreground MEMBER m_foreground NOTIFY appearanceChanged)

public:
    explicit StagingRowItem(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

signals:
    void appearanceChanged();
    void checkClicked();
    void pinClicked();
    void closeClicked();
    void dragRequested();

protected:
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRect rowRect() const;

    QString m_name;
    QString m_typeLabel;
    QVariant m_icon;
    bool m_checked = false;
    bool m_pinned = false;
    bool m_highlighted = false;
    bool m_hovered = false;
    qreal m_removeProgress = 0.0;
    QColor m_foreground = Qt::white;

    bool m_pressed = false;
    bool m_pressOnButton = false;
    QPointF m_pressPos;
};

/** The 传送门 drop zone: rounded box, dashed border, title. */
class PortalZoneItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString title MEMBER m_title NOTIFY appearanceChanged)
    Q_PROPERTY(QString subtitle MEMBER m_subtitle NOTIFY appearanceChanged)
    Q_PROPERTY(bool active MEMBER m_active NOTIFY appearanceChanged)
    Q_PROPERTY(QColor foreground MEMBER m_foreground NOTIFY appearanceChanged)

public:
    explicit PortalZoneItem(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

signals:
    void appearanceChanged();

protected:
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;

private:
    QString m_title;
    QString m_subtitle;
    bool m_active = false;
    bool m_hovered = false;
    QColor m_foreground = Qt::white;
};
