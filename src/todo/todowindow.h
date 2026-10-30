// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DBlurEffectWidget>
#include <QWidget>

class QLabel;
class QListView;
class TodoModel;

/**
 * Floating todo window (optional keep-above, draggable, hidden from the dock).
 *
 * On Wayland it is a layer-shell overlay surface (keyboard interactivity is
 * none so it never steals focus); new entries are added from the panel or the
 * "+" button which opens a normal dialog.
 */
class TodoWindow : public Dtk::Widget::DBlurEffectWidget
{
    Q_OBJECT

public:
    explicit TodoWindow(TodoModel *model, QWidget *parent = nullptr);

    void popup();
    void refreshLayout();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupWindow();
    void setupUi();
    void addEntry();

    TodoModel *m_model = nullptr;
    QListView *m_view = nullptr;
    QLabel *m_countLabel = nullptr;
};
