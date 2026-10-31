// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalpanel.h"

#include "config.h"
#include "portal/portalclient.h"

#include "actions/downloader.h"
#include "actions/texttools.h"
#include "factory/factoryactions.h"

#include "aboutdialog.h"
#include "pointertravel.h"
#include "dialogs/segmentdialog.h"
#include "dialogs/speechdialog.h"
#include "todo/todomodel.h"

#include "staging/stagingitem.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QDrag>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <bernese/window_effects.h>

#ifdef GXDE_HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

namespace {

constexpr int kRowHeight = 56;
constexpr char kStagingMime[] = "application/x-gxde-ai-see-staging";

bool isWayland()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

QString slimyPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/GXDE/gxde-k9/slimy/gxde-ai-see.slimy");
}

} // namespace

PortalPanel::PortalPanel(StagingModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_session(new PortalSession(this))
{
    Config *config = Config::instance();
    m_rightMargin = config->getInt(QStringLiteral("panel/rightMargin"), 16);
    m_bottomMargin = config->getInt(QStringLiteral("panel/bottomMargin"), 16);
    m_manualListHeight = config->getInt(QStringLiteral("panel/listHeight"), 0);
    m_fixedPosition = config->getBool(QStringLiteral("panel/fixedPosition"), true);
    m_fixedSize = config->getBool(QStringLiteral("panel/fixedSize"), false);

    /* hover the portal this long before the windows get out of the way */
    m_portalTimer = new QTimer(this);
    m_portalTimer->setSingleShot(true);
    m_portalTimer->setInterval(80);
    connect(m_portalTimer, &QTimer::timeout, this, &PortalPanel::openPortal);

    /* the drag may cross from the portal into the grid: only restore the
     * windows when it did not come back quickly */
    m_leaveTimer = new QTimer(this);
    m_leaveTimer->setSingleShot(true);
    m_leaveTimer->setInterval(180);
    connect(m_leaveTimer, &QTimer::timeout, this, [this]() {
        m_portalTimer->stop();
        m_session->cancel();
    });

    m_factoryTimer = new QTimer(this);
    m_factoryTimer->setSingleShot(true);
    m_factoryTimer->setInterval(220);
    connect(m_factoryTimer, &QTimer::timeout, this, [this]() {
        /* moving from the portal zone into the factory must not clear it:
         * only once the pointer really left the panel */
        if (m_window && m_window->geometry().contains(QCursor::pos())) {
            m_factoryTimer->start();
            return;
        }
        setContent(DropContent());
    });

    m_suppressWatch = new QTimer(this);
    m_suppressWatch->setInterval(80);
    connect(m_suppressWatch, &QTimer::timeout, this, [this]() {
        if (!m_portalSuppressed) {
            m_suppressWatch->stop();
            return;
        }
        if (m_window && m_window->geometry().contains(QCursor::pos())) {
            return;
        }
        m_suppressWatch->stop();
        setPortalSuppressed(false);
        /* the compositor overview follows the pointer anywhere on screen */
        if (PortalClient::instance()->available()) {
            openPortal();
        }
    });

    connect(m_model, &StagingModel::countChanged, this, [this]() {
        updateMode();
        refreshLayout();
        Q_EMIT checkedChanged();
    });
    connect(m_model, &StagingModel::dataChanged, this, &PortalPanel::checkedChanged);
    connect(m_model, &StagingModel::rowsRemoved, this, &PortalPanel::checkedChanged);

    connect(m_session, &PortalSession::activeChanged, this, &PortalPanel::portalOpenChanged);
    /* never quit with every window minimized */
    connect(qApp, &QCoreApplication::aboutToQuit, m_session, &PortalSession::cancel);
    connect(PortalClient::instance(), &PortalClient::dragEnded, this, [this]() {
        setCompositorPortalActive(false);
        m_session->cancel();
    });
    connect(PortalClient::instance(), &PortalClient::portalClosed, this,
            [this]() { setCompositorPortalActive(false); });

    m_travel = new PointerTravel(this);
    connect(m_travel, &PointerTravel::moved, this, [this](const QPointF &travel) {
        if (m_dragMode != 0) {
            m_followTravel = true;
            followPointer(travel);
        }
    });


    m_engine = new QQmlApplicationEngine(this);
    m_engine->addImageProvider(QStringLiteral("taskwindow"),
                               new TaskWindowIconProvider(m_session));
    m_engine->setInitialProperties({ { QStringLiteral("controller"),
                                       QVariant::fromValue(this) } });
    m_engine->loadFromModule("GxdeAiSee", "PortalPanel");
    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().value(0));
    if (!m_window) {
        qWarning("gxde-ai-see: failed to load the portal panel");
        return;
    }

    setupWindow();
    refreshLayout();
}

