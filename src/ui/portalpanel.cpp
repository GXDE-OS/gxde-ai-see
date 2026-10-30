// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalpanel.h"

#include "config.h"
#include "staging/stagingitem.h"
#include "staging/stagingmodel.h"

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifdef GXDE_HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

namespace {

constexpr int kPanelWidth = 500;
constexpr int kZoneWidth = 176;
constexpr int kRowHeight = 56;
constexpr int kMargin = 12;
constexpr int kMaxListHeight = 0; // computed from the screen

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

QRect textRect(const QRect &row)
{
    const QRect icon = iconRect(row);
    const QRect close = closeRect(row);
    return QRect(icon.right() + 12, row.top() + 6, close.left() - icon.right() - 20,
                 row.height() - 12);
}

bool mimeSupported(const QMimeData *mime)
{
    return mime && (mime->hasUrls() || mime->hasText() || mime->hasImage());
}

QString typeLabel(const QModelIndex &index)
{
    const int type = index.data(StagingModel::TypeRole).toInt();
    switch (type) {
    case StagingItem::File:
        return QObject::tr("文件");
    case StagingItem::Image:
        return QObject::tr("图片");
    case StagingItem::Text:
        return QObject::tr("文字 · %1 字")
            .arg(index.data(StagingModel::TextRole).toString().size());
    default:
        return QString();
    }
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Delegate                                                                  */
/* ------------------------------------------------------------------------- */

class StagingDelegate : public QStyledItemDelegate
{
public:
    explicit StagingDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void setHoverRow(int row)
    {
        m_hoverRow = row;
    }

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

        if (hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, 26));
            painter->drawRoundedRect(row, 8, 8);
        }

        const bool checked = index.data(StagingModel::CheckedRole).toBool();
        const QRect check = checkRect(option.rect);
        painter->setPen(QPen(checked ? QColor(74, 144, 255) : QColor(255, 255, 255, 80), 1.4));
        painter->setBrush(checked ? QColor(74, 144, 255) : QColor(255, 255, 255, 12));
        painter->drawRoundedRect(check, 5, 5);

        if (checked) {
            QPainterPath mark;
            mark.moveTo(check.left() + 4.5, check.center().y());
            mark.lineTo(check.left() + 7.5, check.bottom() - 5);
            mark.lineTo(check.right() - 4.0, check.top() + 5.0);
            painter->setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(mark);
        }

        const QIcon icon = index.data(StagingModel::IconRole).value<QIcon>();
        if (!icon.isNull()) {
            icon.paint(painter, iconRect(option.rect), Qt::AlignCenter);
        }

        const QRect text = textRect(option.rect);

        QFont nameFont = option.font;
        nameFont.setPixelSize(14);
        nameFont.setWeight(QFont::Medium);
        painter->setFont(nameFont);
        painter->setPen(QColor(255, 255, 255, 236));

