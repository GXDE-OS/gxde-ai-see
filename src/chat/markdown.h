// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/** Lightweight Markdown to HTML used by assistant bubbles. */
QString markdownToHtml(const QString &markdown);

/** Escape plain text and keep line breaks. */
QString escapedHtml(const QString &text);
