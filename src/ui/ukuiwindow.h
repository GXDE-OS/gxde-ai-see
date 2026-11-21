// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

class QWindow;

/**
 * 通过 ukui-shell 协议给普通 xdg_toplevel 窗口补两项能力：
 *  - 不进 dock / 不进窗口切换器（xdg_toplevel 本身没有 skip-taskbar）；
 *  - 按全局坐标摆位置（Wayland 客户端不能自己 move 普通窗口）。
 *
 * gxde-wlcom（以及基于 DDE shell 的合成器）实现了这套协议；其它环境
 * 是空操作，调用方需要自己降级。
 */
class UkuiWindow : public QObject
{
    Q_OBJECT

public:
    explicit UkuiWindow(QObject *parent = nullptr);
    ~UkuiWindow() override;

    /** 协议可用（Wayland + 合成器实现了 ukui_shell）。 */
    bool available() const;
    /** 把窗口挂到协议上；成功返回 true。窗口需已创建。 */
    bool attach(QWindow *window);
    void setSkipTaskbar(bool skip);
    void setPosition(int x, int y);

signals:
    /** 合成器报告的新位置（全局坐标），窗口大小请用 QWindow 自己的。 */
    void positionChanged(int x, int y);

private:
    class Private;
    Private *d = nullptr;
};
