// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flowlayout.h"

#include <QStyle>
#include <QWidget>

FlowLayout::FlowLayout(QWidget *parent, int margin, int spacing)
    : QLayout(parent)
    , m_spacing(spacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem *item = takeAt(0)) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem *item) { m_items.append(item); }
int FlowLayout::count() const { return m_items.size(); }
QLayoutItem *FlowLayout::itemAt(int index) const { return m_items.value(index); }

QLayoutItem *FlowLayout::takeAt(int index)
{
    return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const { return {}; }
bool FlowLayout::hasHeightForWidth() const { return true; }
int FlowLayout::heightForWidth(int width) const { return doLayout(QRect(0, 0, width, 0), true); }

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const { return minimumSize(); }

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (QLayoutItem *item : m_items) {
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    const QMargins margins = contentsMargins();
    const QRect effective = rect.adjusted(margins.left(), margins.top(), -margins.right(),
                                          -margins.bottom());
    int x = effective.x();
    int y = effective.y();
    int lineHeight = 0;

    for (QLayoutItem *item : m_items) {
        QWidget *widget = item->widget();
        int spaceX = m_spacing;
        int spaceY = m_spacing;
        if (widget) {
            spaceX = widget->style()->layoutSpacing(QSizePolicy::PushButton,
                                                    QSizePolicy::PushButton, Qt::Horizontal);
            spaceY = widget->style()->layoutSpacing(QSizePolicy::PushButton,
                                                    QSizePolicy::PushButton, Qt::Vertical);
        }
        int nextX = x + item->sizeHint().width() + spaceX;
        if (nextX - spaceX > effective.right() && lineHeight > 0) {
            x = effective.x();
            y = y + lineHeight + spaceY;
            nextX = x + item->sizeHint().width() + spaceX;
            lineHeight = 0;
        }
        if (!testOnly) {
            item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
        }
        x = nextX;
        lineHeight = qMax(lineHeight, item->sizeHint().height());
    }
    return y + lineHeight - rect.y() + margins.bottom();
}
