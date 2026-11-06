// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatclient.h"

#include <QJsonArray>
#include <QJsonDocument>

#include "config.h"

namespace {

constexpr char kProviderMethod[] = "hachimi-provider-account";

// Amount of messages that a rebuilt session see.
constexpr int kReplayMessages = 40;
constexpr int kReplayChars = 24000;

QString speaker(const ChatMessage &message)
{
    if (message.role == QStringLiteral("user")) {
        return QObject::tr("用户");
    }

    if (message.role == QStringLiteral("assistant")) {
        return QObject::tr("小埃");
    }

    return QObject::tr("备注");
}


QString sessionContext(const QVector<ChatMessage> &earlier)
{
    QString context = QObject::tr(
        "你是 GXDE 桌面上的助手「小埃同学」，正在通过图形界面的对话窗口与用户交流（不是终端）。"
        "请使用用户的语言回答，可以使用 Markdown。");

    const QString systemPrompt =
        Config::instance()->get(QStringLiteral("chat/systemPrompt")).toString().trimmed();
    if (!systemPrompt.isEmpty()) {
        context += QStringLiteral("\n\n") + QObject::tr("用户的额外要求：") +
                   QLatin1Char('\n') + systemPrompt;
    }

    QStringList lines;
    int chars = 0;
    for (int i = earlier.size() - 1; i >= 0 && lines.size() < kReplayMessages; --i) {
        const ChatMessage &message = earlier.at(i);
        const QString content = message.content.trimmed();
        if (content.isEmpty() || content.startsWith(QStringLiteral("⚠️"))) {
            continue;
        }

        const QString line = speaker(message) + QStringLiteral("：") + content;
        if (chars + line.size() > kReplayChars && !lines.isEmpty()) {
            break;
        }

        chars += line.size();
        lines.prepend(line.left(kReplayChars));
    }
    if (!lines.isEmpty()) {
        context += QStringLiteral("\n\n") +
                   QObject::tr("以下是这段对话此前的记录（可能已截断），请在此基础上继续：") +
                   QStringLiteral("\n\n") + lines.join(QStringLiteral("\n\n"));
    }
    
    return context + QStringLiteral("\n\n---\n") + QObject::tr("用户的新消息：") +
           QLatin1Char('\n');
}

QJsonObject textBlock(const QString &text)
{
    return QJsonObject{ { QStringLiteral("type"), QStringLiteral("text") },
                        { QStringLiteral("text"), text } };
}

} // namespace

QStringList ChatClient::modeIds()
{
    return { QStringLiteral("default"), QStringLiteral("autoEdit"), QStringLiteral("yolo"),
             QStringLiteral("plan") };
}

QString ChatClient::modeLabel(const QString &modeId)
{
    if (modeId == QStringLiteral("autoEdit")) {
        return tr("自动允许编辑");
    }

    if (modeId == QStringLiteral("yolo")) {
        return tr("全部自动允许");
    }

    if (modeId == QStringLiteral("plan")) {
        return tr("只读");
    }

    return tr("每次确认");
}

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
{
    m_mode = Config::instance()
                 ->get(QStringLiteral("chat/approvalMode"), QStringLiteral("default"))
                 .toString();
    if (!modeIds().contains(m_mode)) {
        m_mode = QStringLiteral("default");
    }

    AiCore *core = AiCore::instance();
    connect(core, &AiCore::sessionUpdate, this, &ChatClient::handleUpdate);
    connect(core, &AiCore::permissionRequested, this, &ChatClient::handlePermission);
    connect(core, &AiCore::accountsChanged, this, [this]() { m_sessions.clear(); });
    connect(core, &AiCore::stopped, this, [this](const QString &reason) {
        m_sessions.clear();
        if (busy()) {
            m_permissions.clear();
            endTurn();
            Q_EMIT failed(reason);
        }
    });
}

ChatClient::~ChatClient()
{
    cancel();
}

void ChatClient::send(const QString &conversationId, const QVector<ChatMessage> &history)
{
    cancel();
    if (history.isEmpty() || history.constLast().role != QStringLiteral("user")) {
        Q_EMIT failed(tr("没有要发送的消息"));
        return;
    }
    startTurn(conversationId, history);
}

