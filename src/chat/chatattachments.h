// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>

/**
 * 用户消息里的附件标记（界面统一用 `[文件] /path`、`[图片] /path`）
 * 转换成 AI Core 的 ACP prompt 内容块：
 *  - 图片：resource 块，带 base64 内嵌数据（各供应商路由都支持）；
 *  - 文本类文件：读出来放进 text 块（限 512KB，避免触发 AI Core 的读文件权限）；
 *  - 其它二进制：留一条说明，让模型需要时用读取工具。
 */
namespace ChatAttachments {

/** 去掉消息尾部的附件标记行，返回剩下的文字；路径收集到 paths。 */
QString takeLines(const QString &text, QStringList *paths);

/** 组装 session/prompt 的 prompt 数组。context 可为空。 */
QJsonArray promptBlocks(const QString &context, const QString &text, const QStringList &paths);

/** 单个附件是否应该内嵌为图片（供测试/调用方判断）。 */
bool isImageFile(const QString &path);

} // namespace ChatAttachments
