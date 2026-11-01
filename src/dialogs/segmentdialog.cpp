// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "segmentdialog.h"

#include "flowlayout.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

#include <DLabel>
#include <DPushButton>

SegmentDialog::SegmentDialog(const QStringList &words, QWidget *parent)
    : Dtk::Widget::DDialog(parent)
{
    setTitle(tr("分词"));
    setAttribute(Qt::WA_DeleteOnClose);

    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const QColor capsuleBg = dark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 18);
    const QColor capsuleBorder = dark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 28);
    const QColor textColor = palette().color(QPalette::WindowText);

    auto *content = new QWidget;
    content->setMinimumWidth(500);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *hint = new Dtk::Widget::DLabel(tr("按住并滑过词语可以连续选择，选中后复制。"), content);
    hint->setStyleSheet(QStringLiteral("font-size: 12px;"));
    layout->addWidget(hint);

    auto *scroll = new QScrollArea(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    scroll->viewport()->setAutoFillBackground(false);
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *container = new QWidget(scroll);
    container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *flow = new FlowLayout(container, 6, 6);
    container->setLayout(flow);
    scroll->setWidget(container);
    scroll->setMinimumHeight(220);
    layout->addWidget(scroll, 1);

    for (const QString &word : words) {
        auto *capsule = new Dtk::Widget::DPushButton(word, container);
        capsule->setCheckable(true);
        capsule->setCursor(Qt::PointingHandCursor);
        capsule->setFocusPolicy(Qt::NoFocus);
        capsule->setStyleSheet(QStringLiteral(
            "QPushButton { color: %1; background: %2; border: 1px solid %3;"
            " border-radius: 11px; padding: 3px 10px; font-size: 13px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:checked { background: rgba(74,144,255,0.85);"
            " border-color: rgba(74,144,255,1.0); color: white; }")
                                    .arg(textColor.name(),
                                         QStringLiteral("rgba(%1,%2,%3,%4)")
                                             .arg(capsuleBg.red())
                                             .arg(capsuleBg.green())
                                             .arg(capsuleBg.blue())
                                             .arg(capsuleBg.alpha()),
                                         capsuleBorder.name(),
                                         dark ? QStringLiteral("rgba(255,255,255,30)")
                                              : QStringLiteral("rgba(0,0,0,28)")));
        capsule->installEventFilter(this);
        connect(capsule, &QPushButton::released, this, [this]() { m_selecting = false; });
        flow->addWidget(capsule);
        m_capsules.append(capsule);
    }

    auto *countRow = new QHBoxLayout;
    m_count = new Dtk::Widget::DLabel(tr("已选 0 个"), content);
    m_count->setStyleSheet(QStringLiteral("font-size: 12px;"));
    countRow->addStretch();
    countRow->addWidget(m_count);
    layout->addLayout(countRow);

    setContentLayoutContentsMargins(QMargins(16, 12, 16, 4));
    addContent(content);

    const int allButton = addButton(tr("全选"));
    const int noneButton = addButton(tr("清空"));
    const int copyButton = addButton(tr("复制选中"), true, ButtonRecommend);
    const int closeButton = addButton(tr("关闭"));
    setOnButtonClickedClose(false);
    connect(this, &Dtk::Widget::DDialog::buttonClicked, this,
            [this, allButton, noneButton, copyButton, closeButton](int index,
                                                                   const QString &) {
                if (index == allButton) {
                    setAll(true);
                } else if (index == noneButton) {
                    setAll(false);
                } else if (index == copyButton) {
                    copySelected();
                } else if (index == closeButton) {
                    close();
                }
            });
}

void SegmentDialog::setAll(bool checked)
{
    for (QPushButton *capsule : m_capsules) {
        capsule->setChecked(checked);
    }
    updateCount();
}

void SegmentDialog::updateCount()
{
    int count = 0;
    for (QPushButton *capsule : m_capsules) {
        if (capsule->isChecked()) {
            ++count;
        }
    }
    m_count->setText(tr("已选 %1 个").arg(count));
}

void SegmentDialog::copySelected()
{
    QStringList selected;
    for (QPushButton *capsule : m_capsules) {
        if (capsule->isChecked()) {
            selected.append(capsule->text());
        }
    }
    if (selected.isEmpty()) {
        m_count->setText(tr("还没有选中词语"));
        return;
    }
    QApplication::clipboard()->setText(selected.join(QLatin1Char(' ')));
    m_count->setText(tr("已复制 %1 个").arg(selected.size()));
}

bool SegmentDialog::eventFilter(QObject *watched, QEvent *event)
{
    auto *capsule = qobject_cast<QPushButton *>(watched);
    if (!capsule) {
        return Dtk::Widget::DDialog::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_selecting = true;
            m_selectValue = !capsule->isChecked();
            capsule->setChecked(m_selectValue);
            updateCount();
        }
    } else if (event->type() == QEvent::Enter && m_selecting &&
               (QApplication::mouseButtons() & Qt::LeftButton)) {
        if (capsule->isChecked() != m_selectValue) {
            capsule->setChecked(m_selectValue);
            updateCount();
        }
    }
    return Dtk::Widget::DDialog::eventFilter(watched, event);
}

void SegmentDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_selecting = false;
    Dtk::Widget::DDialog::mouseReleaseEvent(event);
}