void ChatClient::startTurn(const QString &conversationId, const QVector<ChatMessage> &history)
{
    m_turnConversation = conversationId;
    m_turnSession.clear();
    const int token = ++m_turnToken;

    const int earlier = history.size() - 1;
    const QString text = history.constLast().content;
    const int syncedAfter = history.size() + 1;

    const auto it = m_sessions.constFind(conversationId);
    if (it != m_sessions.constEnd() && it->synced == earlier) {
        prompt(conversationId, it->id, QJsonArray{ textBlock(text) }, syncedAfter);
        return;
    }
    m_sessions.remove(conversationId);

    const QJsonArray blocks{ textBlock(sessionContext(history.mid(0, earlier))),
                             textBlock(text) };
    AiCore *core = AiCore::instance();

    auto fail = [this, token](const QString &error) {
        if (token != m_turnToken) {
            return;
        }
        endTurn();
        Q_EMIT failed(AiCore::friendlyError(error));
    };

    core->call(
        QStringLiteral("authenticate"),
        { { QStringLiteral("methodId"), QString::fromLatin1(kProviderMethod) } },
        [=, this](const QJsonValue &, const QString &authError) {
            if (token != m_turnToken) {
                return;
            }
            if (!authError.isEmpty()) {
                fail(authError);
                return;
            }
            core->call(
                QStringLiteral("session/new"),
                { { QStringLiteral("cwd"), AiCore::workspace() },
                  { QStringLiteral("mcpServers"), mcpServers() } },
                [=, this](const QJsonValue &result, const QString &error) {
                    if (token != m_turnToken) {
                        return;
                    }
                    const QString sessionId =
                        result.toObject().value(QStringLiteral("sessionId")).toString();
                    if (!error.isEmpty() || sessionId.isEmpty()) {
                        fail(error.isEmpty() ? tr("GXDE AI Core 没有创建会话") : error);
                        return;
                    }
                    m_sessions.insert(conversationId, Session{ sessionId, 0 });
                    const QString current = result.toObject()
                                                .value(QStringLiteral("modes"))
                                                .toObject()
                                                .value(QStringLiteral("currentModeId"))
                                                .toString();
                    if (!current.isEmpty() && current != m_mode) {
                        core->call(QStringLiteral("session/set_mode"),
                                   { { QStringLiteral("sessionId"), sessionId },
                                     { QStringLiteral("modeId"), m_mode } });
                    }
                    prompt(conversationId, sessionId, blocks, syncedAfter);
                });
        });
}

void ChatClient::prompt(const QString &conversationId, const QString &sessionId,
                        const QJsonArray &blocks, int syncedAfter)
{
    const int token = m_turnToken;
    m_turnSession = sessionId;
    AiCore::instance()->call(
        QStringLiteral("session/prompt"),
        { { QStringLiteral("sessionId"), sessionId }, { QStringLiteral("prompt"), blocks } },
        [=, this](const QJsonValue &result, const QString &error) {
            if (token != m_turnToken) {
                return;
            }
            dropPermissions();
            endTurn();
            if (!error.isEmpty()) {
                m_sessions.remove(conversationId);
                Q_EMIT failed(AiCore::friendlyError(error));
                return;
            }

            const QString stopReason =
                result.toObject().value(QStringLiteral("stopReason")).toString();
            auto it = m_sessions.find(conversationId);
            if (it != m_sessions.end() && it->id == sessionId) {
                if (stopReason == QStringLiteral("cancelled")) {
                    m_sessions.erase(it);
                } else {
                    it->synced = syncedAfter;
                }
            }

            Q_EMIT finished(stopReason);
        });
}

void ChatClient::cancel()
{
    if (!busy()) {
        return;
    }

    if (!m_turnSession.isEmpty()) {
        AiCore::instance()->notify(QStringLiteral("session/cancel"),
                                   { { QStringLiteral("sessionId"), m_turnSession } });
    }

    dropPermissions();
    m_sessions.remove(m_turnConversation);
    endTurn();
}

void ChatClient::endTurn()
{
    ++m_turnToken;
    m_turnConversation.clear();
    m_turnSession.clear();
}

void ChatClient::forget(const QString &conversationId)
{
    if (m_turnConversation == conversationId) {
        cancel();
    }
    m_sessions.remove(conversationId);
}

