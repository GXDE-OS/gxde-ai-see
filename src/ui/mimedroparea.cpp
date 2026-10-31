// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mimedroparea.h"

#include <QMimeData>

MimeDropArea::MimeDropArea(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemAcceptsDrops, true);
}

bool MimeDropArea::supported(const QMimeData *mime)
{
    return mime && (mime->hasUrls() || mime->hasText() || mime->hasImage());
}

void MimeDropArea::setAcceptDrop(bool accept)
{
    if (m_acceptDrop == accept) {
        return;
    }
    m_acceptDrop = accept;
    Q_EMIT acceptDropChanged();
}

void MimeDropArea::setContainsDrag(bool contains)
{
    if (m_containsDrag == contains) {
        return;
    }
    m_containsDrag = contains;
    Q_EMIT containsDragChanged();
}

void MimeDropArea::dragEnterEvent(QDragEnterEvent *event)
{
    auto *mime = const_cast<QMimeData *>(event->mimeData());
    if (!supported(mime)) {
        event->ignore();
        return;
    }
    setContainsDrag(true);
    event->acceptProposedAction();
    Q_EMIT entered(mime);
}

void MimeDropArea::dragMoveEvent(QDragMoveEvent *event)
{
    auto *mime = const_cast<QMimeData *>(event->mimeData());
    if (!supported(mime)) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    Q_EMIT moved(mime, event->position().x(), event->position().y());
}

void MimeDropArea::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
    setContainsDrag(false);
    Q_EMIT exited();
}

void MimeDropArea::dropEvent(QDropEvent *event)
{
    setContainsDrag(false);
    auto *mime = const_cast<QMimeData *>(event->mimeData());
    if (!supported(mime)) {
        event->ignore();
        return;
    }
    if (m_acceptDrop) {
        event->acceptProposedAction();
    } else {
        event->setDropAction(Qt::IgnoreAction);
        event->ignore();
    }
    Q_EMIT dropped(mime, event->position().x(), event->position().y());
}
