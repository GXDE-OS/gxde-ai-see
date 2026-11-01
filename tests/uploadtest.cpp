// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// End-to-end check for chat attachment uploads against a local HTTP server:
// Files API multipart upload -> file_id -> Chat Completions file block.

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include "chat/attachment.h"
#include "chat/attachmentuploader.h"
#include "chat/chatclient.h"
#include "config.h"

static int failures = 0;
static QTextStream out(stdout);

static void check(bool condition, const QString &what)
{
    out << (condition ? "PASS " : "FAIL ") << what << "\n";
    out.flush();
    if (!condition) {
        ++failures;
    }
}

class FakeApiServer : public QTcpServer
{
public:
    QByteArray filesRequest;
    QByteArray filesBody;
    QByteArray chatRequest;
    QByteArray chatBody;

protected:
    void incomingConnection(qintptr descriptor) override
    {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(descriptor);
        auto buffer = QSharedPointer<QByteArray>::create();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
            buffer->append(socket->readAll());
            const int headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                return;
            }
            const QByteArray headers = buffer->left(headerEnd);
            const QRegularExpression lengthExpression(
                QStringLiteral("Content-Length:\\s*(\\d+)"),
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch match =
                lengthExpression.match(QString::fromLatin1(headers));
            const int length = match.hasMatch() ? match.captured(1).toInt() : 0;
            if (buffer->size() < headerEnd + 4 + length) {
                return;
            }

            const QByteArray requestLine = headers.left(headers.indexOf("\r\n"));
            const QByteArray body = buffer->mid(headerEnd + 4, length);
            buffer->clear();

            if (requestLine.contains("/files")) {
                filesRequest = headers;
                filesBody = body;
                respond(socket,
                        QByteArrayLiteral("application/json"),
                        QByteArrayLiteral("{\"id\":\"file-api-test\",\"object\":\"file\","
                                          "\"bytes\":123,\"created_at\":1,"
                                          "\"filename\":\"pic.png\",\"purpose\":\"user_data\"}"));
            } else {
                chatRequest = headers;
                chatBody = body;
                respond(socket, QByteArrayLiteral("text/event-stream"),
                        QByteArrayLiteral("data: {\"choices\":[{\"index\":0,"
                                          "\"delta\":{\"role\":\"assistant\","
                                          "\"content\":\"ok\"}}]}\n\n"
                                          "data: [DONE]\n\n"));
            }
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }

private:
    static void respond(QTcpSocket *socket, const QByteArray &type, const QByteArray &body)
    {
        QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: " + type +
            "\r\nContent-Length: " + QByteArray::number(body.size()) +
            "\r\nConnection: close\r\n\r\n" + body;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));

    QTemporaryDir configDir;
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());
    qputenv("XDG_DATA_HOME", configDir.path().toUtf8());
    qputenv("XDG_CACHE_HOME", configDir.path().toUtf8());
    Config::instance()->init();

    FakeApiServer server;
    check(server.listen(QHostAddress::LocalHost, 0), "fake API server listening");
    Config::instance()->set(QStringLiteral("chat/apiKey"), QStringLiteral("test-key"));
    Config::instance()->set(QStringLiteral("chat/baseUrl"),
                            QStringLiteral("http://127.0.0.1:%1")
                                .arg(server.serverPort()));
    Config::instance()->sync();

    QImage image(64, 48, QImage::Format_ARGB32);
    image.fill(QColor(200, 40, 40));
    const QString imagePath = configDir.path() + QStringLiteral("/input.png");
    check(image.save(imagePath), "image fixture");

    ChatAttachment attachment =
        AttachmentUtils::fromFile(imagePath, configDir.path() + QStringLiteral("/store"));
    check(attachment.kind == ChatAttachment::Image, "image attachment created");

    AttachmentUploader uploader;
    QString uploadedId;
    QString uploadError;
    QEventLoop uploadLoop;
    QObject::connect(&uploader, &AttachmentUploader::uploaded,
                     &uploadLoop, [&](const QString &id, const QString &fileId) {
                         uploadedId = id;
                         attachment.fileId = fileId;
                         uploadLoop.quit();
                     });
    QObject::connect(&uploader, &AttachmentUploader::failed,
                     &uploadLoop, [&](const QString &, const QString &error) {
                         uploadError = error;
                         uploadLoop.quit();
                     });
    uploader.upload(attachment);
    QTimer::singleShot(5000, &uploadLoop, &QEventLoop::quit);
    uploadLoop.exec();

    check(uploadedId == attachment.id, "uploader reported the local attachment id");
    check(attachment.fileId == QStringLiteral("file-api-test"), "Files API file_id parsed");
    check(uploadError.isEmpty(), "upload did not fail: " + uploadError);
    check(server.filesRequest.startsWith("POST /files"),
          "upload used POST /files");
    check(server.filesBody.contains("name=\"purpose\""), "purpose field sent");
    check(server.filesBody.contains("name=\"file\""), "file field sent");
    check(server.filesBody.contains(QByteArrayLiteral("\x89PNG\r\n\x1a\n")),
          "multipart body contains the image bytes");

    ChatMessage message;
    message.role = QStringLiteral("user");
    message.content = QStringLiteral("这是什么？");
    message.attachments = { attachment };

    ChatClient client;
    QString answer;
    QEventLoop chatLoop;
    QObject::connect(&client, &ChatClient::finished, &chatLoop, [&](const QString &text) {
        answer = text;
        chatLoop.quit();
    });
    QObject::connect(&client, &ChatClient::failed, &chatLoop, [&](const QString &error) {
        uploadError = error;
        chatLoop.quit();
    });
    client.send({ message });
    QTimer::singleShot(5000, &chatLoop, &QEventLoop::quit);
    chatLoop.exec();

    check(answer == QStringLiteral("ok"), "streaming answer received");
    const QJsonDocument chatDocument = QJsonDocument::fromJson(server.chatBody);
    const QJsonObject payload = chatDocument.object();
    const QJsonArray messages = payload.value(QStringLiteral("messages")).toArray();
    check(messages.size() == 1, "one message in the API payload");
    if (!messages.isEmpty()) {
        const QJsonValue content = messages.first().toObject().value(QStringLiteral("content"));
        check(content.isArray(), "message content is a block array");
        bool hasText = false;
        bool hasFile = false;
        for (const QJsonValue &value : content.toArray()) {
            const QJsonObject block = value.toObject();
            if (block.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
                hasText = block.value(QStringLiteral("text")).toString().contains(
                    QStringLiteral("这是什么？"));
            }
            if (block.value(QStringLiteral("type")).toString() == QStringLiteral("file") &&
                block.value(QStringLiteral("file_id")).toString() ==
                    QStringLiteral("file-api-test")) {
                hasFile = true;
            }
        }
        check(hasText, "text block preserved");
        check(hasFile, "uploaded image referenced by file block");
    }

    out << (failures == 0 ? "ALL PASS\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
