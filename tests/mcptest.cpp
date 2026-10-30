// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless MCP smoke test: starts the given command as an MCP server, waits for
// tools/list and calls the first tool once.

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

#include "chat/mcpclient.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    if (argc < 3) {
        out << "usage: " << argv[0] << " <command> <script> [args...]\n";
        return 2;
    }

    QStringList args;
    for (int i = 2; i < argc; ++i) {
        args.append(QString::fromLocal8Bit(argv[i]));
    }

    McpServer server(QStringLiteral("test"), QString::fromLocal8Bit(argv[1]), args);

    QObject::connect(&server, &McpServer::readyChanged, [&]() {
        out << "READY tools=" << server.tools().size() << "\n";
        if (server.tools().isEmpty()) {
            app.exit(1);
            return;
        }
        const QString name =
            server.tools().first().toObject().value(QStringLiteral("name")).toString();
        out << "TOOL " << name << "\n";
        server.callTool(name, QJsonObject{ { QStringLiteral("text"),
                                             QStringLiteral("hello") } },
                        [&](const QJsonObject &result) {
                            out << "RESULT "
                                << QString::fromUtf8(
                                       QJsonDocument(result).toJson(QJsonDocument::Compact))
                                << "\n";
                            out.flush();
                            app.quit();
                        });
    });

    server.start();
    QTimer::singleShot(8000, &app, [&]() {
        out << "TIMEOUT\n";
        out.flush();
        app.exit(1);
    });

    return app.exec();
}
