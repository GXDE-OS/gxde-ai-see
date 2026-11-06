// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aicore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>

namespace {

constexpr int kProtocolVersion = 1;
constexpr char kProviderMethod[] = "hachimi-provider-account";

int jsonRpcError(const QJsonObject &message, QString *text)
{
    const QJsonObject error = message.value(QStringLiteral("error")).toObject();
    *text = error.value(QStringLiteral("message")).toString();
    if (text->isEmpty()) {
        *text = QStringLiteral("GXDE AI Core error %1")
                    .arg(error.value(QStringLiteral("code")).toInt());
    }
    return error.value(QStringLiteral("code")).toInt();
}

} // namespace

AiCore *AiCore::instance()
{
    static AiCore *self = new AiCore(qApp);
    return self;
}

AiCore::AiCore(QObject *parent)
    : QObject(parent)
{
    if (qApp) {
        connect(qApp, &QCoreApplication::aboutToQuit, this, &AiCore::stop);
    }
}

AiCore::~AiCore()
{
    stop();
}

QString AiCore::program()
{
    const QString overridden = qEnvironmentVariable("GXDE_AI_SEE_AICORE");
    if (!overridden.isEmpty()) {
        return overridden;
    }
    return QStandardPaths::findExecutable(QStringLiteral("gxde-ai-core"));
}

QString AiCore::workspace()
{
    return QDir::homePath();
}

bool AiCore::running() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void AiCore::start()
{
    if (running()) {
        return;
    }

    const QString binary = program();
    if (binary.isEmpty()) {
        failPending(tr("没有找到 GXDE AI Core（gxde-ai-core），请先安装 gxde-ai-core 软件包。"));
        return;
    }

    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(workspace());
    m_buffer.clear();
    m_initialized = false;

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_buffer.append(m_process->readAllStandardOutput());
        int newline;
        while ((newline = m_buffer.indexOf('\n')) >= 0) {
            const QByteArray line = m_buffer.left(newline).trimmed();
            m_buffer.remove(0, newline + 1);
            if (!line.isEmpty()) {
                handleLine(line);
            }
        }
    });
    /* AI Core holds its stderr back until it exits, and into a pipe that
     * last write can get lost: a file keeps it for the exit reason. */
    m_stderrPath = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))
                       .filePath(QStringLiteral("gxde-ai-see-aicore.%1.log")
                                     .arg(QCoreApplication::applicationPid()));
    m_process->setStandardErrorFile(m_stderrPath, QIODevice::Truncate);
    connect(m_process, &QProcess::finished, this, &AiCore::handleExit);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            handleExit();
        }
    });

    /* The sessions work in the user's home.  AI Core treats an untrusted
     * workspace as read-mostly: no MCP servers and no auto-approving modes.
     * --skip-trust trusts it for this process only, nothing is persisted. */
    m_process->start(binary, { QStringLiteral("--acp"), QStringLiteral("--skip-trust") });

    /* the handshake jumps the queue: everything else waits for it */
    const int id = m_nextId++;
    m_callbacks.insert(id, [this](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) {
            failPending(error);
            return;
        }
        m_initialized = true;
        const QList<QJsonObject> queued = m_queue;
        m_queue.clear();
        for (const QJsonObject &message : queued) {
            write(message);
        }
    });
    QJsonObject capabilities{
        { QStringLiteral("fs"),
          QJsonObject{ { QStringLiteral("readTextFile"), false },
                       { QStringLiteral("writeTextFile"), false } } },
        { QStringLiteral("terminal"), false },
    };
    write(QJsonObject{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("method"), QStringLiteral("initialize") },
        { QStringLiteral("params"),
          QJsonObject{ { QStringLiteral("protocolVersion"), kProtocolVersion },
                       { QStringLiteral("clientCapabilities"), capabilities },
                       { QStringLiteral("clientInfo"),
                         QJsonObject{ { QStringLiteral("name"), QStringLiteral("gxde-ai-see") },
                                      { QStringLiteral("title"), tr("小埃同学") },
                                      { QStringLiteral("version"),
                                        QCoreApplication::applicationVersion() } } } } },
    });
}