void ChatClient::resetSessions()
{
    m_sessions.clear();
}

void ChatClient::setMode(const QString &modeId)
{
    if (!modeIds().contains(modeId) || modeId == m_mode) {
        return;
    }

    m_mode = modeId;
    Config::instance()->set(QStringLiteral("chat/approvalMode"), modeId);
    Config::instance()->sync();
    for (const Session &session : std::as_const(m_sessions)) {
        AiCore::instance()->call(QStringLiteral("session/set_mode"),
                                 { { QStringLiteral("sessionId"), session.id },
                                   { QStringLiteral("modeId"), modeId } });
    }
}

void ChatClient::answerPermission(const QString &key, const QString &optionId)
{
    if (!m_permissions.contains(key)) {
        return;
    }
    const AiCorePermission request = m_permissions.take(key);
    QString chosen = optionId;
    if (chosen.isEmpty()) {
        for (const QJsonObject &option : request.options) {
            if (option.value(QStringLiteral("kind")).toString() ==
                QStringLiteral("reject_once")) {
                chosen = option.value(QStringLiteral("optionId")).toString();
                break;
            }
        }
    }
    AiCore::instance()->answerPermission(request.rpcId, chosen);
}

void ChatClient::dropPermissions()
{
    const QHash<QString, AiCorePermission> pending = m_permissions;
    m_permissions.clear();
    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
        AiCore::instance()->answerPermission(it->rpcId, QString());
        Q_EMIT permissionDropped(it.key());
    }
}

void ChatClient::handleUpdate(const QString &sessionId, const QJsonObject &update)
{
    if (!busy() || sessionId != m_turnSession) {
        return;
    }
    const QString kind = update.value(QStringLiteral("sessionUpdate")).toString();
    if (kind != QStringLiteral("available_commands_update")) {
        Q_EMIT activity();
    }
    if (kind == QStringLiteral("agent_message_chunk")) {
        const QJsonObject content = update.value(QStringLiteral("content")).toObject();
        if (content.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
            const QString text = content.value(QStringLiteral("text")).toString();
            /* set_mode echoes the new mode into the message stream */
            if (!text.isEmpty() && !text.startsWith(QStringLiteral("[MODE_UPDATE]"))) {
                Q_EMIT delta(text);
            }
        }
    } else if (kind == QStringLiteral("tool_call") ||
               kind == QStringLiteral("tool_call_update")) {
        Q_EMIT toolCall(update.value(QStringLiteral("toolCallId")).toString(),
                        update.value(QStringLiteral("title")).toString(),
                        update.value(QStringLiteral("kind")).toString(),
                        update.value(QStringLiteral("status")).toString());
    }
}

void ChatClient::handlePermission(const AiCorePermission &request)
{
    if (!busy() || request.sessionId != m_turnSession) {
        AiCore::instance()->answerPermission(request.rpcId, QString());
        return;
    }
    const QString key = QString::number(m_nextPermission++);
    m_permissions.insert(key, request);
    Q_EMIT permissionRequested(key, request);
}

QJsonArray ChatClient::mcpServers() const
{
    QJsonArray servers;
    const QString json =
        Config::instance()->get(QStringLiteral("chat/mcpServers")).toString().trimmed();
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        const QString command = object.value(QStringLiteral("command")).toString();
        if (command.isEmpty()) {
            continue;
        }
        QString name = object.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("mcp%1").arg(servers.size() + 1);
        }
        QJsonArray args;
        for (const QJsonValue &arg : object.value(QStringLiteral("args")).toArray()) {
            args.append(arg.toString());
        }
        QJsonArray env;
        const QJsonObject envObject = object.value(QStringLiteral("env")).toObject();
        for (auto it = envObject.constBegin(); it != envObject.constEnd(); ++it) {
            env.append(QJsonObject{ { QStringLiteral("name"), it.key() },
                                    { QStringLiteral("value"), it.value().toString() } });
        }
        servers.append(QJsonObject{ { QStringLiteral("name"), name },
                                    { QStringLiteral("command"), command },
                                    { QStringLiteral("args"), args },
                                    { QStringLiteral("env"), env } });
    }
    return servers;
}
