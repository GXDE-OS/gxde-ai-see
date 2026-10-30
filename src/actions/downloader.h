// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

class QWidget;

/**
 * Small async downloader for links dropped on the content factory.
 *
 * Emits \a message with a human readable status; the panel shows it as a
 * toast.  Downloads go to the XDG download directory unless "save as" is used.
 */
class Downloader : public QObject
{
    Q_OBJECT

public:
    static Downloader *instance();

    void download(const QUrl &url);
    void saveAs(const QUrl &url, QWidget *parent);

signals:
    void message(const QString &text);

private:
    explicit Downloader(QObject *parent = nullptr);

    void start(const QUrl &url, const QString &path);
    static QString suggestedFileName(const QUrl &url);
    static QString uniquePath(const QString &directory, const QString &fileName);

    QNetworkAccessManager m_manager;
};