PortalPanel::~PortalPanel()
{
    m_session->cancel();
}

void PortalPanel::setupWindow()
{
    m_window->setTitle(tr("GXDE 小埃同学"));
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus;
    if (!isWayland()) {
        /* everything else gets minimized under it during a portal jump */
        flags |= Qt::WindowStaysOnTopHint;
    }
    m_window->setFlags(flags);
    m_window->setColor(Qt::transparent);

#ifdef GXDE_HAVE_LAYERSHELLQT
    if (isWayland()) {
        if (auto *layer = LayerShellQt::Window::get(m_window)) {
            LayerShellQt::Window::Anchors anchors;
            anchors |= LayerShellQt::Window::AnchorRight;
            anchors |= LayerShellQt::Window::AnchorBottom;
            layer->setAnchors(anchors);
            layer->setMargins(QMargins(0, 0, m_rightMargin, m_bottomMargin));
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            layer->setExclusiveZone(0);
            layer->setScope(QStringLiteral("gxde-ai-see"));
            layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
        }
    }
#endif

    if (m_wantedSize.isValid()) {
        m_window->resize(m_wantedSize);
    }
    /* a hand move step is on screen once its frame went out */
    connect(
        m_window, &QQuickWindow::frameSwapped, this, [this]() { m_stepPending = false; },
        Qt::QueuedConnection);
    connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (visible) {
            applyMargins();
            updateBlur();
        } else {
            m_session->cancel();
        }
    });
}

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */

bool PortalPanel::isVisible() const
{
    return m_window && m_window->isVisible();
}

int PortalPanel::screenHeight() const
{
    QScreen *scr = m_window && m_window->screen() ? m_window->screen()
                                                  : QGuiApplication::primaryScreen();
    return scr ? scr->availableGeometry().height() : 900;
}

bool PortalPanel::portalOpen() const
{
    return m_compositorPortalActive || m_session->active();
}

QString PortalPanel::contentSummary() const
{
    return m_content.valid() ? m_content.summary() : QString();
}

QVariantList PortalPanel::actions() const
{
    return m_actions;
}

bool PortalPanel::anyChecked() const
{
    for (int row = 0; row < m_model->count(); ++row) {
        const StagingItem *item = m_model->itemAt(row);
        if (item && item->checked) {
            return true;
        }
    }
    return false;
}

void PortalPanel::refreshLayout()
{
    const int maxListHeight = qMax(120, screenHeight() / 3);
    const int contentHeight = m_model->count() * kRowHeight + 8;
    const int wanted = m_manualListHeight > 0 ? m_manualListHeight : contentHeight;
    const int listHeight = qBound(140, wanted, maxListHeight);
    if (listHeight != m_listHeight) {
        m_listHeight = listHeight;
        Q_EMIT listHeightChanged();
    }
}

void PortalPanel::setDragging(bool dragging)
{
    if (m_dragging == dragging) {
        return;
    }
    m_dragging = dragging;
    updateMode();
}

void PortalPanel::updateMode()
{
    /* Only GXDE's compositor reports drags; elsewhere a drag counts once it
     * reaches the panel, which is when the factory gets content. */
    const bool dragging = m_dragging || m_content.valid();
    const bool stagingOnly = !dragging && m_model && m_model->count() > 0;
    if (stagingOnly == m_stagingOnly) {
        return;
    }
    m_stagingOnly = stagingOnly;
    Q_EMIT stagingOnlyChanged();
    refreshLayout();
}

