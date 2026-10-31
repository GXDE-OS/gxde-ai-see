// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class QMimeData;

/**
 * A drop target for QML that hands the real QMimeData to C++.
 *
 * Qt Quick's DropArea only exposes strings, but the classifier and the
 * staging model need image data and every format.  The \c mime argument of
 * the signals is only valid while the handler runs.
 */
class MimeDropArea : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool containsDrag READ containsDrag NOTIFY containsDragChanged)
    Q_PROPERTY(bool acceptDrop READ acceptDrop WRITE setAcceptDrop NOTIFY acceptDropChanged)

public:
    explicit MimeDropArea(QQuickItem *parent = nullptr);

    bool containsDrag() const { return m_containsDrag; }
    /** When false the drop is refused (the drag source sees it as cancelled). */
    bool acceptDrop() const { return m_acceptDrop; }
    void setAcceptDrop(bool accept);

    static bool supported(const QMimeData *mime);

signals:
    void containsDragChanged();
    void acceptDropChanged();
    void entered(QObject *mime);
    void moved(QObject *mime, qreal x, qreal y);
    void exited();
    void dropped(QObject *mime, qreal x, qreal y);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setContainsDrag(bool contains);

    bool m_containsDrag = false;
    bool m_acceptDrop = true;
};
