// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contentfactory.h"

#include "factoryactions.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

bool mimeSupported(const QMimeData *mime)
{
    return mime && (mime->hasUrls() || mime->hasText() || mime->hasImage());
}

} // namespace

class ActionButton : public QToolButton
{
    Q_OBJECT

public:
    ActionButton(const FactoryAction &action, QWidget *parent)
        : QToolButton(parent)
        , m_id(action.id)
    {
        setText(action.label);
        setToolTip(action.hint);
        setEnabled(action.enabled);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(32);
        setAutoRaise(true);
        setFocusPolicy(Qt::NoFocus);
        setAcceptDrops(true);
        if (!action.icon.isEmpty()) {
            const QIcon icon = QIcon::fromTheme(action.icon);
            if (!icon.isNull()) {
                setIcon(icon);
                setIconSize(QSize(16, 16));
                setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            }
        }
        setStyleSheet(QStringLiteral(
            "QToolButton { color: rgba(255,255,255,0.86); background: rgba(255,255,255,0.07);"
            " border: none; border-radius: 7px; padding: 0 8px; font-size: 12px; }"
            "QToolButton:hover { background: rgba(120,180,255,0.30); color: white; }"
            "QToolButton[dragover=\"true\"] { background: rgba(120,180,255,0.45); color: white;"
            " border: 1px solid rgba(160,205,255,0.9); }"
            "QToolButton:disabled { color: rgba(255,255,255,0.32); background: rgba(255,255,255,0.03); }"));
    }

signals:
    void dropped(const QString &id, const QMimeData *mime);
    void clickedAction(const QString &id);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (!mimeSupported(event->mimeData())) {
            event->ignore();
            return;
        }
        setProperty("dragover", true);
        repolish();
        event->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (mimeSupported(event->mimeData())) {
            event->acceptProposedAction();
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        Q_UNUSED(event)
        setProperty("dragover", false);
        repolish();
    }

    void dropEvent(QDropEvent *event) override
    {
        setProperty("dragover", false);
        repolish();
        if (!mimeSupported(event->mimeData())) {
            event->ignore();
            return;
        }
        event->acceptProposedAction();
        Q_EMIT dropped(m_id, event->mimeData());
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QToolButton::mouseReleaseEvent(event);
        if (isEnabled() && rect().contains(event->position().toPoint())) {
            Q_EMIT clickedAction(m_id);
        }
    }

private:
    void repolish()
    {
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

    QString m_id;
};

ContentFactory::ContentFactory(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setMinimumHeight(44);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 7, 12, 7);
    outer->setSpacing(6);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("🔧 内容工厂"), this);
    QFont titleFont = title->font();
    titleFont.setPixelSize(12);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.88);"));
    header->addWidget(title);

    m_summary = new QLabel(this);
    m_summary->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.45); font-size: 11px;"));
    m_summary->setText(tr("拖到这里，按类型处理"));
    header->addWidget(m_summary, 1);

    auto *hint = new QLabel(tr("拖到动作上松手"), this);
    hint->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.32); font-size: 11px;"));
    header->addWidget(hint);

    outer->addLayout(header);

    m_actions = new QWidget(this);
    m_grid = new QGridLayout(m_actions);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(6);
    m_grid->setVerticalSpacing(6);
    m_actions->hide();
    outer->addWidget(m_actions);
}

void ContentFactory::setContent(const DropContent &content)
{
    m_content = content;
    m_summary->setText(content.summary());
    rebuild();
    m_actions->show();
    Q_EMIT heightChanged();
}

void ContentFactory::clearContent()
{
    m_content = DropContent();
    m_summary->setText(tr("拖到这里，按类型处理"));
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_actions->hide();
    Q_EMIT heightChanged();
}

void ContentFactory::rebuild()
{
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QVector<FactoryAction> actions = actionsFor(m_content);
    int column = 0;
    int row = 0;
    for (const FactoryAction &action : actions) {
        auto *button = new ActionButton(action, m_actions);
        installEventFilter(button);
        connect(button, &ActionButton::clickedAction, this, [this](const QString &id) {
            if (m_content.valid()) {
                Q_EMIT actionRequested(id, m_content);
            }
        });
        connect(button, &ActionButton::dropped, this, [this](const QString &id,
                                                             const QMimeData *mime) {
            const DropContent content = ContentClassifier::classify(mime);
            if (content.valid()) {
                Q_EMIT actionRequested(id, content);
            }
        });
        m_grid->addWidget(button, row, column);
        if (++column == 3) {
            column = 0;
            ++row;
        }
    }
}

void ContentFactory::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF box = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    QPainterPath path;
    path.addRoundedRect(box, 12, 12);
    painter.fillPath(path, QColor(255, 255, 255, m_content.valid() ? 14 : 8));
    painter.setPen(QPen(QColor(255, 255, 255, 42), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

bool ContentFactory::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
        Q_EMIT dragActivity();
    }
    return QWidget::eventFilter(watched, event);
}

#include "contentfactory.moc"