void PortalPanel::setContent(const DropContent &content)
{
    m_content = content;
    if (!content.valid()) {
        m_classifiedMime.clear();
    }

    /* actionsFor() may read the dropped file: once per payload */
    m_actions.clear();
    for (const FactoryAction &action : actionsFor(m_content)) {
        m_actions.append(QVariantMap{
            { QStringLiteral("id"), action.id },
            { QStringLiteral("label"), action.label },
            { QStringLiteral("icon"), action.icon },
            { QStringLiteral("hint"), action.hint },
            { QStringLiteral("enabled"), action.enabled },
        });
    }
    Q_EMIT contentChanged();
    updateMode();
}

/* ------------------------------------------------------------------------- */
/* Window                                                                    */
/* ------------------------------------------------------------------------- */

void PortalPanel::popup()
{
    if (!m_window) {
        return;
    }
    refreshLayout();
    m_window->show();
    m_window->raise();
}

void PortalPanel::dismiss()
{
    if (m_window) {
        m_window->hide();
    }
}

void PortalPanel::applyMargins()
{
    if (!m_window) {
        return;
    }
#ifdef GXDE_HAVE_LAYERSHELLQT
    if (isWayland()) {
        if (auto *layer = LayerShellQt::Window::get(m_window)) {
            layer->setMargins(QMargins(0, 0, m_rightMargin, m_bottomMargin));
            /* set_margin only takes effect with the next surface commit:
             * render a frame now, or the panel would lag behind the pointer */
            m_window->update();
            return;
        }
    }
#endif
    m_window->setPosition(cornerGeometry(m_window->size()).topLeft());
}

QRect PortalPanel::cornerGeometry(const QSize &size) const
{
    QScreen *scr = m_window->screen() ? m_window->screen() : QGuiApplication::primaryScreen();
    if (!scr) {
        return QRect(m_window->position(), size);
    }
    const QRect available = scr->availableGeometry();
    return QRect(available.right() + 1 - size.width() - m_rightMargin,
                 available.bottom() + 1 - size.height() - m_bottomMargin, size.width(),
                 size.height());
}

void PortalPanel::resizeWindow(int width, int height)
{
    m_wantedSize = QSize(width, height);
    if (!m_window || m_window->size() == m_wantedSize) {
        return;
    }
    /* The window grows to the left when the window grid opens.  On X11 move
     * and resize in one request, or the portal would jump away from under
     * the drag for a frame; layer-shell keeps the corner by itself. */
    if (isWayland()) {
        m_window->resize(m_wantedSize);
    } else {
        m_window->setGeometry(cornerGeometry(m_wantedSize));
    }
}

void PortalPanel::setBlurAreas(const QVariantList &areas, int radius)
{
    QList<QRect> rects;
    for (const QVariant &area : areas) {
        const QRect rect = area.toRectF().toAlignedRect();
        if (!rect.isEmpty()) {
            rects.append(rect);
        }
    }
    if (rects == m_blurAreas && radius == m_blurRadius) {
        return;
    }
    m_blurAreas = rects;
    m_blurRadius = radius;
    updateBlur();
}

void PortalPanel::updateBlur()
{
    if (!m_window || !m_window->isVisible() || m_blurAreas.isEmpty()) {
        return;
    }
    if (bernese::window_effects::IsBlurBehindSupported()) {
        bernese::window_effects::SetBlurBehind(m_window, m_blurAreas, m_blurRadius);
    }
}

void PortalPanel::saveWindowState()
{
    Config *config = Config::instance();
    config->set(QStringLiteral("panel/rightMargin"), m_rightMargin);
    config->set(QStringLiteral("panel/bottomMargin"), m_bottomMargin);
    config->set(QStringLiteral("panel/listHeight"), m_manualListHeight);
    config->set(QStringLiteral("panel/fixedPosition"), m_fixedPosition);
    config->set(QStringLiteral("panel/fixedSize"), m_fixedSize);
    config->sync();
}

