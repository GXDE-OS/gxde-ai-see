// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DDialog>
#include <DLabel>
#include <QStringList>
#include <QVector>

class QPushButton;

/** Word segmentation result as selectable capsules (DTK dialog). */
class SegmentDialog : public Dtk::Widget::DDialog
{
    Q_OBJECT

public:
    explicit SegmentDialog(const QStringList &words, QWidget *parent = nullptr);

private:
    void updateCount();
    void copySelected();
    void setAll(bool checked);

    QVector<QPushButton *> m_capsules;
    Dtk::Widget::DLabel *m_count = nullptr;
    bool m_selecting = false;
    bool m_selectValue = true;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
};