void AiCore::stop()
{
    if (!m_process) {
        return;
    }
    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    if (process->state() != QProcess::NotRunning) {
        /* AI Core relaunches itself in a child process.  End of input stops
         * both; SIGTERM still reaches the child through the parent, only a
         * SIGKILL of the parent would leave the child behind. */
        process->closeWriteChannel();
        if (!process->waitForFinished(1500)) {
            process->terminate();
            if (!process->waitForFinished(1500)) {
                process->kill();
                process->waitForFinished(500);
            }
        }
    }
    process->deleteLater();
    removeStderrFile();
    m_initialized = false;
    const QString reason = tr("GXDE AI Core 已停止");
    failPending(reason);
    Q_EMIT stopped(reason);
}

void AiCore::call(const QString &method, const QJsonObject &params, Callback callback)
{
    const int id = m_nextId++;
    m_callbacks.insert(id, callback ? callback : Callback([](const QJsonValue &, const QString &) {}));
    const QJsonObject message{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("method"), method },
        { QStringLiteral("params"), params },
    };
    start();
    if (!running()) {
        return; // start() already failed every pending callback
    }
    if (m_initialized) {
        write(message);
    } else {
        m_queue.append(message);
    }
}

void AiCore::notify(const QString &method, const QJsonObject &params)
{
    if (!running()) {
        return;
    }
    const QJsonObject message{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("method"), method },
        { QStringLiteral("params"), params },
    };
    if (m_initialized) {
        write(message);
    } else {
        m_queue.append(message);
    }
}

void AiCore::answerPermission(const QJsonValue &rpcId, const QString &optionId)
{
    if (!running()) {
        return;
    }
    const QJsonObject outcome =
        optionId.isEmpty()
            ? QJsonObject{ { QStringLiteral("outcome"), QStringLiteral("cancelled") } }
            : QJsonObject{ { QStringLiteral("outcome"), QStringLiteral("selected") },
                           { QStringLiteral("optionId"), optionId } };
    write(QJsonObject{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), rpcId },
        { QStringLiteral("result"), QJsonObject{ { QStringLiteral("outcome"), outcome } } },
    });
}

void AiCore::listAccounts(Callback callback)
{
    call(QStringLiteral("gxde-ai-core/auth/providers/list"), {}, std::move(callback));
}

void AiCore::currentAccount(Callback callback)
{
    call(QStringLiteral("gxde-ai-core/auth/providers/current"), {}, std::move(callback));
}

void AiCore::saveAccount(const QJsonObject &account, const QString &apiKey, Callback callback)
{
    QJsonObject params{ { QStringLiteral("account"), account } };
    if (!apiKey.trimmed().isEmpty()) {
        params.insert(QStringLiteral("apiKey"), apiKey.trimmed());
    }
    call(QStringLiteral("gxde-ai-core/auth/providers/save"), params,
         [this, callback](const QJsonValue &result, const QString &error) {
             if (error.isEmpty()) {
                 Q_EMIT accountsChanged();
             }
             if (callback) {
                 callback(result, error);
             }
         });
}

void AiCore::deleteAccount(const QString &id, Callback callback)
{
    call(QStringLiteral("gxde-ai-core/auth/providers/delete"),
         { { QStringLiteral("id"), id } },
         [this, callback](const QJsonValue &result, const QString &error) {
             if (error.isEmpty()) {
                 Q_EMIT accountsChanged();
             }
             if (callback) {
                 callback(result, error);
             }
         });
}

void AiCore::activateAccount(const QString &id, const QString &model, Callback callback)
{
    QJsonObject params{ { QStringLiteral("id"), id } };
    if (!model.isEmpty()) {
        params.insert(QStringLiteral("model"), model);
    }
    call(QStringLiteral("gxde-ai-core/auth/providers/activate"), params,
         [this, callback](const QJsonValue &result, const QString &error) {
             if (!error.isEmpty()) {
                 if (callback) {
                     callback(result, error);
                 }
                 return;
             }
             /* the agent picks the new account up on authenticate */
             call(QStringLiteral("authenticate"),
                  { { QStringLiteral("methodId"), QString::fromLatin1(kProviderMethod) } },
                  [this, callback, result](const QJsonValue &, const QString &) {
                      Q_EMIT accountsChanged();
                      if (callback) {
                          callback(result, QString());
                      }
                  });
         });
}