/*
 * Moving and resizing by hand.
 *
 * The panel follows how far the pointer travelled since the press.
 *
 * X11: QCursor::pos() asks the server, so that travel is exact.
 *
 * Wayland: nobody tells a client where the pointer is on screen, and the
 * surface-local positions move along with the panel.  zwp_relative_pointer_v1
 * reports the motion itself, so the panel follows it on every event, and the
 * compositor applies the new margin with the very next frame.
 *
 * Wayland without relative motion: follow the pointer step by step instead:
 * move the panel by how far the pointer drifted from the spot it grabbed,
 * then wait for that frame before taking the next step.  A step taken too
 * early is undone by the next one, it can not run away.
 */
void PortalPanel::beginMove(qreal windowX, qreal windowY)
{
    if (m_fixedPosition || !m_window) {
        return;
    }
    m_dragMode = 1;
    m_dragStart = QCursor::pos();
    m_dragStartRight = m_rightMargin;
    m_dragStartBottom = m_bottomMargin;
    m_grabPoint = QPointF(windowX, windowY);
    m_stepPending = false;
    m_followTravel = false;
    m_dragClock.start();
    if (isWayland()) {
        m_travel->start();
    }
}

void PortalPanel::beginResize(qreal windowX, qreal windowY)
{
    if (m_fixedSize || !m_window) {
        return;
    }
    m_dragMode = 2;
    m_dragStart = QCursor::pos();
    m_resizeStartHeight = m_listHeight;
    m_grabPoint = QPointF(windowX, windowY);
    m_stepPending = false;
    m_followTravel = false;
    m_dragClock.start();
    if (isWayland()) {
        m_travel->start();
    }
}

void PortalPanel::followPointer(const QPointF &travel)
{
    const QPoint delta = travel.toPoint();
    if (m_dragMode == 2) {
        /* anchored at the bottom: the top edge follows the pointer */
        const int height = qMax(120, m_resizeStartHeight - delta.y());
        if (height != m_manualListHeight) {
            m_manualListHeight = height;
            refreshLayout();
        }
        return;
    }

    /* keep the panel on screen; the relative pointer keeps reporting motion
     * at the screen edge, so let the grab slide instead of piling it up */
    const QScreen *scr = m_window->screen() ? m_window->screen() : QGuiApplication::primaryScreen();
    const QSize room = scr ? scr->availableGeometry().size() - m_window->size() : QSize();
    const int maxRight = qMax(0, room.width());
    const int maxBottom = qMax(0, room.height());

    const int wantRight = m_dragStartRight - delta.x();
    const int wantBottom = m_dragStartBottom - delta.y();
    const int right = qBound(0, wantRight, maxRight);
    const int bottom = qBound(0, wantBottom, maxBottom);
    m_dragStartRight += right - wantRight;
    m_dragStartBottom += bottom - wantBottom;

    if (right == m_rightMargin && bottom == m_bottomMargin) {
        return;
    }
    m_rightMargin = right;
    m_bottomMargin = bottom;
    applyMargins();
}

void PortalPanel::updateMoveResize(qreal windowX, qreal windowY)
{
    if (m_dragMode == 0) {
        return;
    }

    if (!isWayland()) {
        followPointer(QCursor::pos() - m_dragStart);
        return;
    }

    /* the relative pointer drives it (see the constructor); right after
     * binding it give the compositor a moment before falling back */
    if (m_travel->running() && (m_followTravel || m_dragClock.elapsed() < 150)) {
        return;
    }

    /* the last step has not reached the screen yet: its events still use the
     * old position (give up waiting if no frame comes) */
    if (m_stepPending && m_stepClock.elapsed() < 80) {
        return;
    }
    m_stepPending = false;

    const QPoint step = (QPointF(windowX, windowY) - m_grabPoint).toPoint();
    if (step.isNull()) {
        return;
    }
    if (m_dragMode == 1) {
        m_rightMargin = qMax(0, m_rightMargin - step.x());
        m_bottomMargin = qMax(0, m_bottomMargin - step.y());
        applyMargins();
    } else {
        /* anchored at the bottom: the top edge follows the pointer */
        m_manualListHeight = qMax(120, m_listHeight - step.y());
        refreshLayout();
    }
    m_stepPending = true;
    m_stepClock.start();
}

void PortalPanel::endMoveResize()
{
    m_travel->stop();
    m_followTravel = false;
    if (m_dragMode != 0) {
        m_dragMode = 0;
        saveWindowState();
    }
}

