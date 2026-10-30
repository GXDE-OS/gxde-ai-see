// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QVector>

#include "contentclassifier.h"

struct FactoryAction
{
    QString id;
    QString label;
    QString icon; // theme icon name, optional
    QString hint;
    bool enabled = true;
};

/** Actions offered by the content factory for a given payload. */
QVector<FactoryAction> actionsFor(const DropContent &content);
