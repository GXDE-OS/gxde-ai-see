// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todowindow.h"

#include "todomodel.h"

#include "config.h"

#include <QContextMenuEvent>
#include <QMenu>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#ifdef GXDE_HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

namespace {

constexpr int kWidth = 320;
constexpr int kRowHeight = 40;

QRect checkRect(const QRect &row)
{
    return QRect(row.left() + 12, row.center().y() - 9, 18, 18);
}

QRect deleteRect(const QRect &row)
{
    return QRect(row.right() - 34, row.center().y() - 13, 26, 26);
}

} // namespace

class TodoDelegate : public QStyledItemDelegate
{
public:
    explicit TodoDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void setHoverRow(int row) { m_hoverRow = row; }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(0, kRowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect row = option.rect.adjusted(4, 2, -4, -2);
        const bool hovered = index.row() == m_hoverRow;
        const bool done = index.data(TodoModel::DoneRole).toBool();

        if (hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, 24));
            painter->drawRoundedRect(row, 8, 8);
        }

        const QRect check = checkRect(option.rect);
        painter->setPen(QPen(done ? QColor(74, 144, 255) : QColor(255, 255, 255, 90), 1.4));
        painter->setBrush(done ? QColor(74, 144, 255) : QColor(255, 255, 255, 12));
        painter->drawRoundedRect(check, 5, 5);
        if (done) {
            QPainterPath mark;
            mark.moveTo(check.left() + 4.5, check.center().y());
            mark.lineTo(check.left() + 7.5, check.bottom() - 5);
            mark.lineTo(check.right() - 4.0, check.top() + 5.0);
            painter->setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(mark);
        }

        QFont font = option.font;
        font.setPixelSize(13);
        font.setStrikeOut(done);
        painter->setFont(font);
        painter->setPen(done ? QColor(255, 255, 255, 100) : QColor(255, 255, 255, 232));

        const QRect textRect(check.right() + 12, option.rect.top(),
                             deleteRect(option.rect).left() - check.right() - 22,
                             option.rect.height());
        const QString text = index.data(TodoModel::TextRole).toString();
        const QFontMetrics metrics(font);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(text, Qt::ElideRight, textRect.width()));

        if (hovered) {
            const QRect del = deleteRect(option.rect).adjusted(8, 8, -8, -8);
            painter->setPen(QPen(QColor(255, 255, 255, 165), 1.6, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(del.topLeft(), del.bottomRight());
            painter->drawLine(del.topRight(), del.bottomLeft());
        }

        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (event->type() != QEvent::MouseButtonRelease) {
            return false;
        }
        auto *mouse = static_cast<QMouseEvent *>(event);
        const QPoint pos = mouse->position().toPoint();
        auto *todo = static_cast<TodoModel *>(model);

        if (checkRect(option.rect).contains(pos)) {
            todo->setDone(index.row(), !index.data(TodoModel::DoneRole).toBool());
            return true;
        }
        if (deleteRect(option.rect).contains(pos)) {
            todo->removeRow(index.row());
            return true;
        }
        return false;
    }

private:
    int m_hoverRow = -1;
};

TodoWindow::TodoWindow(TodoModel *model, QWidget *parent)
    : Dtk::Widget::DBlurEffectWidget(parent)
    , m_model(model)
{
    setWindowTitle(tr("待办"));
    setWindowIcon(QIcon::fromTheme(
        QStringLiteral("gxde-ai-see-todo"),
        QIcon(QStringLiteral("/usr/share/icons/hicolor/256x256/apps/gxde-ai-see-todo.png"))));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMaskColor(QColor(24, 24, 28, 190));
    setBlurRectXRadius(16);
    setBlurRectYRadius(16);
    setFixedWidth(kWidth);

    m_rightMargin = Config::instance()->getInt(QStringLiteral("todo/rightMargin"), 16);
    m_topMargin = Config::instance()->getInt(QStringLiteral("todo/topMargin"), 72);
    m_keepAbove = Config::instance()->getBool(QStringLiteral("todo/keepAbove"), true);

    setupWindow();
    setupUi();

    connect(m_model, &TodoModel::countChanged, this, [this]() {
        m_countLabel->setText(tr("%1 项").arg(m_model->count()));
        refreshLayout();
        m_view->viewport()->update();
    });
}

void TodoWindow::setupWindow()
{
    if (!m_keepAbove) {
        /* plain toplevel: shows in the taskbar and can be covered/moved */
        setWindowFlag(Qt::Tool, false);
        return;
    }
#ifdef GXDE_HAVE_LAYERSHELLQT
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        createWinId();
        if (auto *layer = LayerShellQt::Window::get(windowHandle())) {
            LayerShellQt::Window::Anchors anchors;
            anchors |= LayerShellQt::Window::AnchorRight;
            anchors |= LayerShellQt::Window::AnchorTop;
            layer->setAnchors(anchors);
            layer->setMargins(QMargins(0, m_topMargin, m_rightMargin, 0));
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            layer->setExclusiveZone(0);
            layer->setScope(QStringLiteral("gxde-ai-see-todo"));
            layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
        }
    }
#endif
}

