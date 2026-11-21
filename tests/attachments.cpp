// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

/* 检查附件标记 → ACP prompt 块的转换：
 *  - `[文件]` / `[图片]` 行从文字里剥掉并变成附件；
 *  - 图片内嵌成 resource(blob) 块；
 *  - 文本文件读进来变成 text 块；
 *  - 不存在的路径给出说明文本。
 */

#include "chat/chatattachments.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int failures = 0;

void check(bool condition, const QString &what)
{
    if (condition) {
        QTextStream(stdout) << "ok   " << what << "\n";
    } else {
        QTextStream(stderr) << "FAIL " << what << "\n";
        ++failures;
    }
}

/* 1x1 透明 PNG，硬编码省掉图片插件依赖 */
constexpr char kPngBase64[] = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGNg"
                              "YGD4DwABBAEAcCBlCwAAAABJRU5ErkJggg==";

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir dir;
    if (!dir.isValid()) {
        QTextStream(stderr) << "cannot create temp dir\n";
        return 1;
    }
    const QString textPath = dir.filePath(QStringLiteral("备注.txt"));
    const QString imagePath = dir.filePath(QStringLiteral("截图.png"));
    {
        QFile file(textPath);
        file.open(QIODevice::WriteOnly);
        file.write(QStringLiteral("第一行\n第二行\n").toUtf8());
    }
    {
        QFile file(imagePath);
        file.open(QIODevice::WriteOnly);
        file.write(QByteArray::fromBase64(kPngBase64));
    }

    QStringList paths;
    const QString text = ChatAttachments::takeLines(
        QStringLiteral("帮我看看这个\n[文件] %1\n[图片] %2").arg(textPath, imagePath), &paths);
    check(text == QStringLiteral("帮我看看这个"), "attachment lines stripped from text");
    check(paths == QStringList({ textPath, imagePath }), "attachment paths collected");

    /* 文件管理器拖拽会额外带一条 file:// URI：有附件时应被丢掉 */
    QStringList uriPaths;
    const QString uriText = ChatAttachments::takeLines(
        QStringLiteral("file://%1\n[文件] %2\n图里是什么").arg(imagePath, imagePath),
        &uriPaths);
    check(uriText == QStringLiteral("图里是什么"), "duplicate file:// line dropped");
    check(uriPaths == QStringList({ imagePath }), "uri line does not add an attachment");

    /* 普通消息里的 file:// 链接（没有附件）要保留 */
    QStringList none;
    const QString link = QStringLiteral("看看 file:///tmp/x.txt 这个");
    check(ChatAttachments::takeLines(link, &none) == link, "plain file:// text kept");

    check(ChatAttachments::isImageFile(imagePath), "png detected as image");
    check(!ChatAttachments::isImageFile(textPath), "txt is not an image");

    const QJsonArray blocks =
        ChatAttachments::promptBlocks(QString(), QStringLiteral("帮我看看"),
                                      { textPath, imagePath, dir.filePath(QStringLiteral("没了.png")) });
    check(blocks.size() == 4, "text + file + image + missing = 4 blocks");

    const QJsonObject textBlock = blocks.at(0).toObject();
    check(textBlock.value(QStringLiteral("type")).toString() == QStringLiteral("text"),
          "first block is the user's text");

    const QJsonObject fileBlock = blocks.at(1).toObject();
    check(fileBlock.value(QStringLiteral("type")).toString() == QStringLiteral("text")
              && fileBlock.value(QStringLiteral("text")).toString().contains(QStringLiteral("第一行")),
          "text file content is inlined as text");

    const QJsonObject imageBlock = blocks.at(2).toObject();
    const QJsonObject resource = imageBlock.value(QStringLiteral("resource")).toObject();
    check(imageBlock.value(QStringLiteral("type")).toString() == QStringLiteral("resource"),
          "image becomes an ACP resource block");
    check(resource.value(QStringLiteral("mimeType")).toString() == QStringLiteral("image/png"),
          "image mime type is sent");
    check(!resource.value(QStringLiteral("blob")).toString().isEmpty(),
          "image bytes are inlined as base64");

    const QJsonObject missingBlock = blocks.at(3).toObject();
    check(missingBlock.value(QStringLiteral("text")).toString().contains(QStringLiteral("不存在")),
          "missing attachment explains itself in text");

    const QJsonArray empty = ChatAttachments::promptBlocks(QString(), QString(), {});
    check(empty.size() == 1
              && empty.at(0).toObject().value(QStringLiteral("type")).toString()
                     == QStringLiteral("text"),
          "empty message still sends one text block");

    QTextStream(stdout) << (failures == 0 ? "ALL OK\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
