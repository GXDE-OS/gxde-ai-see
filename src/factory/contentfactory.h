// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

#include "contentclassifier.h"

class QGridLayout;
class QLabel;
class QMimeData;

/**
 * Third box of the panel: shows type aware actions for the dragged payload.
 *
 * Every action button is itself a drop target, so the user can drag the
 * payload straight onto the action and release.
 */
class ContentFactory : public QWidget
{
    Q_OBJECT

public:
    explicit ContentFactory(QWidget *parent = nullptr);

    void setContent(const DropContent &content);
    void clearContent();
    bool hasContent() const { return m_content.valid(); }
    const DropContent &content() const { return m_content; }

signals:
    void actionRequested(const QString &id, const DropContent &content);
    void heightChanged();
    void dragActivity();
    void dragContent(const QMimeData *mime);
    void askRequested();
    void askDropped(const QMimeData *mime);

public:
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void rebuild();

    QLabel *m_summary = nullptr;
    class AskTile *m_askTile = nullptr;
    QWidget *m_actions = nullptr;
    QGridLayout *m_grid = nullptr;
    DropContent m_content;
};