QString AiCore::friendlyError(const QString &error)
{
    if (error.contains(QStringLiteral("Select a provider account")) ||
        error.contains(QStringLiteral("No active provider account"))) {
        return tr("GXDE AI Core 还没有可用的账号。请点左下角「设置」添加一个供应商账号。");
    }
    if (error.contains(QStringLiteral("has no API key")) ||
        error.contains(QStringLiteral("API key is missing"))) {
        return tr("当前 AI Core 账号没有 API Key，请在「设置」里编辑该账号。");
    }
    if (error.contains(QStringLiteral("HTTP 401"))) {
        return tr("API Key 无效或已过期，请在「设置」里检查当前账号。");
    }
    return error;
}

void AiCore::write(const QJsonObject &message)
{
    if (!running()) {
        return;
    }
    m_process->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void AiCore::handleLine(const QByteArray &line)
{
    const QJsonDocument document = QJsonDocument::fromJson(line);
    if (!document.isObject()) {
        return; // stray log output
    }
    const QJsonObject message = document.object();
    const bool hasMethod = message.contains(QStringLiteral("method"));
    const bool hasId = message.contains(QStringLiteral("id"));

    if (hasMethod && hasId) {
        handleRequest(message);
        return;
    }
    if (hasMethod) {
        if (message.value(QStringLiteral("method")).toString() ==
            QStringLiteral("session/update")) {
            const QJsonObject params = message.value(QStringLiteral("params")).toObject();
            Q_EMIT sessionUpdate(params.value(QStringLiteral("sessionId")).toString(),
                                 params.value(QStringLiteral("update")).toObject());
        }
        return;
    }
    if (!hasId) {
        return;
    }

    const int id = message.value(QStringLiteral("id")).toInt(-1);
    const Callback callback = m_callbacks.take(id);
    if (!callback) {
        return;
    }
    if (message.contains(QStringLiteral("error"))) {
        QString text;
        jsonRpcError(message, &text);
        callback(QJsonValue(), text);
        return;
    }
    callback(message.value(QStringLiteral("result")), QString());
}

void AiCore::handleRequest(const QJsonObject &message)
{
    const QString method = message.value(QStringLiteral("method")).toString();
    const QJsonValue id = message.value(QStringLiteral("id"));
    const QJsonObject params = message.value(QStringLiteral("params")).toObject();

    if (method == QStringLiteral("session/request_permission")) {
        AiCorePermission request;
        request.rpcId = id;
        request.sessionId = params.value(QStringLiteral("sessionId")).toString();
        request.toolCall = params.value(QStringLiteral("toolCall")).toObject();
        for (const QJsonValue &option : params.value(QStringLiteral("options")).toArray()) {
            request.options.append(option.toObject());
        }
        Q_EMIT permissionRequested(request);
        return;
    }

    /* no file system or terminal proxy: the agent uses its own */
    write(QJsonObject{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("error"),
          QJsonObject{ { QStringLiteral("code"), -32601 },
                       { QStringLiteral("message"),
                         QStringLiteral("Method not supported: %1").arg(method) } } },
    });
}

QByteArray AiCore::takeStderrTail()
{
    QByteArray tail;
    QFile file(m_stderrPath);
    if (!m_stderrPath.isEmpty() && file.open(QIODevice::ReadOnly)) {
        file.seek(qMax<qint64>(0, file.size() - 4096));
        tail = file.readAll();
    }
    removeStderrFile();
    return tail;
}

void AiCore::removeStderrFile()
{
    if (!m_stderrPath.isEmpty()) {
        QFile::remove(m_stderrPath);
        m_stderrPath.clear();
    }
}

void AiCore::handleExit()
{
    QString reason = QString::fromUtf8(takeStderrTail()).trimmed().section(QLatin1Char('\n'), -3);
    if (m_process && m_process->error() == QProcess::FailedToStart) {
        reason = tr("无法启动 GXDE AI Core：%1").arg(m_process->errorString());
    } else if (reason.isEmpty()) {
        reason = tr("GXDE AI Core 意外退出");
    } else {
        reason = tr("GXDE AI Core 意外退出：%1").arg(reason);
    }
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_initialized = false;
    failPending(reason);
    Q_EMIT stopped(reason);
}

void AiCore::failPending(const QString &error)
{
    m_queue.clear();
    const QHash<int, Callback> callbacks = m_callbacks;
    m_callbacks.clear();
    for (const Callback &callback : callbacks) {
        if (callback) {
            callback(QJsonValue(), error);
        }
    }
}
