// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless checks for the pure (non widget) parts of the content factory.

#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>
#include <QTranslator>

#include "actions/texttools.h"
#include "factory/contentclassifier.h"
#include "factory/factoryactions.h"
#include "factory/linktools.h"

static int failures = 0;

static void check(bool condition, const QString &what)
{
    QTextStream out(stdout);
    if (condition) {
        out << "PASS  " << what << "\n";
    } else {
        out << "FAIL  " << what << "\n";
        ++failures;
    }
}

static bool hasAction(const QVector<FactoryAction> &actions, const QString &id)
{
    for (const FactoryAction &action : actions) {
        if (action.id == id) {
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (qEnvironmentVariableIsSet("GXDE_AI_SEE_QM")) {
        auto *translator = new QTranslator(&app);
        if (translator->load(QString::fromLocal8Bit(qgetenv("GXDE_AI_SEE_QM")))) {
            app.installTranslator(translator);
            check(QCoreApplication::translate("PortalPanel", "暂存区")
                      == QStringLiteral("Staging"),
                  "english translation: PortalPanel/暂存区");
            check(QCoreApplication::translate("ChatWindow", "发送") == QStringLiteral("Send"),
                  "english translation: ChatWindow/发送");
            check(QCoreApplication::translate("TodoWindow", "待办") == QStringLiteral("Todo"),
                  "english translation: TodoWindow/待办");
        } else {
            check(false, "load english .qm");
        }
    }

    // http link with a video extension
    const LinkInfo video = LinkTools::parse(QStringLiteral("https://example.com/media/clip.mp4"));
    check(video.kind == LinkInfo::Http, "https link recognised");
    check(video.extension == QStringLiteral("mp4"), "mp4 extension parsed");
    check(video.fileType == QStringLiteral("video"), "mp4 classified as video");
    check(hasAction(actionsFor(ContentClassifier::classifyText(QStringLiteral(
                      "https://example.com/media/clip.mp4"))),
                    QStringLiteral("download")),
          "video link offers download");
    check(hasAction(actionsFor(ContentClassifier::classifyText(QStringLiteral(
                      "https://example.com/media/clip.mp4"))),
                    QStringLiteral("play")),
          "video link offers play");

    const LinkInfo image = LinkTools::parse(QStringLiteral("https://example.com/a.png?x=1"));
    check(image.fileType == QStringLiteral("image"), "png classified as image");

    const LinkInfo page = LinkTools::parse(QStringLiteral("https://example.com/article"));
    check(page.fileType == QStringLiteral("page"), "extensionless url is a page");

    // thunder:// -> base64("AA" + url + "ZZ")
    const QByteArray thunderPayload =
        QByteArray("AA") + "http://example.com/file.zip" + QByteArray("ZZ");
    const QString thunder =
        QStringLiteral("thunder://") + QString::fromLatin1(thunderPayload.toBase64());
    const LinkInfo thunderInfo = LinkTools::parse(thunder);
    check(thunderInfo.kind == LinkInfo::Custom, "thunder link classified as custom");
    check(thunderInfo.decodedUrl == QStringLiteral("http://example.com/file.zip"),
          "thunder payload decoded");
    check(hasAction(actionsFor(ContentClassifier::classifyText(thunder)),
                    QStringLiteral("decode")),
          "thunder offers decode action");

    // magnet
    const LinkInfo magnet =
        LinkTools::parse(QStringLiteral("magnet:?xt=urn:btih:abc&dn=Some%20File"));
    check(magnet.kind == LinkInfo::Custom, "magnet classified as custom");
    check(magnet.displayName == QStringLiteral("Some File"), "magnet display name parsed");

    // spk://
    const LinkInfo spk = LinkTools::parse(QStringLiteral("spk://store/app/com.example"));
    check(spk.kind == LinkInfo::Custom, "spk link classified as custom");

    // plain text is not a link
    const LinkInfo text = LinkTools::parse(QStringLiteral("hello world"));
    check(!text.isValid(), "plain text is not a link");

    // segmentation
    const QStringList words = TextTools::segment(QStringLiteral("我爱北京天安门"));
    check(words.size() >= 3, "segmentation produced words");
    check(words.contains(QStringLiteral("北京")), "segmentation keeps 北京 together");

    // link extraction
    const QStringList links = TextTools::extractLinks(
        QStringLiteral("看 https://a.example/x?y=1 和 test@example.com，还有 www.b.example"));
    check(links.size() == 3, QStringLiteral("extractLinks found 3 links, got %1").arg(links.size()));

    // classifier
    const DropContent textContent = ContentClassifier::classifyText(QStringLiteral("just words"));
    check(textContent.kind == DropContent::Text, "text classified as text");
    const DropContent linkContent =
        ContentClassifier::classifyText(QStringLiteral("https://example.com/a.zip"));
    check(linkContent.kind == DropContent::Link, "link classified as link");
    check(hasAction(actionsFor(linkContent), QStringLiteral("save_as")), "link offers save as");

    QTextStream(stdout) << (failures == 0 ? "ALL PASS\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