/* ------------------------------------------------------------------------- */
/* Portal                                                                    */
/* ------------------------------------------------------------------------- */

void PortalPanel::setPortalSuppressed(bool suppressed)
{
    if (suppressed) {
        m_portalTimer->stop();
    }
    if (m_portalSuppressed != suppressed) {
        m_portalSuppressed = suppressed;
        Q_EMIT portalSuppressedChanged();
    }
}

void PortalPanel::setCompositorPortalActive(bool active)
{
    if (!active) {
        m_portalTimer->stop();
    }
    if (m_compositorPortalActive != active) {
        m_compositorPortalActive = active;
        Q_EMIT portalOpenChanged();
    }
}

void PortalPanel::suppressPortalIfUnderCursor()
{
    if (!isVisible()) {
        return;
    }
    /* the drag started on top of the panel: do not jump until it left */
    const bool under = m_window->geometry().contains(QCursor::pos());
    setPortalSuppressed(under);
    if (under) {
        m_suppressWatch->start();
    } else {
        m_suppressWatch->stop();
    }
}

void PortalPanel::stopPortalSuppression()
{
    m_suppressWatch->stop();
    setPortalSuppressed(false);
}

void PortalPanel::portalEntered()
{
    m_leaveTimer->stop();
    if (!portalOpen() && !m_portalSuppressed && !m_portalTimer->isActive()) {
        m_portalTimer->start();
    }
}

void PortalPanel::portalLeft()
{
    m_portalTimer->stop();
    if (m_session->active()) {
        m_leaveTimer->start();
    }
}

void PortalPanel::portalDropped()
{
    m_portalTimer->stop();
    m_leaveTimer->stop();
    m_session->cancel();
    m_classifiedMime.clear();
    if (!portalOpen()) {
        Q_EMIT zoneMessage(tr("悬停在「传送门」上，再移动到目标窗口"));
    }
}

void PortalPanel::openPortal()
{
    if (m_portalSuppressed || portalOpen()) {
        return;
    }

    /* GXDE's own compositor flies the windows out (爆炸平铺) and forwards the
     * drop itself.  Everywhere else (X11, treeland, other wlroots
     * compositors) the windows get minimized and listed in a grid to the
     * left of the portal. */
    PortalClient *portal = PortalClient::instance();
    if (portal->available()) {
        QRect panelRect = m_window->geometry();
        QScreen *scr = m_window->screen() ? m_window->screen() : QGuiApplication::primaryScreen();
        if (scr && isWayland()) {
            const QRect avail = scr->availableGeometry();
            panelRect = QRect(avail.x() + avail.width() - m_window->width() - m_rightMargin,
                              avail.y() + avail.height() - m_window->height() - m_bottomMargin,
                              m_window->width(), m_window->height());
        }
        if (portal->beginPortal(panelRect)) {
            setCompositorPortalActive(true);
            Q_EMIT zoneMessage(tr("移动到目标窗口上松手"));
        } else {
            Q_EMIT zoneMessage(tr("当前无已打开的窗口"));
        }
        return;
    }

    if (!PortalSession::supported()) {
        Q_EMIT zoneMessage(tr("当前环境不支持传送门"));
        return;
    }
    if (m_session->begin()) {
        Q_EMIT zoneMessage(tr("拖到左侧窗口图标上，激活该窗口"));
    } else {
        Q_EMIT zoneMessage(tr("当前无已打开的窗口"));
    }
}

void PortalPanel::jumpTo(const QString &windowId)
{
    m_leaveTimer->stop();
    if (!m_session->active()) {
        return;
    }
    m_session->activate(windowId);
    Q_EMIT zoneMessage(tr("继续拖到窗口里松手"));
}

/* ------------------------------------------------------------------------- */
/* Drag and drop                                                             */
/* ------------------------------------------------------------------------- */

bool PortalPanel::isOwnDrag(QObject *mime) const
{
    auto *data = qobject_cast<QMimeData *>(mime);
    return data && data->hasFormat(QLatin1String(kStagingMime));
}

