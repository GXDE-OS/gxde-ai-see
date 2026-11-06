// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMimeData>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "factory/contentclassifier.h"
#include "portal/portalsession.h"
#include "staging/stagingmodel.h"

class PointerTravel;
class QQmlApplicationEngine;
class QQuickWindow;
class QTimer;

/**
 * The floating staging panel (传送门 + 暂存区 + 内容工厂), drawn with
 * Qt Quick and the GXTK7 style (qml/PortalPanel.qml).  This object is the
 * controller of the QML side.
 *
 * On Wayland the window is a layer-shell overlay surface so that it does not
 * appear in the dock, does not steal keyboard focus and can be positioned
 * exactly.  On X11 it is a frameless, always-on-top tool window.
 */
class PortalPanel : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PortalController)
    QML_UNCREATABLE("created by the application")

    Q_PROPERTY(StagingModel *model READ model CONSTANT)
    Q_PROPERTY(PortalSession *session READ session CONSTANT)
    Q_PROPERTY(bool stagingOnly READ stagingOnly NOTIFY stagingOnlyChanged)
    Q_PROPERTY(int listHeight READ listHeight NOTIFY listHeightChanged)
    Q_PROPERTY(int screenHeight READ screenHeight NOTIFY listHeightChanged)
    Q_PROPERTY(bool portalOpen READ portalOpen NOTIFY portalOpenChanged)
    Q_PROPERTY(bool portalSuppressed READ portalSuppressed NOTIFY portalSuppressedChanged)
    Q_PROPERTY(bool hasContent READ hasContent NOTIFY contentChanged)
    Q_PROPERTY(QString contentSummary READ contentSummary NOTIFY contentChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY contentChanged)
    Q_PROPERTY(bool anyChecked READ anyChecked NOTIFY checkedChanged)
    Q_PROPERTY(bool fixedPosition READ fixedPosition WRITE setFixedPosition NOTIFY settingsChanged)
    Q_PROPERTY(bool fixedSize READ fixedSize WRITE setFixedSize NOTIFY settingsChanged)
    Q_PROPERTY(bool hideOnFullscreen READ hideOnFullscreen WRITE setHideOnFullscreen
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY settingsChanged)

public:
    explicit PortalPanel(StagingModel *model, QObject *parent = nullptr);
    ~PortalPanel() override;

    bool isVisible() const;
    void refreshLayout();
    void popup();
    void suppressPortalIfUnderCursor();
    void setDragging(bool dragging);
    void stopPortalSuppression();
    void dismiss();

    StagingModel *model() const { return m_model; }
    PortalSession *session() const { return m_session; }
    bool stagingOnly() const { return m_stagingOnly; }
    int listHeight() const { return m_listHeight; }
    int screenHeight() const;
    bool portalOpen() const;
    bool portalSuppressed() const { return m_portalSuppressed; }
    bool hasContent() const { return m_content.valid(); }
    QString contentSummary() const;
    QVariantList actions() const;
    bool anyChecked() const;
    bool fixedPosition() const { return m_fixedPosition; }
    void setFixedPosition(bool fixed);
    bool fixedSize() const { return m_fixedSize; }
    void setFixedSize(bool fixed);
    bool hideOnFullscreen() const;
    void setHideOnFullscreen(bool hide);
    bool autostart() const;
    void setAutostart(bool enable);

    /* drag & drop, called from QML with MimeDropArea's mime object */
    Q_INVOKABLE void noteDragContent(QObject *mime);
    Q_INVOKABLE void noteDragFinished();
    Q_INVOKABLE void dropOnStaging(QObject *mime, int row);
    Q_INVOKABLE void dropOnAction(const QString &id, QObject *mime);
    Q_INVOKABLE void dropOnAsk(QObject *mime);
    Q_INVOKABLE bool isOwnDrag(QObject *mime) const;
    Q_INVOKABLE void startItemDrag(int row);

    /* the portal zone and the window grid */
    Q_INVOKABLE void portalEntered();
    Q_INVOKABLE void portalLeft();
    Q_INVOKABLE void portalDropped();
    Q_INVOKABLE void jumpTo(const QString &windowId);

    /* staging list */
    Q_INVOKABLE void toggleChecked(int row);
    Q_INVOKABLE void togglePinned(int row);
    Q_INVOKABLE void removeItem(int row);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void invertSelection();
    Q_INVOKABLE void deleteChecked();

    /* buttons, menu, window */
    Q_INVOKABLE void runAction(const QString &id);
    Q_INVOKABLE void openChat();
    Q_INVOKABLE void resetPosition();
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE void requestHide();
    Q_INVOKABLE void showAbout();
    Q_INVOKABLE void quit();
    /* pointer positions are in window coordinates */
    Q_INVOKABLE void beginMove(qreal windowX, qreal windowY);
    Q_INVOKABLE void beginResize(qreal windowX, qreal windowY);
    Q_INVOKABLE void updateMoveResize(qreal windowX, qreal windowY);
    Q_INVOKABLE void endMoveResize();
    Q_INVOKABLE void setBlurAreas(const QVariantList &areas, int radius);
    Q_INVOKABLE void resizeWindow(int width, int height);