        const QFontMetrics nameMetrics(nameFont);
        const QString name = index.data(StagingModel::NameRole).toString();
        const QRect nameRect(text.left(), text.top() + 2, text.width(), 20);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                          nameMetrics.elidedText(name, Qt::ElideMiddle, nameRect.width()));

        QFont subFont = option.font;
        subFont.setPixelSize(11);
        painter->setFont(subFont);
        painter->setPen(QColor(255, 255, 255, 120));
        const QRect subRect(text.left(), text.top() + 24, text.width(), 16);
        painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, typeLabel(index));

        if (hovered) {
            const QRect close = closeRect(option.rect).adjusted(9, 9, -9, -9);
            painter->setPen(QPen(QColor(255, 255, 255, 170), 1.7, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(close.topLeft(), close.bottomRight());
            painter->drawLine(close.topRight(), close.bottomLeft());
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

        if (checkRect(option.rect).contains(pos)) {
            auto *staging = static_cast<StagingModel *>(model);
            staging->setChecked(index.row(), !index.data(StagingModel::CheckedRole).toBool());
            return true;
        }

        if (closeRect(option.rect).contains(pos)) {
            model->removeRow(index.row());
            return true;
        }

        return false;
    }

private:
    int m_hoverRow = -1;
};

/* ------------------------------------------------------------------------- */
/* View                                                                      */
/* ------------------------------------------------------------------------- */

class StagingView : public QListView
{
    Q_OBJECT

public:
    explicit StagingView(StagingModel *model, QWidget *parent = nullptr)
        : QListView(parent)
        , m_delegate(new StagingDelegate(this))
    {
        setModel(model);
        setItemDelegate(m_delegate);
        setSelectionMode(QAbstractItemView::NoSelection);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setUniformItemSizes(true);
        setSpacing(0);
        setMouseTracking(true);
        viewport()->setMouseTracking(true);
        viewport()->setAutoFillBackground(false);
        setStyleSheet(QStringLiteral("QListView { background: transparent; }"));
        setAcceptDrops(true);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QListView::paintEvent(event);
        if (model() && model()->rowCount() == 0) {
            QPainter painter(viewport());
            painter.setRenderHint(QPainter::Antialiasing, true);
            QFont font = this->font();
            font.setPixelSize(12);
            painter.setFont(font);
            painter.setPen(QColor(255, 255, 255, 105));
            painter.drawText(viewport()->rect().adjusted(12, 12, -12, -12),
                             Qt::AlignCenter | Qt::TextWordWrap,
                             tr("把文件、文字或图片拖到这里"));
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressPos = event->position().toPoint();
        m_pressIndex = indexAt(m_pressPos);
        if (m_pressIndex.isValid()) {
            const QRect row = visualRect(m_pressIndex);
            if (checkRect(row).contains(m_pressPos) || closeRect(row).contains(m_pressPos)) {
                m_pressIndex = QModelIndex();
            }
        }
        QListView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const QPoint pos = event->position().toPoint();
        if ((event->buttons() & Qt::LeftButton) && m_pressIndex.isValid()) {
            if ((pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
                const QModelIndex index = m_pressIndex;
                m_pressIndex = QModelIndex();
                startDragForIndex(index);
                return;
            }
        }
        updateHover(pos);
        QListView::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        updateHover(QPoint(-1, -1));
        QListView::leaveEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (mimeSupported(event->mimeData())) {
            event->acceptProposedAction();
            return;
        }
        event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (mimeSupported(event->mimeData())) {
            event->acceptProposedAction();
            updateHover(event->position().toPoint());
            return;
        }
        event->ignore();
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!mimeSupported(event->mimeData())) {
            event->ignore();
            return;
        }

        auto *staging = qobject_cast<StagingModel *>(model());
        if (staging) {
            staging->addMimeData(event->mimeData());
        }
        event->acceptProposedAction();
    }

private:
    void updateHover(const QPoint &pos)
    {
        const int row = pos.x() < 0 ? -1 : indexAt(pos).row();
        if (row == m_hoverRow) {
            return;
        }
        m_hoverRow = row;
        m_delegate->setHoverRow(row);
        viewport()->update();
    }

    void startDragForIndex(const QModelIndex &index)
    {
        if (!index.isValid()) {
            return;
        }

        const int type = index.data(StagingModel::TypeRole).toInt();
        const QString uri = index.data(StagingModel::UriRole).toString();
        const QString text = index.data(StagingModel::TextRole).toString();

        auto *mime = new QMimeData;
        if (type == StagingItem::File || type == StagingItem::Image) {
            if (!uri.isEmpty()) {
                mime->setUrls({QUrl::fromLocalFile(uri)});
                mime->setText(uri);
            }
            if (type == StagingItem::Image) {
                const QImage image(uri);
                if (!image.isNull()) {
                    mime->setImageData(image);
                }
            }
        } else {
            mime->setText(text);
        }

        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        const QIcon icon = index.data(StagingModel::IconRole).value<QIcon>();
        if (!icon.isNull()) {
            drag->setPixmap(icon.pixmap(48, 48));
        }
        drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    }

    StagingDelegate *m_delegate = nullptr;
    QPoint m_pressPos;
    QModelIndex m_pressIndex;
    int m_hoverRow = -1;
};

/* ------------------------------------------------------------------------- */
/* Portal zone                                                               */
/* ------------------------------------------------------------------------- */

class PortalZone : public QWidget
{
    Q_OBJECT

public:
    explicit PortalZone(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAcceptDrops(true);
        setMinimumHeight(190);

        m_chatButton = new QPushButton(tr("问问小埃 →"), this);
        m_chatButton->setCursor(Qt::PointingHandCursor);
        m_chatButton->setFlat(true);
        m_chatButton->setFocusPolicy(Qt::NoFocus);
        m_chatButton->setStyleSheet(QStringLiteral(
            "QPushButton { color: #9ecbff; background: transparent; border: none;"
            " font-size: 13px; }"
            "QPushButton:hover { color: #cfe5ff; }"));
        connect(m_chatButton, &QPushButton::clicked, this, &PortalZone::chatRequested);

        m_hintLabel = new QLabel(this);
        m_hintLabel->setAlignment(Qt::AlignCenter);
        m_hintLabel->setWordWrap(true);
        m_hintLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_hintLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.62);"
                                                  "font-size: 11px;"));
        m_hintLabel->hide();

        m_hintTimer.setSingleShot(true);
        m_hintTimer.setInterval(2600);
        connect(&m_hintTimer, &QTimer::timeout, m_hintLabel, &QLabel::hide);
    }

