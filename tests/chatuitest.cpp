// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless UI smoke test for chat attachments:
// drop files + a long text, grab a screenshot, then send and verify that the
// attachments were persisted in chat.json.

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QTimer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include "chat/attachment.h"
#include "chat/chatwindow.h"
#include "config.h"

static int failures = 0;

static void check(bool condition, const QString &what)
{
    QTextStream out(stdout);
    out << (condition ? "PASS " : "FAIL ") << what << "\n";
    out.flush();
    if (!condition) {
        ++failures;
    }
}

static void spin(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char *argv[])
{
    const QString configDir = QStringLiteral(SOURCE_DIR "/build/chatuitest-data");
    QDir(configDir).removeRecursively();
    qputenv("XDG_CONFIG_HOME", (configDir + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_DATA_HOME", (configDir + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CACHE_HOME", (configDir + QStringLiteral("/cache")).toUtf8());

    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));
    app.setWindowIcon(QIcon(QStringLiteral(SOURCE_DIR
                                           "/data/icons/hicolor/128x128/apps/gxde-ai-see.png")));
    app.setQuitOnLastWindowClosed(false);

    Config::instance()->init();
    Config::instance()->set(QStringLiteral("chat/apiKey"), QString());
    Config::instance()->set(QStringLiteral("chat/baseUrl"),
                            QStringLiteral("http://127.0.0.1:1"));
    Config::instance()->sync();

    QTemporaryDir media;
    check(media.isValid(), "media temp dir");
    if (!media.isValid()) {
        return 1;
    }

    QImage image(320, 200, QImage::Format_ARGB32);
    image.fill(QColor(58, 120, 210));
    const QString imagePath = media.path() + QStringLiteral("/screenshot.png");
    check(image.save(imagePath), "create image fixture");

    const QString textPath = media.path() + QStringLiteral("/notes.md");
    QFile textFile(textPath);
    check(textFile.open(QIODevice::WriteOnly), "create text fixture");
    textFile.write(QStringLiteral("# 会议记录\n\n- 传送门\n- 附件上传\n").toUtf8());
    textFile.close();

    ChatWindow window;
    window.resize(1040, 720);
    window.setAttribute(Qt::WA_DontShowOnScreen, true);
    window.show();
    spin(200);

    QMimeData files;
    files.setUrls({ QUrl::fromLocalFile(imagePath), QUrl::fromLocalFile(textPath) });
    window.handleDroppedMime(&files);

    QMimeData longText;
    longText.setText(QString(6000, QChar(0x9644)) + QStringLiteral(" long text"));
    window.handleDroppedMime(&longText);
    spin(300);

    const QString shot = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                  : QStringLiteral("/tmp/gxde-ai-see-chatui.png");
    check(window.grab().save(shot), "save chat UI screenshot");

    QPushButton *send = nullptr;
    for (QPushButton *button : window.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("发送")) {
            send = button;
            break;
        }
    }
    check(send != nullptr, "send button found");
    if (send) {
        check(send->isEnabled(), "send enabled by attachments alone");
        send->click();
        spin(800);
        const QString sentShot = shot + QStringLiteral(".sent.png");
        check(window.grab().save(sentShot), "save sent-message screenshot");
    }

    const QString storePath = Config::instance()->dataDir() + QStringLiteral("/chat.json");
    QFile store(storePath);
    check(store.open(QIODevice::ReadOnly), "chat store written");
    const QJsonDocument document = QJsonDocument::fromJson(store.readAll());
    const QJsonArray conversations =
        document.object().value(QStringLiteral("conversations")).toArray();
    check(!conversations.isEmpty(), "conversation persisted");
    int attachmentCount = 0;
    if (!conversations.isEmpty()) {
        const QJsonArray messages = conversations.first()
                                        .toObject()
                                        .value(QStringLiteral("messages"))
                                        .toArray();
        for (const QJsonValue &value : messages) {
            attachmentCount += value.toObject()
                                   .value(QStringLiteral("attachments"))
                                   .toArray()
                                   .size();
        }
    }
    check(attachmentCount == 3, "three attachments persisted in the sent message");

    bool serviceSwitchFound = false;
    QTimer::singleShot(400, [&serviceSwitchFound]() {
        QWidget *dialog = QApplication::activeModalWidget();
        check(dialog != nullptr, "chat settings dialog opens");
        if (!dialog) {
            return;
        }
        for (QCheckBox *box : dialog->findChildren<QCheckBox *>()) {
            if (box->text().contains(QStringLiteral("传送门服务"))) {
                serviceSwitchFound = true;
            }
        }
        check(dialog->grab().save(QStringLiteral("/tmp/gxde-ai-see-settings.png")),
              "chat settings rendered");
        dialog->close();
    });
    window.showSettings();
    check(serviceSwitchFound, "chat settings exposes the portal service switch");

    QTextStream(stdout) << (failures == 0 ? "ALL PASS\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