signals:
    void openChatRequested();
    void askWithContent(const QString &text, const QStringList &files);
    void hideRequested();
    void todoAdded();

    void stagingOnlyChanged();
    void listHeightChanged();
    void portalOpenChanged();
    void portalSuppressedChanged();
    void contentChanged();
    void checkedChanged();
    void settingsChanged();
    /** A short message under the portal title. */
    void zoneMessage(const QString &text);
    void toast(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupWindow();
    void updateMode();
    void updateBlur();
    void setContent(const DropContent &content);
    void runFactoryAction(const QString &id, const DropContent &content);
    void showTextDialog(const QString &title, const QString &text);
    void openPortal();
    void setPortalSuppressed(bool suppressed);
    void liftPortalSuppression();
    void setCompositorPortalActive(bool active);
    void applyMargins();
    void followPointer(const QPointF &travel);
    QRect cornerGeometry(const QSize &size) const;
    void saveWindowState();

    StagingModel *m_model = nullptr;
    PortalSession *m_session = nullptr;
    QQmlApplicationEngine *m_engine = nullptr;
    QQuickWindow *m_window = nullptr;

    DropContent m_content;
    QVariantList m_actions;
    /* the drag the factory content was classified from */
    QPointer<QMimeData> m_classifiedMime;
    QStringList m_classifiedFormats;
    QTimer *m_factoryTimer = nullptr;
    QTimer *m_portalTimer = nullptr;
    QTimer *m_leaveTimer = nullptr;
    QTimer *m_suppressWatch = nullptr;
    QSize m_wantedSize;
    QList<QRect> m_blurAreas;
    int m_blurRadius = 0;

    int m_rightMargin = 16;
    int m_bottomMargin = 16;
    int m_manualListHeight = 0;
    int m_listHeight = 140;
    bool m_stagingOnly = false;
    bool m_dragging = false;
    bool m_portalSuppressed = false;
    bool m_ownDrag = false;
    bool m_compositorPortalActive = false;
    bool m_fixedPosition = true;
    bool m_fixedSize = false;
    int m_dragMode = 0; // 0 none, 1 move, 2 resize
    QPoint m_dragStart;
    int m_dragStartRight = 0;
    int m_dragStartBottom = 0;
    int m_resizeStartHeight = 0;
    /* Wayland: the pointer's motion, which surface positions can not give */
    PointerTravel *m_travel = nullptr;
    bool m_followTravel = false;
    QElapsedTimer m_dragClock;
    /* Wayland without relative pointer: where the pointer holds the window,
     * and the step in flight */
    QPointF m_grabPoint;
    bool m_stepPending = false;
    QElapsedTimer m_stepClock;
};
