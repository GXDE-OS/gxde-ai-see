// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>

#include <functional>

class QProcess;

/**
 * One MCP server speaking JSON-RPC 2.0 over stdio.
 */
class McpServer : public QObject
{
    Q_OBJECT

public:
    McpServer(const QString &name, const QString &command, const QStringList &args,
              QObject *parent = nullptr);
    ~McpServer() override;

    void start();

    QString name() const { return m_name; }
    bool ready() const { return m_ready; }
    QJsonArray tools() const { return m_tools; }

    void callTool(const QString &tool, const QJsonObject &arguments,
                  std::function<void(const QJsonObject &)> callback);

signals:
    void readyChanged();

private:
    void send(const QJsonObject &message);
    void handleLine(const QByteArray &line);
    void finishToolList();

    QString m_name;
    QString m_command;
    QStringList m_args;
    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    int m_nextId = 1;
    QHash<int, std::function<void(const QJsonObject &)>> m_callbacks;
    QJsonArray m_tools;
    bool m_ready = false;
};

/**
 * Aggregates the configured MCP servers and exposes their tools in the
 * OpenAI function-calling format.
 */
class McpManager : public QObject
{
    Q_OBJECT

public:
    static McpManager *instance();

    void reload();

    bool hasTools() const;
    QJsonArray toolDefinitions() const;
    QStringList statusLines() const;

    void callTool(const QString &exposedName, const QJsonObject &arguments,
                  std::function<void(const QJsonObject &)> callback);

signals:
    void toolsChanged();

private:
    explicit McpManager(QObject *parent = nullptr);
    void clearServers();
    void rebuild();

    QList<McpServer *> m_servers;
    struct ToolRoute
    {
        McpServer *server = nullptr;
        QString originalName;
    };
    QHash<QString, ToolRoute> m_routes;
    QJsonArray m_definitions;
    QStringList m_status;
};
