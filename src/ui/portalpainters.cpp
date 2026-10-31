// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalpainters.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QStyleHints>

namespace {

QColor withAlpha(const QColor &color, int alpha)
{
    QColor c = color;
    c.setAlpha(alpha);
    return c;
}

const QColor kAccent(74, 144, 255);
const QColor kAccentLight(120, 180, 255);

QRect checkRect(const QRect &row)
{
    return QRect(row.left() + 12, row.center().y() - 9, 18, 18);
}

QRect iconRect(const QRect &row)
{
    return QRect(row.left() + 42, row.center().y() - 17, 34, 34);
}

QRect closeRect(const QRect &row)
{
    return QRect(row.right() - 36, row.center().y() - 14, 28, 28);
}

QRect pinRect(const QRect &row)
{
    return QRect(row.right() - 66, row.center().y() - 14, 28, 28);
}

QRect textRect(const QRect &row)
{
    const QRect icon = iconRect(row);
    const QRect pin = pinRect(row);
    return QRect(icon.right() + 12, row.top() + 6, pin.left() - icon.right() - 20,
                 row.height() - 12);
}

void drawPushpin(QPainter *painter, const QRect &rect, const QColor &color, bool pinned)
{
    painter->save();
    painter->translate(rect.center());
    painter->rotate(45.0);
    painter->setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(pinned ? QBrush(color) : Qt::NoBrush);
    painter->drawRoundedRect(QRectF(-4.5, -9.0, 9.0, 7.0), 3.0, 3.0);
    painter->drawLine(QPointF(0.0, -2.0), QPointF(0.0, 7.5));
    painter->restore();
}

} // namespace

/* ------------------------------------------------------------------------- */
/* StagingRowItem                                                            */
/* ------------------------------------------------------------------------- */

StagingRowItem::StagingRowItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    connect(this, &StagingRowItem::appearanceChanged, this, [this]() { update(); });
}

QRect StagingRowItem::rowRect() const
{
    return QRect(0, 0, int(width()), int(height()));
}

void StagingRowItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_removeProgress > 0.0) {
        painter->setOpacity(1.0 - m_removeProgress);
        painter->translate(m_removeProgress * 48.0, 0.0);
    }

    const QRect full = rowRect();
    const QRect row = full.adjusted(4, 2, -4, -2);
    const bool hovered = m_hovered || m_highlighted;

    if (hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(withAlpha(m_foreground, 26));
        painter->drawRoundedRect(row, 8, 8);
    }

    const QRect check = checkRect(full);
    painter->setPen(QPen(m_checked ? kAccent : withAlpha(m_foreground, 80), 1.4));
    painter->setBrush(m_checked ? kAccent : withAlpha(m_foreground, 12));
    painter->drawRoundedRect(check, 5, 5);

    if (m_checked) {
        QPainterPath mark;
        mark.moveTo(check.left() + 4.5, check.center().y());
        mark.lineTo(check.left() + 7.5, check.bottom() - 5);
        mark.lineTo(check.right() - 4.0, check.top() + 5.0);
        painter->setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(mark);
    }

    const QIcon icon = m_icon.value<QIcon>();
    if (!icon.isNull()) {
        icon.paint(painter, iconRect(full), Qt::AlignCenter);
    }

    const QRect text = textRect(full);

    QFont nameFont = QGuiApplication::font();
    nameFont.setPixelSize(14);
    nameFont.setWeight(QFont::Medium);
    painter->setFont(nameFont);
    painter->setPen(withAlpha(m_foreground, 236));
    const QFontMetrics nameMetrics(nameFont);
    const QRect nameRect(text.left(), text.top() + 2, text.width(), 20);
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(m_name, Qt::ElideMiddle, nameRect.width()));

    QFont subFont = QGuiApplication::font();
    subFont.setPixelSize(11);
    painter->setFont(subFont);
    painter->setPen(withAlpha(m_foreground, 120));
    const QRect subRect(text.left(), text.top() + 24, text.width(), 16);
    painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, m_typeLabel);

    if (m_pinned || hovered) {
        drawPushpin(painter, pinRect(full),
                    m_pinned ? withAlpha(kAccentLight, 235) : withAlpha(m_foreground, 150),
                    m_pinned);
    }

    if (hovered) {
        const QRect close = closeRect(full).adjusted(9, 9, -9, -9);
        painter->setPen(QPen(withAlpha(m_foreground, 170), 1.7, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(close.topLeft(), close.bottomRight());
        painter->drawLine(close.topRight(), close.bottomLeft());
    }
}

void StagingRowItem::hoverEnterEvent(QHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void StagingRowItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    update();
}

void StagingRowItem::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const QRect full = rowRect();
    m_pressed = true;
    m_pressPos = event->position();
    m_pressOnButton = checkRect(full).contains(pos) || pinRect(full).contains(pos) ||
                      closeRect(full).contains(pos);
    event->accept();
}

void StagingRowItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed || m_pressOnButton) {
        return;
    }
    if ((event->position() - m_pressPos).manhattanLength() <
        QGuiApplication::styleHints()->startDragDistance()) {
        return;
    }
    /* QDrag::exec swallows the release, so forget the press first */
    m_pressed = false;
    ungrabMouse();
    Q_EMIT dragRequested();
}

void StagingRowItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_pressed) {
        return;
    }
    m_pressed = false;

    const QPoint pos = event->position().toPoint();
    const QRect full = rowRect();
    if (checkRect(full).contains(pos)) {
        Q_EMIT checkClicked();
    } else if (pinRect(full).contains(pos)) {
        Q_EMIT pinClicked();
    } else if (closeRect(full).contains(pos)) {
        Q_EMIT closeClicked();
    }
}

/* ------------------------------------------------------------------------- */
/* PortalZoneItem                                                            */
/* ------------------------------------------------------------------------- */

PortalZoneItem::PortalZoneItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptHoverEvents(true);
    connect(this, &PortalZoneItem::appearanceChanged, this, [this]() { update(); });
}

void PortalZoneItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int w = int(width());
    const int h = int(height());
    const QRectF box = QRectF(0, 0, w, h).adjusted(1.0, 1.0, -1.0, -1.0);
    QPainterPath path;
    path.addRoundedRect(box, 14, 14);

    QColor fill = withAlpha(m_foreground, 10);
    if (m_active) {
        fill = withAlpha(kAccent, 62);
    } else if (m_hovered) {
        fill = withAlpha(m_foreground, 20);
    }
    painter->fillPath(path, fill);

    const QColor border = m_active ? withAlpha(kAccentLight, 230) : withAlpha(m_foreground, 58);
    painter->setPen(QPen(border, 1.5, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);

    QFont titleFont = QGuiApplication::font();
    titleFont.setPixelSize(22);
    titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont);
    painter->setPen(withAlpha(m_foreground, 242));
    painter->drawText(QRect(0, h / 3 - 18, w, 32), Qt::AlignHCenter, m_title);

    QFont subFont = QGuiApplication::font();
    subFont.setPixelSize(12);
    painter->setFont(subFont);
    painter->setPen(withAlpha(m_foreground, 150));
    painter->drawText(QRect(12, h / 3 + 20, w - 24, 40), Qt::AlignHCenter | Qt::TextWordWrap,
                      m_subtitle);
}

void PortalZoneItem::hoverEnterEvent(QHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void PortalZoneItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    update();
}