void PortalPanel::noteDragContent(QObject *mime)
{
    auto *data = qobject_cast<QMimeData *>(mime);
    if (!data || isOwnDrag(data)) {
        return;
    }
    m_factoryTimer->stop();

    /* Reading a foreign drag's data is a round trip to its owner (and an
     * image gets decoded): classify once per drag, not on every move. */
    const QStringList formats = data->formats();
    if (data == m_classifiedMime && formats == m_classifiedFormats) {
        return;
    }
    m_classifiedMime = data;
    m_classifiedFormats = formats;

    const DropContent content = ContentClassifier::classify(data);
    if (!content.valid()) {
        return;
    }
    if (m_content.valid() && m_content.summary() == content.summary()) {
        return;
    }
    setContent(content);
}

void PortalPanel::noteDragFinished()
{
    m_factoryTimer->start();
}

void PortalPanel::dropOnStaging(QObject *mime, int row)
{
    auto *data = qobject_cast<QMimeData *>(mime);
    if (!data) {
        return;
    }

    if (isOwnDrag(data)) {
        const QString id = QString::fromUtf8(data->data(QLatin1String(kStagingMime)));
        int from = -1;
        for (int i = 0; i < m_model->count(); ++i) {
            const StagingItem *item = m_model->itemAt(i);
            if (item && item->id == id) {
                from = i;
                break;
            }
        }
        const int to = row >= 0 && row < m_model->count() ? row : m_model->count() - 1;
        if (from >= 0 && to >= 0 && from != to) {
            m_model->moveItem(from, to);
        }
        /* read back by startItemDrag(): a reorder must not consume the item */
        data->setProperty("gxdeInternalMove", true);
        return;
    }

    m_model->addMimeData(data);
    m_classifiedMime.clear();
    noteDragFinished();
}

void PortalPanel::dropOnAction(const QString &id, QObject *mime)
{
    auto *data = qobject_cast<QMimeData *>(mime);
    const DropContent content = data ? ContentClassifier::classify(data) : DropContent();
    if (content.valid()) {
        runFactoryAction(id, content);
    }
}

void PortalPanel::dropOnAsk(QObject *mime)
{
    auto *data = qobject_cast<QMimeData *>(mime);
    if (!data) {
        return;
    }
    QStringList files;
    for (const QUrl &url : data->urls()) {
        if (url.isLocalFile()) {
            files.append(url.toLocalFile());
        }
    }
    Q_EMIT askWithContent(data->hasText() ? data->text() : QString(), files);
}

