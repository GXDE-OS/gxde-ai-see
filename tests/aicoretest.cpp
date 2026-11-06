// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// End-to-end chat test against the real GXDE AI Core.
//
// gxde-ai-core runs with HOME (and so its accounts, sessions and file-backed
// key store) in a temporary directory and talks to tests/api/fake_openai.py.
// Covered: account save/activate over ACP, tool approval, MCP servers handed
// to the session, session reuse vs. rebuild with a replayed history, the
// approval modes, cancelling a turn that waits for approval, the model's
// reasoning arriving apart from the answer and conversations answering with
// the account and model they picked.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include <functional>

#include "aicore.h"
#include "chatclient.h"
#include "config.h"

namespace {

QTextStream out(stdout);

struct Step
{
    QString name;
    QString conversation;
    QVector<ChatMessage> history;
    QString mode = QStringLiteral("default");
    bool cancelOnPermission = false;
    /* text is the whole streamed answer; return an error or an empty string */
    std::function<QString(const QString &text, int permissions)> check;
    /* accountId "other" stands for the second account */
    ChatProvider provider = {};
};

ChatMessage user(const QString &text)
{
    return ChatMessage{ QStringLiteral("user"), text };
}

ChatMessage assistant(const QString &text)
{
    return ChatMessage{ QStringLiteral("assistant"), text };
}

int usersIn(const QString &text)
{
    const QRegularExpressionMatch match =
        QRegularExpression(QStringLiteral("users=(\\d+)")).match(text);
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

} // namespace

int main(int argc, char *argv[])
{
    QTemporaryDir home;
    if (!home.isValid()) {
        out << "FAIL cannot create a temporary HOME\n";
        return 1;
    }
    /* isolate everything gxde-ai-core and the app would write */
    qputenv("HOME", home.path().toUtf8());
    qunsetenv("GXDE_AI_CORE_HOME");
    qputenv("GEMINI_FORCE_FILE_STORAGE", "true");
    qputenv("XDG_CONFIG_HOME", (home.path() + QStringLiteral("/.config")).toUtf8());
    qputenv("XDG_DATA_HOME", (home.path() + QStringLiteral("/.local/share")).toUtf8());
    qputenv("XDG_CACHE_HOME", (home.path() + QStringLiteral("/.cache")).toUtf8());

    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));
    Config::instance()->init();

    if (AiCore::program().isEmpty()) {
        out << "SKIP gxde-ai-core is not installed\n";
        return 77;
    }

    const QString testsDir = QStringLiteral(TESTS_DIR);
    QProcess server;
    server.start(QStringLiteral("python3"), { testsDir + QStringLiteral("/api/fake_openai.py") });
    int port = 0;
    QByteArray banner;
    while (port == 0 && server.waitForReadyRead(5000)) {
        banner += server.readAllStandardOutput();
        const QRegularExpressionMatch match =
            QRegularExpression(QStringLiteral("PORT (\\d+)")).match(QString::fromUtf8(banner));
        if (match.hasMatch()) {
            port = match.captured(1).toInt();
        }
    }
    if (port == 0) {
        out << "FAIL fake API did not start\n";
        return 1;
    }
    out << "fake API on port " << port << "\n";
    out.flush();

    Config::instance()->set(
        QStringLiteral("chat/mcpServers"),
        QStringLiteral("[{\"name\":\"test\",\"command\":\"python3\",\"args\":[\"%1/mcp/test_server.py\"]}]")
            .arg(testsDir));
    Config::instance()->sync();

    int exitCode = 0;
    auto finish = [&](bool ok, const QString &message) {
        out << (ok ? "PASS " : "FAIL ") << message << "\n";
        out.flush();
        if (!ok) {
            exitCode = 1;
        }
    };

    const QJsonObject other{
        { QStringLiteral("name"), QStringLiteral("other") },
        { QStringLiteral("providerId"), QStringLiteral("custom") },
        { QStringLiteral("protocol"), QStringLiteral("openai") },
        { QStringLiteral("baseUrl"), QStringLiteral("http://127.0.0.1:%1/v1").arg(port) },
        { QStringLiteral("models"),
          QJsonArray{ QJsonObject{ { QStringLiteral("id"), QStringLiteral("other-a") } },
                      QJsonObject{ { QStringLiteral("id"), QStringLiteral("other-b") } } } },
        { QStringLiteral("model"), QStringLiteral("other-a") },
    };
    QString otherId;

    const QJsonObject account{
        { QStringLiteral("name"), QStringLiteral("fake") },
        { QStringLiteral("providerId"), QStringLiteral("custom") },
        { QStringLiteral("protocol"), QStringLiteral("openai") },
        { QStringLiteral("baseUrl"), QStringLiteral("http://127.0.0.1:%1/v1").arg(port) },
        { QStringLiteral("models"), QJsonArray{ QJsonObject{ { QStringLiteral("id"),
                                                               QStringLiteral("fake-model") } } } },
        { QStringLiteral("model"), QStringLiteral("fake-model") },
    };

    ChatClient client;
    /* reasoning of the running step, see ChatClient::thought */
    QString thoughts;
    QVector<Step> steps;
    int stepsUsers = -1;

    steps.append(Step{
        QStringLiteral("shell tool asks first, runs after approval"),
        QStringLiteral("c1"),
        { user(QStringLiteral("please use the shell")) },
        QStringLiteral("default"),
        false,
        [&](const QString &text, int permissions) -> QString {
            if (permissions != 1) {
                return QStringLiteral("expected 1 permission request, got %1").arg(permissions);
            }
            if (!QFile::exists(home.path() + QStringLiteral("/touched_by_aicoretest"))) {
                return QStringLiteral("the approved command did not run");
            }
            return text.contains(QStringLiteral("完成")) ? QString()
                                                          : QStringLiteral("answer: ") + text;
        } });
    steps.append(Step{
        QStringLiteral("next turn reuses the session"),
        QStringLiteral("c1"),
        { user(QStringLiteral("please use the shell")), assistant(QStringLiteral("完成：")),
          user(QStringLiteral("plain question")) },
        QStringLiteral("default"),
        false,
        [&](const QString &text, int) -> QString {
            stepsUsers = usersIn(text);
            if (stepsUsers < 2 || !text.contains(QStringLiteral("replay=no"))) {
                return QStringLiteral("answer: ") + text;
            }
            return QString();
        } });
    steps.append(Step{
        QStringLiteral("edited history rebuilds the session with a replay"),
        QStringLiteral("c1"),
        { user(QStringLiteral("please use the shell")), assistant(QStringLiteral("完成：")),
          user(QStringLiteral("edited question")) },
        QStringLiteral("default"),
        false,
        [&](const QString &text, int) -> QString {
            const int users = usersIn(text);
            if (!text.contains(QStringLiteral("replay=yes")) || users < 1 || users >= stepsUsers) {
                return QStringLiteral("answer: %1 (previous users=%2)").arg(text).arg(stepsUsers);
            }
            return QString();
        } });
    steps.append(Step{
        QStringLiteral("configured MCP server reaches the session"),
        QStringLiteral("c2"),
        { user(QStringLiteral("call the mcp echo tool")) },
        QStringLiteral("default"),
        false,
        [&](const QString &text, int) -> QString {
            return text.contains(QStringLiteral("echo: hello")) ? QString()
                                                                 : QStringLiteral("answer: ") + text;
        } });
    steps.append(Step{
        QStringLiteral("yolo mode runs tools without asking"),
        QStringLiteral("c3"),
        { user(QStringLiteral("shell again")) },
        QStringLiteral("yolo"),
        false,
        [&](const QString &text, int permissions) -> QString {
            if (permissions != 0) {
                return QStringLiteral("asked %1 times in yolo mode").arg(permissions);
            }
            return text.contains(QStringLiteral("完成")) ? QString()
                                                          : QStringLiteral("answer: ") + text;
        } });
    steps.append(Step{
        QStringLiteral("cancelling while approval is pending"),
        QStringLiteral("c4"),
        { user(QStringLiteral("shell then cancel")) },
        QStringLiteral("default"),
        true,
        {} });
    steps.append(Step{
        QStringLiteral("conversation keeps working after a cancel"),
        QStringLiteral("c4"),
        { user(QStringLiteral("shell then cancel")), assistant(QString()),
          user(QStringLiteral("plain after cancel")) },
        QStringLiteral("default"),
        false,
        [&](const QString &text, int) -> QString {
            return text.contains(QStringLiteral("replay=")) ? QString()
                                                            : QStringLiteral("answer: ") + text;
        } });
    steps.append(Step{
        QStringLiteral("reasoning streams as thoughts, apart from the answer"),
        QStringLiteral("c5"),
        { user(QStringLiteral("think first")) },
        QStringLiteral("default"),
        false,
        [&](const QString &answer, int) -> QString {
            if (!thoughts.contains(QStringLiteral("先想一想"))) {
                return QStringLiteral("thoughts: ") + thoughts;
            }
            return answer.trimmed() == QStringLiteral("想好了") ? QString()
                                                                : QStringLiteral("answer: ") + answer;
        } });

    steps.append(Step{
        QStringLiteral("a conversation answers with the account it picked"),
        QStringLiteral("c6"),
        { user(QStringLiteral("which model")) },
        QStringLiteral("default"),
        false,
        [&](const QString &text, int) -> QString {
            return text.contains(QStringLiteral("model=other-a")) ? QString()
                                                                  : QStringLiteral("answer: ") + text;
        },
        { QStringLiteral("other"), QStringLiteral("other-a") } });
    steps.append(Step{
        QStringLiteral("picking another model keeps the session"),
        QStringLiteral("c6"),
        { user(QStringLiteral("which model")), assistant(QStringLiteral("model=other-a")),
          user(QStringLiteral("which model now")) },
        QStringLiteral("default"),
        false,
        [&](const QString &text, int) -> QString {
            return text.contains(QStringLiteral("model=other-b")) &&
                           text.contains(QStringLiteral("replay=no"))
                       ? QString()
                       : QStringLiteral("answer: ") + text;
        },
        { QStringLiteral("other"), QStringLiteral("other-b") } });
    steps.append(Step{
        QStringLiteral("other conversations stay on the default account"),
        QStringLiteral("c7"),
        { user(QStringLiteral("which model")) },
        QStringLiteral("default"),
        false,
        [&](const QString &text, int) -> QString {
            return text.contains(QStringLiteral("model=fake-model")) ? QString()
                                                                     : QStringLiteral("answer: ") + text;
        } });

    int current = -1;
    QString text;
    int permissions = 0;
    std::function<void()> next;

    QObject::connect(&client, &ChatClient::delta, [&](const QString &chunk) { text += chunk; });
    QObject::connect(&client, &ChatClient::thought,
                     [&](const QString &chunk) { thoughts += chunk; });
    QObject::connect(&client, &ChatClient::toolCall,
                     [&](const QString &, const QString &title, const QString &,
                         const QString &status) {
                         out << "  tool " << title << " " << status << "\n";
                     });
    QObject::connect(&client, &ChatClient::permissionRequested,
                     [&](const QString &key, const AiCorePermission &request) {
                         ++permissions;
                         out << "  permission for "
                             << request.toolCall.value(QStringLiteral("title")).toString() << "\n";
                         const Step &step = steps.at(current);
                         if (step.cancelOnPermission) {
                             client.cancel();
                             /* nothing may come back for the cancelled turn */
                             QTimer::singleShot(1500, &app, [&, at = current, name = step.name]() {
                                 if (current != at) {
                                     return;
                                 }
                                 finish(!client.busy(), name);
                                 next();
                             });
                             return;
                         }
                         for (const QJsonObject &option : request.options) {
                             if (option.value(QStringLiteral("kind")).toString() ==
                                 QStringLiteral("allow_once")) {
                                 client.answerPermission(
                                     key, option.value(QStringLiteral("optionId")).toString());
                                 return;
                             }
                         }
                         client.answerPermission(key, QString());
                     });
    QObject::connect(&client, &ChatClient::finished, [&](const QString &stopReason) {
        const Step &step = steps.at(current);
        if (step.cancelOnPermission) {
            finish(false, step.name + QStringLiteral(": turn finished (%1)").arg(stopReason));
            next();
            return;
        }
        const QString problem = step.check(text, permissions);
        finish(problem.isEmpty(),
               problem.isEmpty() ? step.name : step.name + QStringLiteral(": ") + problem);
        next();
    });
    QObject::connect(&client, &ChatClient::failed, [&](const QString &error) {
        finish(false, steps.at(current).name + QStringLiteral(": failed: ") + error);
        next();
    });

    next = [&]() {
        ++current;
        if (current >= steps.size()) {
            app.quit();
            return;
        }
        const Step &step = steps.at(current);
        text.clear();
        thoughts.clear();
        permissions = 0;
        out << "step " << step.name << "\n";
        out.flush();
        client.setMode(step.mode);
        ChatProvider provider = step.provider;
        if (provider.accountId == QStringLiteral("other")) {
            provider.accountId = otherId;
        }
        client.send(step.conversation, step.history, provider);
    };

    AiCore::instance()->saveAccount(
        account, QStringLiteral("sk-test"), [&](const QJsonValue &saved, const QString &error) {
            if (!error.isEmpty()) {
                finish(false, QStringLiteral("save account: ") + error);
                app.quit();
                return;
            }
            const QString id = saved.toObject()
                                   .value(QStringLiteral("account"))
                                   .toObject()
                                   .value(QStringLiteral("id"))
                                   .toString();
            AiCore::instance()->activateAccount(
                id, QString(), [&](const QJsonValue &, const QString &activateError) {
                    finish(activateError.isEmpty(),
                           QStringLiteral("account saved and activated over ACP ") + activateError);
                    if (!activateError.isEmpty()) {
                        app.quit();
                        return;
                    }
                    /* a second account, not the default, for conversations to pick */
                    AiCore::instance()->saveAccount(
                        other, QStringLiteral("sk-other"),
                        [&](const QJsonValue &savedOther, const QString &otherError) {
                            otherId = savedOther.toObject()
                                          .value(QStringLiteral("account"))
                                          .toObject()
                                          .value(QStringLiteral("id"))
                                          .toString();
                            finish(otherError.isEmpty(),
                                   QStringLiteral("second account saved ") + otherError);
                            if (!otherError.isEmpty()) {
                                app.quit();
                                return;
                            }
                            next();
                        });
                });
        });

    QTimer::singleShot(180000, &app, [&]() {
        finish(false, QStringLiteral("timeout"));
        app.quit();
    });

    app.exec();
    AiCore::instance()->stop();
    server.kill();
    server.waitForFinished(1000);
    return exitCode;
}
