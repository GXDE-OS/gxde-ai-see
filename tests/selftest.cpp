// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless checks for the pure (non widget) parts of the content factory.

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTranslator>
#include <QtMath>

#include "actions/texttools.h"
#include "chat/attachment.h"
#include "chat/chattheme.h"
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

/** Composite @p foreground (possibly translucent) over @p background. */
static QColor over(const QColor &foreground, const QColor &background)
{
    const qreal alpha = foreground.alphaF();
    return QColor(qRound(foreground.red() * alpha + background.red() * (1 - alpha)),
                  qRound(foreground.green() * alpha + background.green() * (1 - alpha)),
                  qRound(foreground.blue() * alpha + background.blue() * (1 - alpha)));
}

static qreal relativeLuminance(const QColor &color)
{
    const auto channel = [](int value) {
        const qreal c = value / 255.0;
        return c <= 0.03928 ? c / 12.92 : qPow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.red()) + 0.7152 * channel(color.green()) +
        0.0722 * channel(color.blue());
}

static qreal contrastRatio(const QColor &foreground, const QColor &background)
{
    const qreal a = relativeLuminance(over(foreground, background));
    const qreal b = relativeLuminance(background);
    return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
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

    // chat theme palettes (light / dark following DTK)
    const ChatPalette darkTheme = ChatTheme::buildPalette(true);
    const ChatPalette lightTheme = ChatTheme::buildPalette(false);
    check(darkTheme.dark && !lightTheme.dark, "palette builder reports the requested mode");
    check(darkTheme.window.lightness() < 80, "dark palette uses a dark window background");
    check(lightTheme.window.lightness() > 200, "light palette uses a light window background");
    check(darkTheme.textPrimary != lightTheme.textPrimary, "text colour differs per theme");
    check(darkTheme.accentStrong != lightTheme.accentStrong, "accent differs per theme");

    const QColor darkAssistantBubble = over(darkTheme.bubbleAssistant, darkTheme.window);
    const QColor lightAssistantBubble = over(lightTheme.bubbleAssistant, lightTheme.window);
    const QColor darkUserBubble = over(darkTheme.bubbleUser, darkTheme.window);
    const QColor lightUserBubble = over(lightTheme.bubbleUser, lightTheme.window);
    check(contrastRatio(darkTheme.textPrimary, darkTheme.window) >= 7.0,
          "dark primary text contrast >= 7:1");
    check(contrastRatio(lightTheme.textPrimary, lightTheme.window) >= 7.0,
          "light primary text contrast >= 7:1");
    check(contrastRatio(darkTheme.bubbleAssistantText, darkAssistantBubble) >= 4.5,
          "dark assistant bubble text contrast >= 4.5:1");
    check(contrastRatio(lightTheme.bubbleAssistantText, lightAssistantBubble) >= 4.5,
          "light assistant bubble text contrast >= 4.5:1");
    check(contrastRatio(darkTheme.bubbleUserText, darkUserBubble) >= 4.5,
          "dark user bubble text contrast >= 4.5:1");
    check(contrastRatio(lightTheme.bubbleUserText, lightUserBubble) >= 4.5,
          "light user bubble text contrast >= 4.5:1");
    /* Filled buttons keep the brand blue; white on it clears the 3:1 UI
     * component floor even though it does not reach body-text contrast. */
    check(contrastRatio(darkTheme.accentText,
                        over(darkTheme.accentStrong, darkTheme.window)) >= 3.0,
          "dark accent button text contrast >= 3:1");
    check(contrastRatio(lightTheme.accentText,
                        over(lightTheme.accentStrong, lightTheme.window)) >= 3.0,
          "light accent button text contrast >= 3:1");

    check(contrastRatio(darkTheme.textMuted, darkTheme.window) >= 4.5,
          "dark muted text contrast >= 4.5:1");
    check(contrastRatio(lightTheme.textMuted, lightTheme.window) >= 4.5,
          "light muted text contrast >= 4.5:1");
    check(contrastRatio(darkTheme.listTitle, darkTheme.sidebar) >= 7.0,
          "dark conversation title contrast >= 7:1");
    check(contrastRatio(lightTheme.listTitle, lightTheme.sidebar) >= 7.0,
          "light conversation title contrast >= 7:1");
    /* Faint text is decorative (timestamps, hints, disabled labels). */
    check(contrastRatio(darkTheme.textFaint, darkTheme.window) >= 3.0,
          "dark faint text contrast >= 3:1");
    check(contrastRatio(lightTheme.textFaint, lightTheme.window) >= 3.0,
          "light faint text contrast >= 3:1");

    check(contrastRatio(darkTheme.link, darkAssistantBubble) >= 4.5,
          "dark link contrast >= 4.5:1");
    check(contrastRatio(lightTheme.link, lightAssistantBubble) >= 4.5,
          "light link contrast >= 4.5:1");
    check(contrastRatio(over(darkTheme.status, darkTheme.window), darkTheme.window) >= 4.5,
          "dark status text contrast >= 4.5:1");
    check(contrastRatio(lightTheme.status, lightTheme.window) >= 4.5,
          "light status text contrast >= 4.5:1");

    const auto badgeBackground = [](const ChatPalette &palette) {
        QColor tint = palette.accent;
        tint.setAlphaF(0.13);
        return over(tint, palette.window);
    };
    check(contrastRatio(over(darkTheme.badgeText, badgeBackground(darkTheme)),
                        badgeBackground(darkTheme)) >= 4.5,
          "dark model badge contrast >= 4.5:1");
    check(contrastRatio(over(lightTheme.badgeText, badgeBackground(lightTheme)),
                        badgeBackground(lightTheme)) >= 4.5,
          "light model badge contrast >= 4.5:1");

    // style-sheet helper
    check(ChatTheme::css(QColor(255, 255, 255)) == QStringLiteral("#ffffff"),
          "opaque colour serialised as hex");
    check(ChatTheme::css(QColor(0, 0, 0, 128)) == QStringLiteral("rgba(0,0,0,50%)"),
          "translucent colour serialised as rgba percentage");

    // chat attachments
    check(AttachmentUtils::isLongText(
              QString(AttachmentUtils::kLongTextThreshold + 1, QLatin1Char('x'))),
          "text above the threshold is a long-text attachment");
    check(!AttachmentUtils::isLongText(
              QString(AttachmentUtils::kLongTextThreshold, QLatin1Char('x'))),
          "text at the threshold stays in the input");
    check(!AttachmentUtils::isLongText(QStringLiteral("short message")),
          "short text stays in the input");

    const ChatAttachment longText = AttachmentUtils::fromText(QString(5000, QChar(0x5B57)));
    check(longText.kind == ChatAttachment::Text && longText.text.size() == 5000,
          "long-text attachment keeps its content");
    check(longText.name.endsWith(QStringLiteral(".txt")),
          "long-text attachment gets a .txt name");

    QTemporaryDir attachmentDir;
    check(attachmentDir.isValid(), "temporary dir for attachment files");
    const QString textPath = attachmentDir.path() + QStringLiteral("/note.md");
    {
        QFile file(textPath);
        check(file.open(QIODevice::WriteOnly), "create markdown attachment fixture");
        file.write(QStringLiteral("# hello\n内容").toUtf8());
    }
    const ChatAttachment textFile = AttachmentUtils::fromFile(
        textPath, attachmentDir.path() + QStringLiteral("/store"));
    check(textFile.kind == ChatAttachment::File, "markdown file stays a file attachment");
    check(textFile.text.contains(QStringLiteral("hello")) &&
              textFile.text.contains(QStringLiteral("内容")),
          "text file content extracted");
    const QString textPrompt =
        AttachmentUtils::promptText(QStringLiteral("问题"), { textFile });
    check(textPrompt.contains(QStringLiteral("note.md")) &&
              textPrompt.contains(QStringLiteral("hello")),
          "text file inlined into the prompt");

    QImage testImage(64, 48, QImage::Format_ARGB32);
    testImage.fill(QColor(10, 20, 30));
    const QString imagePath = attachmentDir.path() + QStringLiteral("/pic.png");
    check(testImage.save(imagePath), "test image saved");
    const ChatAttachment imageFile = AttachmentUtils::fromFile(
        imagePath, attachmentDir.path() + QStringLiteral("/store"));
    check(imageFile.kind == ChatAttachment::Image, "image file classified as image");
    check(imageFile.path != imagePath && QFile::exists(imageFile.path),
          "image copied into the attachment store");
    check(imageFile.mimeType == QStringLiteral("image/png"), "image mime detected");

    const QJsonValue plainPayload =
        AttachmentUtils::contentJson(QStringLiteral("问题"), { textFile });
    check(plainPayload.isString() &&
              plainPayload.toString().contains(QStringLiteral("问题")),
          "text-only messages keep a string payload");

    ChatAttachment uploadedImage = imageFile;
    uploadedImage.fileId = QStringLiteral("file-api-test");
    const QJsonArray uploadedBlocks =
        AttachmentUtils::contentJson(QStringLiteral("描述"), { uploadedImage }).toArray();
    bool hasFileBlock = false;
    for (const QJsonValue &value : uploadedBlocks) {
        const QJsonObject block = value.toObject();
        if (block.value(QStringLiteral("type")).toString() == QStringLiteral("file") &&
            block.value(QStringLiteral("file_id")).toString() ==
                QStringLiteral("file-api-test")) {
            hasFileBlock = true;
        }
    }
    check(hasFileBlock, "uploaded image referenced by file_id");

    const QJsonArray inlineBlocks =
        AttachmentUtils::contentJson(QString(), { imageFile }).toArray();
    bool hasDataUrl = false;
    for (const QJsonValue &value : inlineBlocks) {
        const QJsonObject block = value.toObject();
        if (block.value(QStringLiteral("type")).toString() ==
            QStringLiteral("image_url")) {
            hasDataUrl = block.value(QStringLiteral("image_url"))
                             .toObject()
                             .value(QStringLiteral("url"))
                             .toString()
                             .startsWith(QStringLiteral("data:image/png;base64,"));
        }
    }
    check(hasDataUrl, "image without a file id falls back to an inline data URL");
    bool imageNameInText = false;
    for (const QJsonValue &value : inlineBlocks) {
        const QJsonObject block = value.toObject();
        if (block.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
            imageNameInText = block.value(QStringLiteral("text"))
                                  .toString()
                                  .contains(imageFile.name);
        }
    }
    check(imageNameInText, "image-only messages still carry a text block");

    QTextStream(stdout) << (failures == 0 ? "ALL PASS\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
