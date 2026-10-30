// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// End-to-end chat + MCP test against a fake OpenAI compatible server:
// request -> streaming tool call -> MCP tools/call -> follow-up request -> text.

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include "chat/chatclient.h"
#include "chat/mcpclient.h"
#include "config.h"

static QTextStream out(stdout);
static int exitCode = 1;

static void finish(QCoreApplication *app, bool ok, const QString &message)
{
    out << (ok ? "PASS " : "FAIL ") << message << "\n";
    out.flush();
    exitCode = ok ? 0 : 1;
    app->quit();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));

    QTemporaryDir configDir;
    if (!configDir.isValid()) {
        finish(&app, false, QStringLiteral("cannot create temp config dir"));
        return exitCode;
    }
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());
    qputenv("XDG_DATA_HOME", configDir.path().toUtf8());
    qputenv("XDG_CACHE_HOME", configDir.path().toUtf8());

    const QString testsDir = QStringLiteral(TESTS_DIR);

    QProcess server;
    server.setProcessChannelMode(QProcess::MergedChannels);
    server.start(QStringLiteral("python3"),
                 { testsDir + QStringLiteral("/api/fake_openai.py") });
    if (!server.waitForStarted(5000)) {
        finish(&app, false, QStringLiteral("fake API server did not start"));
        return exitCode;
    }

    QByteArray output;
    int port = 0;
    while (port == 0) {
        if (!server.waitForReadyRead(5000)) {
            break;
        }
        output += server.readAll();
        const int index = output.indexOf("PORT ");
        if (index >= 0) {
            const int end = output.indexOf('\n', index);
            if (end > index) {
                port = output.mid(index + 5, end - index - 5).trimmed().toInt();
            }
        }
    }
    if (port == 0) {
        finish(&app, false, QStringLiteral("cannot read fake API port"));
        return exitCode;
    }
    out << "fake API port " << port << "\n";

    Config::instance()->init();
    Config::instance()->set(QStringLiteral("chat/apiKey"), QStringLiteral("test"));
    Config::instance()->set(QStringLiteral("chat/baseUrl"),
                            QStringLiteral("http://127.0.0.1:%1").arg(port));
    Config::instance()->set(QStringLiteral("chat/model"), QStringLiteral("test-model"));
    Config::instance()->set(
        QStringLiteral("chat/mcpServers"),
        QStringLiteral("[{\"name\":\"test\",\"command\":\"python3\",\"args\":[\"%1/mcp/test_server.py\"]}]")
            .arg(testsDir));
    Config::instance()->sync();

    McpManager::instance()->reload();
    ChatClient client;

    QObject::connect(&client, &ChatClient::failed, [&](const QString &error) {
        finish(&app, false, QStringLiteral("chat failed: %1").arg(error));
    });

    QObject::connect(&client, &ChatClient::toolCallsReady,
                     [&](const QVector<ToolCall> &calls) {
                         if (calls.isEmpty()) {
                             finish(&app, false, QStringLiteral("empty tool call"));
                             return;
                         }
                         out << "tool call " << calls.first().name << " "
                             << calls.first().arguments << "\n";
                         const ToolCall call = calls.first();
                         McpManager::instance()->callTool(
                             call.name,
                             QJsonObject{ { QStringLiteral("text"), QStringLiteral("hello") } },
                             [&, call](const QJsonObject &result) {
                                 QString text;
                                 for (const QJsonValue &value :
                                      result.value(QStringLiteral("content")).toArray()) {
                                     text += value.toObject()
                                                 .value(QStringLiteral("text"))
                                                 .toString();
                                 }
                                 out << "tool result " << text << "\n";
                                 if (!text.contains(QStringLiteral("echo: hello"))) {
                                     finish(&app, false, QStringLiteral("tool result mismatch"));
                                     return;
                                 }

                                 QJsonObject assistant;
                                 assistant.insert(QStringLiteral("role"),
                                                  QStringLiteral("assistant"));
                                 assistant.insert(QStringLiteral("content"), QJsonValue::Null);
                                 QJsonObject function;
                                 function.insert(QStringLiteral("name"), call.name);
                                 function.insert(QStringLiteral("arguments"), call.arguments);
                                 assistant.insert(
                                     QStringLiteral("tool_calls"),
                                     QJsonArray{ QJsonObject{
                                         { QStringLiteral("id"), call.id },
                                         { QStringLiteral("type"), QStringLiteral("function") },
                                         { QStringLiteral("function"), function } } });
                                 QJsonObject toolMessage{
                                     { QStringLiteral("role"), QStringLiteral("tool") },
                                     { QStringLiteral("tool_call_id"), call.id },
                                     { QStringLiteral("content"), text },
                                 };
                                 client.send(
                                     { ChatMessage{ QStringLiteral("user"),
                                                    QStringLiteral("hi") } },
                                     QJsonArray{ assistant, toolMessage });
                             });
                     });

    QObject::connect(&client, &ChatClient::finished, [&](const QString &text) {
        out << "answer " << text << "\n";
        finish(&app, text.contains(QStringLiteral("echo: hello")),
               QStringLiteral("tool loop end to end"));
    });

    QTimer::singleShot(20000, &app, [&]() {
        finish(&app, false, QStringLiteral("timeout"));
    });

    auto *poller = new QTimer(&app);
    poller->setInterval(100);
    QObject::connect(poller, &QTimer::timeout, [&]() {
        if (!McpManager::instance()->hasTools()) {
            return;
        }
        poller->stop();
        out << "mcp tools ready\n";
        client.setTools(McpManager::instance()->toolDefinitions());
        client.send({ ChatMessage{ QStringLiteral("user"), QStringLiteral("hi") } });
    });
    poller->start();

    const int rc = app.exec();
    if (server.state() != QProcess::NotRunning) {
        server.kill();
        server.waitForFinished(1000);
    }
    return exitCode != 0 ? exitCode : rc;
}
