// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/** Creates the application-wide about dialog that DTK's titlebar menu opens. */
void setupAboutDialog();

/** DTK native about dialog shared by the panel and the chat window. */
void showAboutDialog(const QString &description);
