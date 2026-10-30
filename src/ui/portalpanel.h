// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DBlurEffectWidget>
#include <QWidget>

class ContentFactory;
class QLabel;
class QMimeData;
class QPushButton;
class QTimer;
struct DropContent;
class PortalZone;
class StagingModel;
class StagingView;

/**
 * The floating staging panel (传送门 + 暂存区).
 *
 * On Wayland this is a layer-shell overlay surface so that it does not appear
 * in the dock, does not steal keyboard focus and can be positioned exactly.
 * On X11 it falls back to a frameless tool window placed bottom-right.
 */
class PortalPanel : public Dtk::Widget::DBlurEffectWidget
{
    Q_OBJECT

public:
    explicit PortalPanel(StagingModel *model, QWidget *parent = nullptr);
    ~PortalPanel() override;

    void refreshLayout();
    void popup();
    void dismiss();

signals:
    void openChatRequested();

protected:
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setupUi();
    void setupWindow();
    void updateGeometryForContent();
    void updateSelectionBar();
    void noteDragContent(const QMimeData *mime);
    void noteDragFinished();
    void runFactoryAction(const QString &id, const DropContent &content);
    void showTextDialog(const QString &title, const QString &text);
    void toast(const QString &text);
    void applyMargins();
    void saveWindowState();
    void showContextMenu(const QPoint &globalPos);

    StagingModel *m_model = nullptr;
    StagingView *m_view = nullptr;
    PortalZone *m_zone = nullptr;
    QWidget *m_header = nullptr;
    QWidget *m_selectionBar = nullptr;
    ContentFactory *m_factory = nullptr;
    QLabel *m_toast = nullptr;
    QTimer *m_factoryTimer = nullptr;
    QLabel *m_countLabel = nullptr;
    QPushButton *m_selectAllButton = nullptr;
    QPushButton *m_invertButton = nullptr;
    QPushButton *m_deleteButton = nullptr;

    int m_rightMargin = 16;
    int m_bottomMargin = 16;
    int m_manualListHeight = 0;
    int m_factoryOffset = 0;
    bool m_fixedPosition = true;
    bool m_fixedSize = false;
    int m_dragMode = 0; // 0 none, 1 move, 2 resize
    QPoint m_dragStart;
    int m_dragStartRight = 0;
    int m_dragStartBottom = 0;
    int m_resizeStartHeight = 0;
};
