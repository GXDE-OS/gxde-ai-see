// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

class DragWatcher : public QObject
{
    Q_OBJECT

public:
    static DragWatcher *instance();

signals:
    void dragStarted();
    void dragEnded();

protected:
    explicit DragWatcher(QObject *parent = nullptr)
        : QObject(parent) {}
};
