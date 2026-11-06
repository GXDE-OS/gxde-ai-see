// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatbridge.h"

#include "chatclient.h"
#include "config.h"

#include <QFile>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

#include <utility>

namespace {

/* Runs before the page's own scripts: connects the channel and hands the
 * "chat" object to the page as window.gxdeBridge (a promise). */
constexpr auto kBootstrap = R"(
window.gxdeBridge = new Promise(function (resolve) {
    new QWebChannel(qt.webChannelTransport, function (channel) {
        resolve(channel.objects.chat);
    });
});
)";

QString readResource(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("ChatBridge: cannot read %s", qPrintable(path));
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

ChatBridge::ChatBridge(QObject *parent)
    : QObject(parent)
    , m_client(new ChatClient(this))
    , m_storePath(Config::instance()->dataDir() + QStringLiteral("/chat.json"))
{
    /* ChatClient's signals belong to the running turn; tag them with its
     * conversation so the page can follow a turn while another is shown */
    connect(m_client, &ChatClient::delta, this, [this](const QString &text) {
        Q_EMIT delta(m_turnConversation, text);
    });
    connect(m_client, &ChatClient::thought, this, [this](const QString &text) {
        Q_EMIT thought(m_turnConversation, text);
    });
    connect(m_client, &ChatClient::toolCall, this,
            [this](const QString &id, const QString &title, const QString &kind,
                   const QString &status) {
                Q_EMIT toolCall(m_turnConversation, id, title, kind, status);
            });
    connect(m_client, &ChatClient::permissionRequested, this,
            [this](const QString &key, const AiCorePermission &request) {
                QJsonArray options;
                for (const QJsonObject &option : request.options) {
                    options.append(option);
                }
                Q_EMIT permissionRequested(m_turnConversation, key, request.toolCall, options);

                QString title = request.toolCall.value(QStringLiteral("title")).toString();
                if (title.trimmed().isEmpty()) {
                    title = request.toolCall.value(QStringLiteral("kind")).toString();
                }
                Q_EMIT permissionPending(title.trimmed());
            });
    connect(m_client, &ChatClient::permissionDropped, this, &ChatBridge::permissionDropped);
    connect(m_client, &ChatClient::activity, this, [this]() {
        Q_EMIT activity(m_turnConversation);
    });
    connect(m_client, &ChatClient::finished, this, &ChatBridge::endTurn);
    connect(m_client, &ChatClient::failed, this, [this](const QString &error) {
        const QString conversationId = m_turnConversation;
        m_turnConversation.clear();
        Q_EMIT failed(conversationId, error);
        Q_EMIT busyChanged();
    });
    connect(AiCore::instance(), &AiCore::accountsChanged, this, &ChatBridge::accountsChanged);
}

void ChatBridge::install(QWebEnginePage *page)
{
    auto *channel = new QWebChannel(page);
    channel->registerObject(QStringLiteral("chat"), this);
    page->setWebChannel(channel);

    QWebEngineScript script;
    script.setName(QStringLiteral("gxde-bridge"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(readResource(QStringLiteral(":/qtwebchannel/qwebchannel.js"))
                         + QString::fromUtf8(kBootstrap));
    page->scripts().insert(script);

    /* a reload starts a page that has not asked for anything yet */
    connect(page, &QWebEnginePage::loadStarted, this, &ChatBridge::pageReset);
}

QString ChatBridge::mode() const
{
    return m_client->mode();
}

void ChatBridge::setMode(const QString &modeId)
{
    /* notify even when the mode stays: the page has already cached what it
     * assigned, the signal hands it back the real value */
    m_client->setMode(modeId);
    Q_EMIT modeChanged();
}

void ChatBridge::ask(const QString &text, const QStringList &files)
{
    if (!m_pageReady) {
        m_pendingAsks.append({ text, files });
        return;
    }
    Q_EMIT askRequested(text, files);
}

void ChatBridge::ready()
{
    m_pageReady = true;
    const auto pending = std::exchange(m_pendingAsks, {});
    for (const auto &[text, files] : pending) {
        Q_EMIT askRequested(text, files);
    }
}

QJsonArray ChatBridge::modes() const
{
    QJsonArray result;
    for (const QString &id : ChatClient::modeIds()) {
        result.append(QJsonObject{ { QStringLiteral("id"), id },
                                   { QStringLiteral("label"), ChatClient::modeLabel(id) } });
    }
    return result;
}

void ChatBridge::send(const QString &conversationId, const QJsonArray &history)
{
    /* ChatClient::send() drops a running turn without a word */
    if (busy()) {
        endTurn(QStringLiteral("cancelled"));
    }

    QVector<ChatMessage> messages;
    messages.reserve(history.size());
    for (const QJsonValue &value : history) {
        const QJsonObject object = value.toObject();
        messages.append({ object.value(QStringLiteral("role")).toString(),
                          object.value(QStringLiteral("content")).toString() });
    }

    m_turnConversation = conversationId;
    Q_EMIT busyChanged();
    m_client->send(conversationId, messages);
}

void ChatBridge::cancel()
{
    if (!busy()) {
        return;
    }
    m_client->cancel();
    endTurn(QStringLiteral("cancelled"));
}

void ChatBridge::forget(const QString &conversationId)
{
    if (conversationId == m_turnConversation) {
        cancel();
    }
    m_client->forget(conversationId);
}

void ChatBridge::resetSessions()
{
    m_client->resetSessions();
}

void ChatBridge::answerPermission(const QString &key, const QString &optionId)
{
    m_client->answerPermission(key, optionId);
}

QJsonObject ChatBridge::loadConversations() const
{
    QFile file(m_storePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool ChatBridge::saveConversations(const QJsonObject &store)
{
    QSaveFile file(m_storePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(store).toJson(QJsonDocument::Indented));
    return file.commit();
}

void ChatBridge::listAccounts(int callId)
{
    AiCore::instance()->listAccounts(replyTo(callId));
}

void ChatBridge::currentAccount(int callId)
{
    AiCore::instance()->currentAccount(replyTo(callId));
}

void ChatBridge::saveAccount(int callId, const QJsonObject &account, const QString &apiKey)
{
    AiCore::instance()->saveAccount(account, apiKey, replyTo(callId));
}

void ChatBridge::deleteAccount(int callId, const QString &id)
{
    AiCore::instance()->deleteAccount(id, replyTo(callId));
}

void ChatBridge::activateAccount(int callId, const QString &id, const QString &model)
{
    AiCore::instance()->activateAccount(id, model, replyTo(callId));
}

AiCore::Callback ChatBridge::replyTo(int callId)
{
    return [self = QPointer<ChatBridge>(this), callId](const QJsonValue &result,
                                                       const QString &error) {
        if (self) {
            Q_EMIT self->replied(callId, result,
                                 error.isEmpty() ? QString() : AiCore::friendlyError(error));
        }
    };
}

void ChatBridge::endTurn(const QString &stopReason)
{
    const QString conversationId = m_turnConversation;
    m_turnConversation.clear();
    Q_EMIT finished(conversationId, stopReason);
    Q_EMIT busyChanged();
}

void ChatBridge::pageReset()
{
    m_pageReady = false;
}
