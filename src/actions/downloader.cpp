// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "downloader.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

#include "factory/linktools.h"

Downloader *Downloader::instance()
{
    static Downloader *self = new Downloader;
    return self;
}

Downloader::Downloader(QObject *parent)
    : QObject(parent)
{
    m_manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

QString Downloader::suggestedFileName(const QUrl &url)
{
    QString name = LinkTools::displayNameFor(url);
    name = QFileInfo(name).fileName();
    if (name.isEmpty()) {
        name = QStringLiteral("download");
    }
    return name;
}

QString Downloader::uniquePath(const QString &directory, const QString &fileName)
{
    QDir dir(directory);
    QString path = dir.filePath(fileName);
    if (!QFileInfo::exists(path)) {
        return path;
    }

    const QFileInfo info(path);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();
    for (int index = 1; index < 1000; ++index) {
        const QString candidate =
            suffix.isEmpty()
                ? dir.filePath(QStringLiteral("%1 (%2)").arg(base).arg(index))
                : dir.filePath(QStringLiteral("%1 (%2).%3").arg(base).arg(index).arg(suffix));
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return path;
}

void Downloader::download(const QUrl &url)
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    QDir().mkpath(directory);
    start(url, uniquePath(directory, suggestedFileName(url)));
}

void Downloader::saveAs(const QUrl &url, QWidget *parent)
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    const QString suggested = QDir(directory).filePath(suggestedFileName(url));
    const QString path = QFileDialog::getSaveFileName(parent, tr("另存为"), suggested);
    if (path.isEmpty()) {
        return;
    }
    start(url, path);
}

void Downloader::start(const QUrl &url, const QString &path)
{
    auto *file = new QFile(path, this);
    if (!file->open(QIODevice::WriteOnly)) {
        Q_EMIT message(tr("无法写入：%1").arg(path));
        file->deleteLater();
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_manager.get(request);

    Q_EMIT message(tr("开始下载：%1").arg(QFileInfo(path).fileName()));

    connect(reply, &QNetworkReply::readyRead, file, [reply, file]() {
        file->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, path]() {
        file->write(reply->readAll());
        file->close();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT message(tr("下载失败：%1").arg(reply->errorString()));
            QFile::remove(path);
        } else {
            Q_EMIT message(tr("下载完成：%1").arg(path));
        }
        file->deleteLater();
        reply->deleteLater();
    });
}
