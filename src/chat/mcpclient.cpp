// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mcpclient.h"

#include <QDebug>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

#include "config.h"

McpServer::McpServer(const QString &name, const QString &command, const QStringList &args,
                     QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_command(command)
    , m_args(args)
{
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_buffer.append(m_process->readAllStandardOutput());
        int newline;
        while ((newline = m_buffer.indexOf('\n')) >= 0) {
            QByteArray line = m_buffer.left(newline);
            m_buffer.remove(0, newline + 1);
            line = line.trimmed();
            if (!line.isEmpty()) {
                handleLine(line);
            }
        }
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray error = m_process->readAllStandardError();
        if (!error.isEmpty()) {
            qWarning().noquote() << QStringLiteral("[mcp:%1]").arg(m_name)
                                 << QString::fromUtf8(error).trimmed();
        }
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        qWarning().noquote() << QStringLiteral("[mcp:%1] process error %2").arg(m_name).arg(error);
    });
}

McpServer::~McpServer()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
            m_process->waitForFinished(500);
        }
    }
}

void McpServer::start()
{
    m_process->start(m_command, m_args);
    if (!m_process->waitForStarted(3000)) {
        qWarning().noquote() << QStringLiteral("[mcp:%1] failed to start %2").arg(m_name, m_command);
        return;
    }

    const int initId = m_nextId++;
    m_callbacks.insert(initId, [this](const QJsonObject &) {
        send(QJsonObject{ { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
                          { QStringLiteral("method"),
                            QStringLiteral("notifications/initialized") } });

        const int toolsId = m_nextId++;
        m_callbacks.insert(toolsId, [this](const QJsonObject &response) {
            m_tools = response.value(QStringLiteral("tools")).toArray();
            m_ready = true;
            Q_EMIT readyChanged();
        });
        send(QJsonObject{ { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
                          { QStringLiteral("id"), toolsId },
                          { QStringLiteral("method"), QStringLiteral("tools/list") },
                          { QStringLiteral("params"), QJsonObject{} } });
    });

    QJsonObject params{
        { QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05") },
        { QStringLiteral("capabilities"), QJsonObject{} },
        { QStringLiteral("clientInfo"),
          QJsonObject{ { QStringLiteral("name"), QStringLiteral("gxde-ai-see") },
                       { QStringLiteral("version"), QStringLiteral("0.1.0") } } },
    };
    send(QJsonObject{ { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
                      { QStringLiteral("id"), initId },
                      { QStringLiteral("method"), QStringLiteral("initialize") },
                      { QStringLiteral("params"), params } });
}

void McpServer::send(const QJsonObject &message)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        return;
    }
    QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact);
    data.append('\n');
    m_process->write(data);
}

void McpServer::handleLine(const QByteArray &line)
{
    const QJsonDocument document = QJsonDocument::fromJson(line);
    if (!document.isObject()) {
        return;
    }
    const QJsonObject object = document.object();

    if (object.contains(QStringLiteral("id"))) {
        const int id = object.value(QStringLiteral("id")).toInt(-1);
        auto callback = m_callbacks.take(id);
        if (!callback) {
            return;
        }
        if (object.contains(QStringLiteral("error"))) {
            const QJsonObject error = object.value(QStringLiteral("error")).toObject();
            callback(QJsonObject{ { QStringLiteral("error"),
                                   error.value(QStringLiteral("message"))
                                       .toString(QStringLiteral("MCP 调用失败")) } });
        } else {
            callback(object.value(QStringLiteral("result")).toObject());
        }
    }
}

void McpServer::callTool(const QString &tool, const QJsonObject &arguments,
                         std::function<void(const QJsonObject &)> callback)
{
    if (!m_ready) {
        QJsonObject error{ { QStringLiteral("error"),
                             QStringLiteral("MCP 服务器尚未就绪") } };
        if (callback) {
            callback(error);
        }
        return;
    }

    const int id = m_nextId++;
    m_callbacks.insert(id, std::move(callback));
    QJsonObject params{ { QStringLiteral("name"), tool },
                        { QStringLiteral("arguments"), arguments } };
    send(QJsonObject{ { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
                      { QStringLiteral("id"), id },
                      { QStringLiteral("method"), QStringLiteral("tools/call") },
                      { QStringLiteral("params"), params } });
}

McpManager *McpManager::instance()
{
    static McpManager *self = new McpManager;
    return self;
}

McpManager::McpManager(QObject *parent)
    : QObject(parent)
{
}

void McpManager::clearServers()
{
    for (McpServer *server : m_servers) {
        server->deleteLater();
    }
    m_servers.clear();
}

void McpManager::reload()
{
    clearServers();
    m_routes.clear();
    m_definitions = QJsonArray();
    m_status.clear();

    const QString json =
        Config::instance()->get(QStringLiteral("chat/mcpServers")).toString().trimmed();
    if (!json.isEmpty()) {
        const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
        const QJsonArray servers = document.isArray() ? document.array() : QJsonArray();
        for (const QJsonValue &value : servers) {
            const QJsonObject object = value.toObject();
            const QString command = object.value(QStringLiteral("command")).toString();
            if (command.isEmpty()) {
                continue;
            }
            QString name = object.value(QStringLiteral("name")).toString();
            if (name.isEmpty()) {
                name = QStringLiteral("mcp");
            }
            QStringList args;
            for (const QJsonValue &arg : object.value(QStringLiteral("args")).toArray()) {
                args.append(arg.toString());
            }
            auto *server = new McpServer(name, command, args, this);
            m_servers.append(server);
            connect(server, &McpServer::readyChanged, this, [this]() { rebuild(); });
            server->start();
        }
    }

    rebuild();
}

void McpManager::rebuild()
{
    m_definitions = QJsonArray();
    m_routes.clear();
    m_status.clear();

    QSet<QString> used;
    for (McpServer *server : m_servers) {
        const QJsonArray tools = server->tools();
        m_status.append(QStringLiteral("%1: %2 个工具%3")
                            .arg(server->name())
                            .arg(tools.size())
                            .arg(server->ready() ? QString() : QStringLiteral("（连接中）")));
        for (const QJsonValue &value : tools) {
            const QJsonObject tool = value.toObject();
            QString exposed = tool.value(QStringLiteral("name")).toString();
            exposed.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
                            QStringLiteral("_"));
            if (exposed.isEmpty()) {
                continue;
            }
            if (used.contains(exposed)) {
                exposed = server->name() + QLatin1Char('_') + exposed;
                exposed.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
                                QStringLiteral("_"));
            }
            while (used.contains(exposed)) {
                exposed += QLatin1Char('_');
            }
            used.insert(exposed);

            m_routes.insert(exposed, ToolRoute{ server, tool.value(QStringLiteral("name")).toString() });

            QJsonObject function;
            function.insert(QStringLiteral("name"), exposed);
            function.insert(QStringLiteral("description"),
                            tool.value(QStringLiteral("description")).toString());
            QJsonObject parameters = tool.value(QStringLiteral("inputSchema")).toObject();
            if (parameters.isEmpty()) {
                parameters.insert(QStringLiteral("type"), QStringLiteral("object"));
                parameters.insert(QStringLiteral("properties"), QJsonObject{});
            }
            function.insert(QStringLiteral("parameters"), parameters);
            m_definitions.append(
                QJsonObject{ { QStringLiteral("type"), QStringLiteral("function") },
                             { QStringLiteral("function"), function } });
        }
    }

    Q_EMIT toolsChanged();
}

bool McpManager::hasTools() const
{
    return !m_definitions.isEmpty();
}

QJsonArray McpManager::toolDefinitions() const
{
    return m_definitions;
}

QStringList McpManager::statusLines() const
{
    return m_status;
}

void McpManager::callTool(const QString &exposedName, const QJsonObject &arguments,
                          std::function<void(const QJsonObject &)> callback)
{
    const auto it = m_routes.constFind(exposedName);
    if (it == m_routes.constEnd()) {
        if (callback) {
            callback(QJsonObject{ { QStringLiteral("error"),
                                   QStringLiteral("未找到工具 %1").arg(exposedName) } });
        }
        return;
    }
    it->server->callTool(it->originalName, arguments, std::move(callback));
}
