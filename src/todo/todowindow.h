// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DBlurEffectWidget>
#include <QWidget>

#include <memory>

class QCloseEvent;
class QLabel;
class QListView;
class QTimer;
class TodoModel;
class UkuiWindow;

/**
 * Floating todo window (optional keep-above, draggable, hidden from the dock).
 *
 * 默认是普通窗口（非置顶、可被别的窗口盖住），并且在 Wayland 上通过
 * ukui-shell 协议设置 skip-taskbar，所以 dock 里不会出现图标；托盘图标在
 * App 里维护，关闭窗口只是隐藏到托盘。
 */
class TodoWindow : public Dtk::Widget::DBlurEffectWidget
{
    Q_OBJECT

public:
    explicit TodoWindow(TodoModel *model, QWidget *parent = nullptr);
    ~TodoWindow() override;

    void popup();
    void refreshLayout();

public slots:
    void addEntry();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void recreateRequested();
    /** 用户点了关闭（隐藏到托盘），让外层给个提示。 */
    void hiddenToTray();

private:
    void setupWindow();
    void setupUi();
    void applyMargins();
    void saveMargins();
    bool useUkuiPositioning() const;

    TodoModel *m_model = nullptr;
    QListView *m_view = nullptr;
    QLabel *m_countLabel = nullptr;
    int m_rightMargin = 16;
    int m_topMargin = 72;
    bool m_keepAbove = false;
    bool m_dragging = false;
    QPoint m_dragStart;
    int m_dragStartRight = 0;
    int m_dragStartTop = 0;
    std::unique_ptr<UkuiWindow> m_ukui;
};
