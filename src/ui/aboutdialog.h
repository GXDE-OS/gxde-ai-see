// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

class QWidget;

/** DTK native about dialog shared by the panel and the chat window. */
void showAboutDialog(QWidget *parent, const QString &description);