signals:
    void chatRequested();

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        m_chatButton->setGeometry(12, height() - 42, width() - 24, 30);
        m_hintLabel->setGeometry(12, height() - 84, width() - 24, 36);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF box = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        QPainterPath path;
        path.addRoundedRect(box, 14, 14);

        QColor fill = QColor(255, 255, 255, 10);
        if (m_active) {
            fill = QColor(74, 144, 255, 62);
        } else if (m_hover) {
            fill = QColor(255, 255, 255, 20);
        }
        painter.fillPath(path, fill);

        const QColor border = m_active ? QColor(120, 180, 255, 230)
                                       : QColor(255, 255, 255, 58);
        painter.setPen(QPen(border, 1.5, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        QFont titleFont = font();
        titleFont.setPixelSize(22);
        titleFont.setWeight(QFont::DemiBold);
        painter.setFont(titleFont);
        painter.setPen(QColor(255, 255, 255, 242));
        painter.drawText(QRect(0, height() / 3 - 18, width(), 32), Qt::AlignHCenter,
                         tr("传送门"));

        QFont subFont = font();
        subFont.setPixelSize(12);
        painter.setFont(subFont);
        painter.setPen(QColor(255, 255, 255, 150));
        painter.drawText(QRect(12, height() / 3 + 20, width() - 24, 40),
                         Qt::AlignHCenter | Qt::TextWordWrap,
                         tr("拖到这里\n选择窗口跃迁"));
    }

    void enterEvent(QEnterEvent *event) override
    {
        Q_UNUSED(event)
        m_hover = true;
        update();
    }

    void leaveEvent(QEvent *event) override
    {
        QWidget::leaveEvent(event);
        m_hover = false;
        m_active = false;
        update();
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (!mimeSupported(event->mimeData())) {
            event->ignore();
            return;
        }
        m_active = true;
        update();
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
        m_active = false;
        update();
    }

    void dropEvent(QDropEvent *event) override
    {
        m_active = false;
        update();
        event->acceptProposedAction();
        showHint(tr("传送门将在接入合成器后可用"));
    }

private:
    void showHint(const QString &hint)
    {
        m_hintLabel->setText(hint);
        m_hintLabel->show();
        m_hintTimer.start();
    }

    QPushButton *m_chatButton = nullptr;
    QLabel *m_hintLabel = nullptr;
    QTimer m_hintTimer;
    bool m_active = false;
    bool m_hover = false;
};

/* ------------------------------------------------------------------------- */
/* Panel                                                                     */
/* ------------------------------------------------------------------------- */

PortalPanel::PortalPanel(StagingModel *model, QWidget *parent)
    : Dtk::Widget::DBlurEffectWidget(parent)
    , m_model(model)
{
    setWindowTitle(tr("GXDE 小埃同学"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMaskColor(QColor(24, 24, 28, 190));
    setBlurRectXRadius(16);
    setBlurRectYRadius(16);
    setFixedWidth(kPanelWidth);

    setupWindow();
    setupUi();

    connect(m_model, &StagingModel::countChanged, this, [this]() {
        if (m_countLabel) {
            m_countLabel->setText(tr("%1 项").arg(m_model->count()));
        }
        refreshLayout();
    });
    connect(m_model, &StagingModel::countChanged, m_view->viewport(),
            qOverload<>(&QWidget::update));
    connect(m_model, &StagingModel::dataChanged, this, [this]() { updateSelectionBar(); });
    connect(m_model, &StagingModel::rowsRemoved, this, [this]() { updateSelectionBar(); });

    connect(m_selectAllButton, &QPushButton::clicked, this, [this]() {
        for (int row = 0; row < m_model->count(); ++row) {
            m_model->setChecked(row, true);
        }
    });
    connect(m_invertButton, &QPushButton::clicked, this, [this]() {
        for (int row = 0; row < m_model->count(); ++row) {
            const StagingItem *item = m_model->itemAt(row);
            if (item) {
                m_model->setChecked(row, !item->checked);
            }
        }
    });
    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        QList<int> rows;
        for (int row = 0; row < m_model->count(); ++row) {
            const StagingItem *item = m_model->itemAt(row);
            if (item && item->checked) {
                rows.append(row);
            }
        }
        for (int i = rows.size() - 1; i >= 0; --i) {
            m_model->removeRows(rows.at(i), 1);
        }
    });
}

PortalPanel::~PortalPanel() = default;

