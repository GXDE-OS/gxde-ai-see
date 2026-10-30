// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "factoryactions.h"

#include <QFileInfo>

namespace {

FactoryAction action(const QString &id, const QString &label, const QString &icon = QString(),
                     const QString &hint = QString(), bool enabled = true)
{
    return FactoryAction{ id, label, icon, hint, enabled };
}

QVector<FactoryAction> linkActions(const DropContent &content)
{
    QVector<FactoryAction> actions;
    const LinkInfo &link = content.link;

    if (link.kind == LinkInfo::Http) {
        actions.append(action(QStringLiteral("open"), QStringLiteral("浏览器打开"),
                              QStringLiteral("web-browser")));
        if (link.fileType == QStringLiteral("image")) {
            actions.append(action(QStringLiteral("preview"), QStringLiteral("预览图片"),
                                  QStringLiteral("image-x-generic")));
        } else if (link.fileType == QStringLiteral("audio") ||
                   link.fileType == QStringLiteral("video")) {
            actions.append(action(QStringLiteral("play"), QStringLiteral("在线播放"),
                                  QStringLiteral("media-playback-start")));
        }
        actions.append(action(QStringLiteral("download"), QStringLiteral("下载"),
                              QStringLiteral("download")));
        actions.append(action(QStringLiteral("save_as"), QStringLiteral("另存为…"),
                              QStringLiteral("document-save-as")));
        actions.append(action(QStringLiteral("copy"), QStringLiteral("复制链接"),
                              QStringLiteral("edit-copy")));
        actions.append(action(QStringLiteral("copy_markdown"), QStringLiteral("复制为 Markdown"),
                              QStringLiteral("edit-copy")));
        actions.append(action(QStringLiteral("copy_name"), QStringLiteral("复制文件名"),
                              QStringLiteral("edit-copy")));
        actions.append(action(QStringLiteral("todo"), QStringLiteral("添加到待办"),
                              QStringLiteral("checkbox-checked")));
        actions.append(action(QStringLiteral("ask"), QStringLiteral("问问小埃"),
                              QStringLiteral("dialog-question")));
        return actions;
    }

    actions.append(action(QStringLiteral("try_open"), QStringLiteral("尝试打开"),
                          QStringLiteral("application-x-executable")));
    if (!link.decodedUrl.isEmpty()) {
        actions.append(action(QStringLiteral("decode"), QStringLiteral("查看真实地址"),
                              QStringLiteral("edit-find")));
        actions.append(action(QStringLiteral("copy_decoded"), QStringLiteral("复制真实地址"),
                              QStringLiteral("edit-copy")));
    }
    actions.append(action(QStringLiteral("copy"), QStringLiteral("复制链接"),
                          QStringLiteral("edit-copy")));
    if (!link.displayName.isEmpty()) {
        actions.append(action(QStringLiteral("copy_name"), QStringLiteral("复制名称"),
                              QStringLiteral("edit-copy")));
    }
    actions.append(action(QStringLiteral("todo"), QStringLiteral("添加到待办"),
                          QStringLiteral("checkbox-checked")));
    actions.append(action(QStringLiteral("ask"), QStringLiteral("问问小埃"),
                          QStringLiteral("dialog-question")));
    return actions;
}

QVector<FactoryAction> fileActions(const DropContent &content)
{
    QVector<FactoryAction> actions;
    const QString path = content.filePaths.value(0);
    const QString type = LinkTools::fileTypeForExtension(QFileInfo(path).suffix().toLower());

    actions.append(action(QStringLiteral("open"), QStringLiteral("打开"),
                          QStringLiteral("document-open")));
    if (type == QStringLiteral("image")) {
        actions.append(action(QStringLiteral("ocr"), QStringLiteral("提取文字"),
                              QStringLiteral("insert-text")));
        actions.append(action(QStringLiteral("describe"), QStringLiteral("生成描述"),
                              QStringLiteral("dialog-question")));
    } else if (type == QStringLiteral("audio") || type == QStringLiteral("video")) {
        actions.append(action(QStringLiteral("transcribe"), QStringLiteral("转文字"),
                              QStringLiteral("insert-text"), QStringLiteral("尚未接入"), false));
    } else if (type == QStringLiteral("document") || type == QStringLiteral("code")) {
        actions.append(action(QStringLiteral("segment"), QStringLiteral("分词"),
                              QStringLiteral("insert-text")));
        actions.append(action(QStringLiteral("extract_links"), QStringLiteral("提取链接"),
                              QStringLiteral("insert-link")));
        actions.append(action(QStringLiteral("speak"), QStringLiteral("朗读"),
                              QStringLiteral("audio-volume-high")));
    }

    actions.append(action(QStringLiteral("copy_path"), QStringLiteral("复制路径"),
                          QStringLiteral("edit-copy")));
    actions.append(action(QStringLiteral("todo"), QStringLiteral("添加到待办"),
                          QStringLiteral("checkbox-checked")));
    actions.append(action(QStringLiteral("ask"), QStringLiteral("问问小埃"),
                          QStringLiteral("dialog-question")));
    return actions;
}

} // namespace

QVector<FactoryAction> actionsFor(const DropContent &content)
{
    switch (content.kind) {
    case DropContent::Files:
        return fileActions(content);
    case DropContent::Image:
        return {
            action(QStringLiteral("preview"), QStringLiteral("预览图片"),
                   QStringLiteral("image-x-generic")),
            action(QStringLiteral("save_image"), QStringLiteral("保存图片…"),
                   QStringLiteral("document-save-as")),
            action(QStringLiteral("ocr"), QStringLiteral("提取文字"),
                   QStringLiteral("insert-text")),
            action(QStringLiteral("describe"), QStringLiteral("生成描述"),
                   QStringLiteral("dialog-question")),
            action(QStringLiteral("todo"), QStringLiteral("添加到待办"),
                   QStringLiteral("checkbox-checked")),
            action(QStringLiteral("ask"), QStringLiteral("问问小埃"),
                   QStringLiteral("dialog-question")),
        };
    case DropContent::Text:
        return {
            action(QStringLiteral("segment"), QStringLiteral("分词"),
                   QStringLiteral("insert-text")),
            action(QStringLiteral("extract_links"), QStringLiteral("提取链接"),
                   QStringLiteral("insert-link")),
            action(QStringLiteral("translate"), QStringLiteral("翻译"),
                   QStringLiteral("accessories-dictionary")),
            action(QStringLiteral("speak"), QStringLiteral("朗读"),
                   QStringLiteral("audio-volume-high")),
            action(QStringLiteral("copy"), QStringLiteral("复制"), QStringLiteral("edit-copy")),
            action(QStringLiteral("todo"), QStringLiteral("添加到待办"),
                   QStringLiteral("checkbox-checked")),
            action(QStringLiteral("ask"), QStringLiteral("问问小埃"),
                   QStringLiteral("dialog-question")),
        };
    case DropContent::Link:
    case DropContent::CustomLink:
        return linkActions(content);
    default:
        return {};
    }
}