void TodoWindow::setupUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 10, 12, 10);
    outer->setSpacing(8);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(4, 0, 4, 0);
    header->setSpacing(8);

    auto *title = new QLabel(tr("待办"), this);
    QFont titleFont = title->font();
    titleFont.setPixelSize(13);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.9);"));
    header->addWidget(title);

    m_countLabel = new QLabel(tr("%1 项").arg(m_model->count()), this);
    m_countLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.45); font-size: 11px;"));
    header->addWidget(m_countLabel);
    header->addStretch();

    auto *addButton = new QPushButton(tr("＋"), this);
    addButton->setFixedSize(26, 26);
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setFocusPolicy(Qt::NoFocus);
    addButton->setToolTip(tr("新建待办"));
    addButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: rgba(255,255,255,0.8); background: rgba(255,255,255,0.08);"
        " border: none; border-radius: 6px; font-size: 14px; }"
        "QPushButton:hover { background: rgba(120,180,255,0.3); color: white; }"));
    connect(addButton, &QPushButton::clicked, this, &TodoWindow::addEntry);
    header->addWidget(addButton);

    auto *closeButton = new QPushButton(tr("×"), this);
    closeButton->setFixedSize(26, 26);
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setToolTip(tr("隐藏"));
    closeButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: rgba(255,255,255,0.7); background: transparent;"
        " border: none; font-size: 16px; }"
        "QPushButton:hover { color: white; }"));
    connect(closeButton, &QPushButton::clicked, this, &QWidget::hide);
    header->addWidget(closeButton);

    outer->addLayout(header);

    m_view = new QListView(this);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setMouseTracking(true);
    m_view->viewport()->setMouseTracking(true);
    m_view->setStyleSheet(QStringLiteral("QListView { background: transparent; }"));

    auto *delegate = new TodoDelegate(m_view);
    m_view->setModel(m_model);
    m_view->setItemDelegate(delegate);
    m_view->setUniformItemSizes(true);

    connect(m_view, &QListView::entered, this, [delegate](const QModelIndex &index) {
        delegate->setHoverRow(index.row());
    });

    outer->addWidget(m_view, 1);
}

void TodoWindow::popup()
{
    refreshLayout();
    show();
    raise();
}

void TodoWindow::refreshLayout()
{
    QScreen *screen = this->screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const int screenHeight = screen ? screen->availableGeometry().height() : 900;
    const int content = m_model->count() * kRowHeight + 8;
    const int listHeight = qBound(80, content, qMax(160, screenHeight / 2));
    if (m_view) {
        m_view->setFixedHeight(listHeight);
    }
    resize(kWidth, listHeight + 56);
}

void TodoWindow::addEntry()
{
    QDialog dialog(nullptr);
    dialog.setWindowTitle(tr("新建待办"));
    dialog.setMinimumWidth(320);

    auto *layout = new QVBoxLayout(&dialog);
    auto *edit = new QLineEdit(&dialog);
    edit->setPlaceholderText(tr("要做的事…"));
    layout->addWidget(edit);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        m_model->add(edit->text());
        refreshLayout();
    }
}

void TodoWindow::applyMargins()
{
    if (!m_keepAbove) {
        return;
    }
#ifdef GXDE_HAVE_LAYERSHELLQT
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        if (auto *layer = LayerShellQt::Window::get(windowHandle())) {
            layer->setMargins(QMargins(0, m_topMargin, m_rightMargin, 0));
            return;
        }
    }
#endif
    if (QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen()) {
        const QRect available = scr->availableGeometry();
        move(available.right() - width() - m_rightMargin, available.top() + m_topMargin);
    }
}

void TodoWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *pin = menu.addAction(tr("置顶"));
    pin->setCheckable(true);
    pin->setChecked(m_keepAbove);
    QAction *hideAction = menu.addAction(tr("隐藏"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == hideAction) {
        hide();
        return;
    }
    if (chosen == pin) {
        const bool keep = pin->isChecked();
        Config::instance()->set(QStringLiteral("todo/keepAbove"), keep);
        Config::instance()->sync();
        Q_EMIT recreateRequested();
    }
}

void TodoWindow::saveMargins()
{
    Config::instance()->set(QStringLiteral("todo/rightMargin"), m_rightMargin);
    Config::instance()->set(QStringLiteral("todo/topMargin"), m_topMargin);
    Config::instance()->sync();
}

void TodoWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->position().y() < 48) {
        if (!m_keepAbove) {
            if (windowHandle()) {
                windowHandle()->startSystemMove();
            }
            event->accept();
            return;
        }
        m_dragging = true;
        m_dragStart = event->position().toPoint();
        m_dragStartRight = m_rightMargin;
        m_dragStartTop = m_topMargin;
        event->accept();
        return;
    }
    Dtk::Widget::DBlurEffectWidget::mousePressEvent(event);
}

void TodoWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint delta = event->position().toPoint() - m_dragStart;
        m_rightMargin = qMax(0, m_dragStartRight - delta.x());
        m_topMargin = qMax(0, m_dragStartTop + delta.y());
        applyMargins();
        event->accept();
        return;
    }
    Dtk::Widget::DBlurEffectWidget::mouseMoveEvent(event);
}

void TodoWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_dragging = false;
        saveMargins();
        event->accept();
        return;
    }
    Dtk::Widget::DBlurEffectWidget::mouseReleaseEvent(event);
}

void TodoWindow::showEvent(QShowEvent *event)
{
    Dtk::Widget::DBlurEffectWidget::showEvent(event);
    refreshLayout();
    applyMargins();
}