void PortalPanel::setupWindow()
{
#ifdef GXDE_HAVE_LAYERSHELLQT
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        createWinId();
        if (auto *layer = LayerShellQt::Window::get(windowHandle())) {
            LayerShellQt::Window::Anchors anchors;
            anchors |= LayerShellQt::Window::AnchorRight;
            anchors |= LayerShellQt::Window::AnchorBottom;
            layer->setAnchors(anchors);
            layer->setMargins(QMargins(0, 0, 16, 16));
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setKeyboardInteractivity(
                LayerShellQt::Window::KeyboardInteractivityNone);
            layer->setExclusiveZone(0);
            layer->setScope(QStringLiteral("gxde-ai-see"));
            layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
        }
    }
#endif
}

void PortalPanel::setupUi()
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    outer->setSpacing(10);

    m_zone = new PortalZone(this);
    m_zone->setFixedWidth(kZoneWidth);
    outer->addWidget(m_zone);
    connect(m_zone, &PortalZone::chatRequested, this, &PortalPanel::openChatRequested);

    auto *right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(6);

    m_header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    headerLayout->setSpacing(8);

    auto *title = new QLabel(tr("暂存区"), m_header);
    QFont titleFont = title->font();
    titleFont.setPixelSize(13);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.88);"));
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    m_countLabel = new QLabel(tr("%1 项").arg(m_model->count()), m_header);
    m_countLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.45); font-size: 11px;"));
    headerLayout->addWidget(m_countLabel);

    right->addWidget(m_header);

    m_selectionBar = new QWidget(this);
    auto *selectionLayout = new QHBoxLayout(m_selectionBar);
    selectionLayout->setContentsMargins(12, 0, 12, 0);
    selectionLayout->setSpacing(6);

    auto makeButton = [this, selectionLayout](const QString &text) {
        auto *button = new QPushButton(text, m_selectionBar);
        button->setFlat(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral(
            "QPushButton { color: rgba(255,255,255,0.78); background: rgba(255,255,255,0.08);"
            " border: none; border-radius: 6px; padding: 4px 10px; font-size: 12px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.16); }"));
        selectionLayout->addWidget(button);
        return button;
    };

    m_selectAllButton = makeButton(tr("全选"));
    m_invertButton = makeButton(tr("反选"));
    m_deleteButton = makeButton(tr("删除选中"));
    selectionLayout->addStretch();
    m_selectionBar->hide();
    right->addWidget(m_selectionBar);

    m_view = new StagingView(m_model, this);
    right->addWidget(m_view, 1);

    outer->addLayout(right, 1);

    m_factory = new QWidget(this);
    m_factory->hide();
}

void PortalPanel::refreshLayout()
{
    updateGeometryForContent();
}

void PortalPanel::updateGeometryForContent()
{
    QScreen *scr = screen();
    if (!scr) {
        scr = QGuiApplication::primaryScreen();
    }
    const int screenHeight = scr ? scr->availableGeometry().height() : 900;
    const int maxListHeight = qMax(120, screenHeight / 3);

    const int contentHeight = m_model->count() * kRowHeight + 8;
    const int listHeight = qBound(140, contentHeight, maxListHeight);
    m_view->setFixedHeight(listHeight);

    const int headerHeight = 26;
    const int selectionHeight = (m_selectionBar && m_selectionBar->isVisible()) ? 32 : 0;
    const int totalHeight = kMargin * 2 + headerHeight + 6 + selectionHeight + listHeight;
    resize(kPanelWidth, totalHeight);
}

void PortalPanel::updateSelectionBar()
{
    if (!m_selectionBar) {
        return;
    }

    bool anyChecked = false;
    for (int row = 0; row < m_model->count(); ++row) {
        const StagingItem *item = m_model->itemAt(row);
        if (item && item->checked) {
            anyChecked = true;
            break;
        }
    }

    if (m_selectionBar->isVisible() == anyChecked) {
        return;
    }

    m_selectionBar->setVisible(anyChecked);
    updateGeometryForContent();
}

void PortalPanel::popup()
{
    refreshLayout();
    show();
    raise();
}

void PortalPanel::dismiss()
{
    hide();
}

void PortalPanel::showEvent(QShowEvent *event)
{
    Dtk::Widget::DBlurEffectWidget::showEvent(event);
    refreshLayout();

    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        QScreen *scr = screen();
        if (!scr) {
            scr = QGuiApplication::primaryScreen();
        }
        if (scr) {
            const QRect available = scr->availableGeometry();
            move(available.right() - width() - 16, available.bottom() - height() - 16);
        }
    }
}

#include "portalpanel.moc"