void PortalPanel::startItemDrag(int row)
{
    const StagingItem *item = m_model->itemAt(row);
    if (!item) {
        return;
    }

    const QString id = item->id;
    const bool pinned = item->pinned;

    auto *mime = new QMimeData;
    mime->setData(QLatin1String(kStagingMime), id.toUtf8());
    if (item->type == StagingItem::File || item->type == StagingItem::Image) {
        if (!item->uri.isEmpty()) {
            mime->setUrls({ QUrl::fromLocalFile(item->uri) });
            mime->setText(item->uri);
        }
        if (item->type == StagingItem::Image) {
            const QImage image(item->uri);
            if (!image.isNull()) {
                mime->setImageData(image);
            }
        }
    } else {
        mime->setText(item->text);
    }

    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    const QIcon icon = m_model->index(row).data(StagingModel::IconRole).value<QIcon>();
    if (!icon.isNull()) {
        drag->setPixmap(icon.pixmap(48, 48));
    }

    const Qt::DropAction action = drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    const bool internalMove = mime->property("gxdeInternalMove").toBool();
    drag->deleteLater();
    if (internalMove) {
        return;
    }

    /* Dragging an item out of the staging area consumes it unless it is
     * pinned.  A cancelled drag (Esc / dropped on nothing) keeps it. */
    if (action != Qt::IgnoreAction && !pinned) {
        for (int i = 0; i < m_model->count(); ++i) {
            const StagingItem *current = m_model->itemAt(i);
            if (current && current->id == id) {
                m_model->removeRows(i, 1);
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Staging list                                                              */
/* ------------------------------------------------------------------------- */

void PortalPanel::toggleChecked(int row)
{
    if (const StagingItem *item = m_model->itemAt(row)) {
        m_model->setChecked(row, !item->checked);
    }
}

void PortalPanel::togglePinned(int row)
{
    if (const StagingItem *item = m_model->itemAt(row)) {
        m_model->setPinned(row, !item->pinned);
    }
}

void PortalPanel::removeItem(int row)
{
    if (row >= 0 && row < m_model->count()) {
        m_model->removeRows(row, 1);
    }
}

void PortalPanel::selectAll()
{
    for (int row = 0; row < m_model->count(); ++row) {
        m_model->setChecked(row, true);
    }
}

void PortalPanel::invertSelection()
{
    for (int row = 0; row < m_model->count(); ++row) {
        if (const StagingItem *item = m_model->itemAt(row)) {
            m_model->setChecked(row, !item->checked);
        }
    }
}

void PortalPanel::deleteChecked()
{
    for (int row = m_model->count() - 1; row >= 0; --row) {
        const StagingItem *item = m_model->itemAt(row);
        if (item && item->checked) {
            m_model->removeRows(row, 1);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Menu                                                                      */
/* ------------------------------------------------------------------------- */

void PortalPanel::setFixedPosition(bool fixed)
{
    if (m_fixedPosition != fixed) {
        m_fixedPosition = fixed;
        saveWindowState();
        Q_EMIT settingsChanged();
    }
}

void PortalPanel::setFixedSize(bool fixed)
{
    if (m_fixedSize != fixed) {
        m_fixedSize = fixed;
        saveWindowState();
        Q_EMIT settingsChanged();
    }
}

bool PortalPanel::hideOnFullscreen() const
{
    return Config::instance()->getBool(QStringLiteral("panel/hideOnFullscreen"), false);
}

void PortalPanel::setHideOnFullscreen(bool hide)
{
    Config::instance()->set(QStringLiteral("panel/hideOnFullscreen"), hide);
    Config::instance()->sync();
    Q_EMIT settingsChanged();
}

bool PortalPanel::autostart() const
{
    return QFile::exists(slimyPath());
}

void PortalPanel::setAutostart(bool enable)
{
    const QString slimy = slimyPath();
    QDir().mkpath(QFileInfo(slimy).absolutePath());
    if (enable) {
        QFile file(slimy);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "#!/bin/bash\n"
                   "PID_DIR=\"${PID_DIR:-/tmp/GXDE/gxde-k9/}\"\n"
                   "mkdir -p \"$PID_DIR\"\n"
                   "PID_FILE=\"$PID_DIR/gxde-ai-see.pid\"\n"
                   "if [ -f \"$PID_FILE\" ]; then\n"
                   "  PID=$(cat \"$PID_FILE\" 2>/dev/null)\n"
                   "  [ -n \"$PID\" ] && [ -e \"/proc/$PID\" ] && exit 0\n"
                   "  rm -f \"$PID_FILE\"\n"
                   "fi\n"
                   "/usr/bin/gxde-ai-see --daemon >/dev/null 2>&1 &\n"
                   "echo $! > \"$PID_FILE\"\n";
            file.close();
            file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                QFileDevice::ExeOwner);
        }
    } else {
        QFile::remove(slimy);
    }
    Q_EMIT settingsChanged();
    Q_EMIT toast(enable ? tr("已开启开机自启与保活") : tr("已关闭开机自启与保活"));
}

void PortalPanel::resetPosition()
{
    m_rightMargin = 16;
    m_bottomMargin = 16;
    applyMargins();
    saveWindowState();
}

void PortalPanel::clearAll()
{
    m_model->clearAll();
}

void PortalPanel::requestHide()
{
    Q_EMIT hideRequested();
}

void PortalPanel::showAbout()
{
    showAboutDialog(nullptr, tr("传送门、内容工厂、待办与小埃同学对话。"));
}

void PortalPanel::quit()
{
    QCoreApplication::quit();
}

void PortalPanel::openChat()
{
    Q_EMIT openChatRequested();
}

/* ------------------------------------------------------------------------- */
/* Content factory                                                           */
/* ------------------------------------------------------------------------- */

void PortalPanel::runAction(const QString &id)
{
    if (m_content.valid()) {
        runFactoryAction(id, m_content);
    }
}

void PortalPanel::showTextDialog(const QString &title, const QString &text)
{
    QMessageBox box(QMessageBox::Information, title, text.left(4000), QMessageBox::Ok);
    QPushButton *copyButton = box.addButton(tr("复制"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == copyButton) {
        QApplication::clipboard()->setText(text);
        Q_EMIT toast(tr("已复制"));
    }
}

void PortalPanel::runFactoryAction(const QString &id, const DropContent &content)
{
    auto copyText = [this](const QString &text) {
        QApplication::clipboard()->setText(text);
        Q_EMIT toast(tr("已复制"));
    };

    const QString link = content.link.url.toString();
    const QString sourceText = content.kind == DropContent::Text
                                   ? content.text
                                   : (content.kind == DropContent::Files
                                          ? TextTools::readTextFile(content.filePaths.value(0),
                                                                    512 * 1024)
                                          : content.text);

    if (id == QLatin1String("open")) {
        if (content.kind == DropContent::Files) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(content.filePaths.value(0)));
        } else if (!link.isEmpty()) {
            QDesktopServices::openUrl(content.link.url);
        }
        return;
    }
    if (id == QLatin1String("open_browser") || id == QLatin1String("try_open") ||
        id == QLatin1String("preview") || id == QLatin1String("play")) {
        QDesktopServices::openUrl(content.link.url);
        return;
    }
    if (id == QLatin1String("download")) {
        Downloader::instance()->download(content.link.url);
        return;
    }
    if (id == QLatin1String("save_as")) {
        Downloader::instance()->saveAs(content.link.url, nullptr);
        return;
    }
    if (id == QLatin1String("copy")) {
        copyText(!content.link.raw.isEmpty() ? content.link.raw
                                             : (link.isEmpty() ? content.text : link));
        return;
    }
    if (id == QLatin1String("copy_markdown")) {
        copyText(QStringLiteral("[%1](%2)").arg(content.link.displayName, link));
        return;
    }
    if (id == QLatin1String("copy_name")) {
        copyText(content.link.displayName);
        return;
    }
    if (id == QLatin1String("copy_path")) {
        copyText(content.filePaths.value(0));
        return;
    }
    if (id == QLatin1String("copy_decoded")) {
        copyText(content.link.decodedUrl);
        return;
    }
    if (id == QLatin1String("decode")) {
        showTextDialog(tr("真实地址"), content.link.decodedUrl);
        return;
    }
    if (id == QLatin1String("segment")) {
        const QStringList words = TextTools::segment(sourceText.left(20000));
        auto *dialog = new SegmentDialog(words);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        return;
    }
    if (id == QLatin1String("extract_links")) {
        const QStringList links = TextTools::extractLinks(sourceText);
        showTextDialog(tr("提取链接（%1）").arg(links.size()), links.join(QLatin1Char('\n')));
        return;
    }
    if (id == QLatin1String("speak")) {
        auto *dialog = new SpeechDialog(sourceText.left(20000));
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        return;
    }
    if (id == QLatin1String("todo")) {
        const QString text = content.kind == DropContent::Text ? content.text.left(500)
                                                               : content.summary();
        TodoModel::instance()->add(text);
        Q_EMIT toast(tr("已添加到待办"));
        Q_EMIT todoAdded();
        return;
    }
    if (id == QLatin1String("save_image")) {
        if (content.image.isNull()) {
            Q_EMIT toast(tr("没有可保存的图片"));
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            nullptr, tr("保存图片"), QStringLiteral("image.png"), tr("图片 (*.png *.jpg)"));
        if (!path.isEmpty()) {
            content.image.save(path);
            Q_EMIT toast(tr("已保存：%1").arg(path));
        }
        return;
    }
    if (id == QLatin1String("ask")) {
        Q_EMIT openChatRequested();
        return;
    }
    if (id == QLatin1String("translate")) {
        Q_EMIT toast(tr("翻译接口将在小埃同学接入后可用"));
        return;
    }
    if (id == QLatin1String("ocr") || id == QLatin1String("describe") ||
        id == QLatin1String("transcribe")) {
        Q_EMIT toast(tr("该能力尚未接入 GXDE Daemon"));
        return;
    }

    Q_EMIT toast(tr("未知操作：%1").arg(id));
}
