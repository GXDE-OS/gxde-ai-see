// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DBlurEffectWidget>
#include <QWidget>

class QLabel;
class QPushButton;
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

private:
    void setupUi();
    void setupWindow();
    void updateGeometryForContent();
    void updateSelectionBar();

    StagingModel *m_model = nullptr;
    StagingView *m_view = nullptr;
    PortalZone *m_zone = nullptr;
    QWidget *m_header = nullptr;
    QWidget *m_selectionBar = nullptr;
    QWidget *m_factory = nullptr;
    QLabel *m_countLabel = nullptr;
    QPushButton *m_selectAllButton = nullptr;
    QPushButton *m_invertButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
};
