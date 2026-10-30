// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "segmentdialog.h"

#include "flowlayout.h"

#include <QApplication>
#include <QPalette>
#include <QClipboard>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

SegmentDialog::SegmentDialog(const QStringList &words, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("分词"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(560, 420);

    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const QColor capsuleBg = dark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 18);
    const QColor capsuleBorder = dark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 28);
    const QColor textColor = palette().color(QPalette::WindowText);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr("按住并滑过词语可以连续选择，选中后复制。"), this);
    hint->setStyleSheet(QStringLiteral("font-size: 12px;"));
    layout->addWidget(hint);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *container = new QWidget(scroll);
    auto *flow = new FlowLayout(container, 6, 6);
    container->setLayout(flow);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);

    for (const QString &word : words) {
        auto *capsule = new QPushButton(word, container);
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

    auto *bottom = new QHBoxLayout;
    auto *all = new QPushButton(tr("全选"), this);
    auto *none = new QPushButton(tr("清空"), this);
    m_count = new QLabel(tr("已选 0 个"), this);
    m_count->setStyleSheet(QStringLiteral("font-size: 12px;"));
    auto *copy = new QPushButton(tr("复制选中"), this);
    copy->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; background: rgba(74,144,255,0.85); border: none;"
        " border-radius: 8px; padding: 6px 14px; }"
        "QPushButton:hover { background: rgba(90,160,255,0.95); }"));
    bottom->addWidget(all);
    bottom->addWidget(none);
    bottom->addStretch();
    bottom->addWidget(m_count);
    bottom->addSpacing(10);
    bottom->addWidget(copy);
    layout->addLayout(bottom);

    connect(all, &QPushButton::clicked, this, [this]() { setAll(true); });
    connect(none, &QPushButton::clicked, this, [this]() { setAll(false); });
    connect(copy, &QPushButton::clicked, this, &SegmentDialog::copySelected);

    auto *close = new QPushButton(tr("关闭"), this);
    layout->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
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
        return QDialog::eventFilter(watched, event);
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
    return QDialog::eventFilter(watched, event);
}

void SegmentDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_selecting = false;
    QDialog::mouseReleaseEvent(event);
}
