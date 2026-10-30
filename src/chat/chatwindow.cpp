// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatwindow.h"

#include "config.h"
#include "mcpclient.h"

#include <QCheckBox>
#include <QClipboard>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSpinBox>
#include <QShortcut>
#include <QTextCursor>
#include <QTextBrowser>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

namespace {

bool mimeOk(const QMimeData *mime)
{
    return mime && (mime->hasUrls() || mime->hasText() || mime->hasImage());
}

QString escapedHtml(const QString &text)
{
    QString html = text.toHtmlEscaped();
    html.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return html;
}

/** Lightweight Markdown to HTML for assistant messages. */
QString renderInline(QString text)
{
    text = text.toHtmlEscaped();
    text.replace(QRegularExpression(QStringLiteral("`([^`]+)`")),
                 QStringLiteral("<code>\\1</code>"));
    text.replace(QRegularExpression(QStringLiteral("\\*\\*([^*]+)\\*\\*")),
                 QStringLiteral("<b>\\1</b>"));
    text.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\((https?://[^)\\s]+)\\)")),
                 QStringLiteral("<a href=\"\\2\">\\1</a>"));
    text.replace(QRegularExpression(QStringLiteral("(^|[\\s(])(https?://[^\\s<]+)")),
                 QStringLiteral("\\1<a href=\"\\2\">\\2</a>"));
    return text;
}

QString markdownToHtml(const QString &markdown)
{
    QString html;
    QString code;
    bool inCode = false;

    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QStringLiteral("```"))) {
            if (inCode) {
                html += QStringLiteral("<pre><code>%1</code></pre>").arg(code.toHtmlEscaped());
                code.clear();
            }
            inCode = !inCode;
            continue;
        }
        if (inCode) {
            code += line;
            code += QLatin1Char('\n');
            continue;
        }

        if (line.startsWith(QStringLiteral("### "))) {
            html += QStringLiteral("<h3>%1</h3>").arg(renderInline(line.mid(4)));
        } else if (line.startsWith(QStringLiteral("## "))) {
            html += QStringLiteral("<h2>%1</h2>").arg(renderInline(line.mid(3)));
        } else if (line.startsWith(QStringLiteral("# "))) {
            html += QStringLiteral("<h2>%1</h2>").arg(renderInline(line.mid(2)));
        } else if (line.startsWith(QStringLiteral("- ")) ||
                   line.startsWith(QStringLiteral("* "))) {
            html += QStringLiteral("<div class='li'>• %1</div>").arg(renderInline(line.mid(2)));
        } else if (line.trimmed().isEmpty()) {
            html += QStringLiteral("<div class='gap'></div>");
        } else {
            html += QStringLiteral("<div>%1</div>").arg(renderInline(line));
        }
    }

    if (inCode) {
        html += QStringLiteral("<pre><code>%1</code></pre>").arg(code.toHtmlEscaped());
    }
    return html;
}

} // namespace

class ChatSidebar : public QListWidget
{
    Q_OBJECT

public:
    explicit ChatSidebar(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setAcceptDrops(true);
    }

signals:
    void droppedAt(int index, const QMimeData *mime);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (mimeOk(event->mimeData())) {
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (mimeOk(event->mimeData())) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!mimeOk(event->mimeData())) {
            event->ignore();
            return;
        }
        const QModelIndex index = indexAt(event->position().toPoint());
        Q_EMIT droppedAt(index.isValid() ? index.row() : -1, event->mimeData());
        event->acceptProposedAction();
    }
};

class ChatView : public QTextBrowser
{
    Q_OBJECT

public:
    explicit ChatView(QWidget *parent = nullptr)
        : QTextBrowser(parent)
    {
        setAcceptDrops(true);
    }

signals:
    void dropped(const QMimeData *mime);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (mimeOk(event->mimeData())) {
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (mimeOk(event->mimeData())) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!mimeOk(event->mimeData())) {
            event->ignore();
            return;
        }
        Q_EMIT dropped(event->mimeData());
        event->acceptProposedAction();
    }
};

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
    , m_client(new ChatClient(this))
{
    setWindowTitle(tr("小埃同学"));
    setMinimumSize(760, 520);
    resize(1000, 700);
    m_storePath = Config::instance()->dataDir() + QStringLiteral("/chat.json");

    setupUi();
    McpManager::instance()->reload();
    load();
    m_current = m_conversations.isEmpty() ? -1 : qBound(0, m_current, m_conversations.size() - 1);
    updateSidebar();
    renderConversation();

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        move(screen->availableGeometry().center() - rect().center());
    }

    connect(m_client, &ChatClient::delta, this, [this](const QString &text) {
        if (m_current < 0 || m_streamingIndex < 0 ||
            m_streamingIndex >= m_conversations[m_current].messages.size()) {
            return;
        }
        m_conversations[m_current].messages[m_streamingIndex].content += text;
        renderConversation();
    });
    connect(m_client, &ChatClient::finished, this, [this](const QString &) {
        m_streamingIndex = -1;
        m_toolScaffold = QJsonArray();
        m_toolRounds = 0;
        if (m_current >= 0) {
            m_conversations[m_current].updatedAt = QDateTime::currentMSecsSinceEpoch();
        }
        save();
        renderConversation();
    });
    connect(m_client, &ChatClient::toolCallsReady, this, [this](const QVector<ToolCall> &calls) {
        if (m_current < 0 || m_streamingIndex < 0) {
            return;
        }
        if (m_toolRounds >= 5) {
            appendError(tr("工具调用次数过多，已停止"));
            m_streamingIndex = -1;
            return;
        }
        ++m_toolRounds;

        QStringList names;
        for (const ToolCall &call : calls) {
            names.append(call.name);
        }
        ChatMessage &message =
            m_conversations[m_current].messages[m_streamingIndex];
        message.content += QStringLiteral("%1🔧 调用工具：%2")
                               .arg(message.content.isEmpty() ? QString() : QStringLiteral("\n\n"),
                                    names.join(QStringLiteral(", ")));
        renderConversation();

        runToolCalls(calls, 0, QJsonArray());
    });
    connect(m_client, &ChatClient::onceFinished, this, [this](const QString &summary) {
        m_compressing = false;
        if (m_current >= 0) {
            ChatConversation &conversation = m_conversations[m_current];
            const int keep = qMax(2, Config::instance()
                                         ->getInt(QStringLiteral("chat/contextLength"), 40) /
                                         4);
            const int keepCount = qMin(keep, conversation.messages.size());
            const QVector<ChatMessage> kept =
                conversation.messages.mid(conversation.messages.size() - keepCount);
            conversation.messages.clear();
            conversation.messages.append(
                ChatMessage{ QStringLiteral("system"),
                             QStringLiteral("【此前对话摘要】\n") + summary });
            conversation.messages += kept;
            save();
            renderConversation();
        }
        m_status->hide();
        if (m_afterCompressSend) {
            m_afterCompressSend = false;
            m_skipCompress = true;
            requestCompletion();
        }
    });
    connect(m_client, &ChatClient::onceFailed, this, [this](const QString &error) {
        m_compressing = false;
        m_status->hide();
        if (m_afterCompressSend) {
            m_afterCompressSend = false;
            m_skipCompress = true;
            requestCompletion();
        } else {
            appendError(error);
        }
    });
    connect(m_client, &ChatClient::failed, this, [this](const QString &error) {
        if (m_current >= 0 && m_streamingIndex >= 0 &&
            m_streamingIndex < m_conversations[m_current].messages.size()) {
            m_conversations[m_current].messages[m_streamingIndex].content =
                tr("⚠️ %1").arg(error);
        }
        m_streamingIndex = -1;
        save();
        renderConversation();
    });
}

void ChatWindow::setupUi()
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *sidebarPane = new QWidget(this);
    sidebarPane->setObjectName(QStringLiteral("sidebar"));
    sidebarPane->setFixedWidth(250);
    sidebarPane->setStyleSheet(QStringLiteral(
        "#sidebar { background: rgba(255,255,255,0.045);"
        " border-right: 1px solid rgba(255,255,255,0.08); }"));

    auto *sidebarLayout = new QVBoxLayout(sidebarPane);
    sidebarLayout->setContentsMargins(12, 14, 12, 12);
    sidebarLayout->setSpacing(10);

    auto *brand = new QLabel(tr("小埃同学"), sidebarPane);
    QFont brandFont = brand->font();
    brandFont.setPixelSize(17);
    brandFont.setWeight(QFont::DemiBold);
    brand->setFont(brandFont);
    brand->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.92);"));
    sidebarLayout->addWidget(brand);

    auto *subtitle = new QLabel(tr("把内容拖进对话即可提问"), sidebarPane);
    subtitle->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.4); font-size: 11px;"));
    sidebarLayout->addWidget(subtitle);

    m_sidebar = new ChatSidebar(sidebarPane);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none; color: rgba(255,255,255,0.82); }"
        "QListWidget::item { padding: 8px 6px; border-radius: 7px; }"
        "QListWidget::item:hover { background: rgba(255,255,255,0.07); }"
        "QListWidget::item:selected { background: rgba(120,180,255,0.24); color: white; }"));
    sidebarLayout->addWidget(m_sidebar, 1);

    auto *newButton = new QPushButton(tr("＋ 新建对话"), sidebarPane);
    newButton->setCursor(Qt::PointingHandCursor);
    newButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: rgba(255,255,255,0.85); background: rgba(255,255,255,0.08);"
        " border: none; border-radius: 8px; padding: 7px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(120,180,255,0.28); color: white; }"));
    sidebarLayout->addWidget(newButton);

    auto *menuButton = new QPushButton(tr("⋯ 更多"), sidebarPane);
    menuButton->setCursor(Qt::PointingHandCursor);
    menuButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: rgba(255,255,255,0.55); background: transparent;"
        " border: none; padding: 2px; font-size: 12px; }"
        "QPushButton:hover { color: white; }"));
    auto *menu = new QMenu(menuButton);
    menu->addAction(tr("压缩上下文"), this, [this]() { beginCompression(false); });
    menu->addAction(tr("重试上一条"), this, &ChatWindow::retryLast);
    menu->addAction(tr("重新加载 MCP"), this, [this]() {
        McpManager::instance()->reload();
        const QStringList status = McpManager::instance()->statusLines();
        m_status->setText(status.isEmpty() ? tr("没有配置 MCP 服务器")
                                           : status.join(QStringLiteral(" · ")));
        m_status->show();
    });
    menu->addSeparator();
    menu->addAction(tr("关于"), this, &ChatWindow::showAbout);
    menuButton->setMenu(menu);
    sidebarLayout->addWidget(menuButton);

    auto *settingsButton = new QPushButton(tr("设置"), sidebarPane);
    settingsButton->setCursor(Qt::PointingHandCursor);
    settingsButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: rgba(255,255,255,0.55); background: transparent;"
        " border: none; padding: 4px; font-size: 12px; }"
        "QPushButton:hover { color: white; }"));
    sidebarLayout->addWidget(settingsButton);

    root->addWidget(sidebarPane);

    auto *right = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(14, 14, 14, 12);
    rightLayout->setSpacing(10);

    m_view = new ChatView(right);
    m_view->setOpenExternalLinks(false);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setStyleSheet(QStringLiteral(
        "QTextBrowser { background: transparent; color: rgba(240,240,240,0.94);"
        " border: none; font-size: 14px; }"));
    rightLayout->addWidget(m_view, 1);

    m_status = new QLabel(right);
    m_status->setStyleSheet(QStringLiteral("color: rgba(255,170,120,0.9); font-size: 12px;"));
    m_status->hide();
    rightLayout->addWidget(m_status);

    auto *inputRow = new QHBoxLayout;
    inputRow->setSpacing(10);

    m_input = new QPlainTextEdit(right);
    m_input->setPlaceholderText(tr("输入消息，Ctrl+Enter 发送；也可以把文件/文字拖进窗口"));
    m_input->setFixedHeight(84);
    m_input->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: rgba(255,255,255,0.06); color: rgba(245,245,245,0.95);"
        " border: 1px solid rgba(255,255,255,0.08); border-radius: 10px; padding: 8px;"
        " font-size: 13px; }"));
    inputRow->addWidget(m_input, 1);

    m_sendButton = new QPushButton(tr("发送"), right);
    m_sendButton->setFixedSize(76, 40);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; background: rgba(74,144,255,0.85); border: none;"
        " border-radius: 10px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(90,160,255,0.95); }"
        "QPushButton:disabled { background: rgba(255,255,255,0.12); color: rgba(255,255,255,0.4); }"));
    inputRow->addWidget(m_sendButton);

    rightLayout->addLayout(inputRow);
    root->addWidget(right, 1);

    connect(newButton, &QPushButton::clicked, this, &ChatWindow::newConversation);
    connect(settingsButton, &QPushButton::clicked, this, &ChatWindow::openSettings);
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(m_input, &QPlainTextEdit::textChanged, this, [this]() {
        m_sendButton->setEnabled(!m_input->toPlainText().trimmed().isEmpty() &&
                                 !m_client->busy());
    });
    m_sendButton->setEnabled(false);

    connect(m_sidebar, &QListWidget::currentRowChanged, this, &ChatWindow::selectConversation);
    connect(static_cast<ChatSidebar *>(m_sidebar), &ChatSidebar::droppedAt, this,
            &ChatWindow::handleDropOnConversation);
    connect(static_cast<ChatView *>(m_view), &ChatView::dropped, this,
            &ChatWindow::handleDroppedMime);
    connect(m_view, &QTextBrowser::anchorClicked, this, &ChatWindow::handleAnchor);

    auto *sendShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), m_input);
    connect(sendShortcut, &QShortcut::activated, this, &ChatWindow::sendMessage);
    auto *sendShortcut2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), m_input);
    connect(sendShortcut2, &QShortcut::activated, this, &ChatWindow::sendMessage);
}

void ChatWindow::load()
{
    QFile file(m_storePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }
    const QJsonObject root = document.object();

    m_conversations.clear();
    for (const QJsonValue &value :
         root.value(QStringLiteral("conversations")).toArray()) {
        const QJsonObject object = value.toObject();
        ChatConversation conversation;
        conversation.id = object.value(QStringLiteral("id")).toString();
        conversation.title = object.value(QStringLiteral("title")).toString();
        conversation.updatedAt =
            static_cast<qint64>(object.value(QStringLiteral("updatedAt")).toDouble());
        for (const QJsonValue &messageValue :
             object.value(QStringLiteral("messages")).toArray()) {
            const QJsonObject messageObject = messageValue.toObject();
            ChatMessage message;
            message.role = messageObject.value(QStringLiteral("role")).toString();
            message.content = messageObject.value(QStringLiteral("content")).toString();
            conversation.messages.append(message);
        }
        m_conversations.append(conversation);
    }
    m_current = root.value(QStringLiteral("current")).toInt(-1);
}

void ChatWindow::save() const
{
    QJsonArray conversations;
    for (const ChatConversation &conversation : m_conversations) {
        QJsonArray messages;
        for (const ChatMessage &message : conversation.messages) {
            QJsonObject object;
            object.insert(QStringLiteral("role"), message.role);
            object.insert(QStringLiteral("content"), message.content);
            messages.append(object);
        }
        QJsonObject object;
        object.insert(QStringLiteral("id"), conversation.id);
        object.insert(QStringLiteral("title"), conversation.title);
        object.insert(QStringLiteral("updatedAt"),
                      static_cast<double>(conversation.updatedAt));
        object.insert(QStringLiteral("messages"), messages);
        conversations.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("current"), m_current);
    root.insert(QStringLiteral("conversations"), conversations);

    QSaveFile file(m_storePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

void ChatWindow::newConversation()
{
    if (m_client->busy()) {
        m_client->cancel();
        m_streamingIndex = -1;
    }

    ChatConversation conversation;
    conversation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    conversation.title = tr("新对话");
    conversation.updatedAt = QDateTime::currentMSecsSinceEpoch();

    m_conversations.prepend(conversation);
    m_current = 0;
    updateSidebar();
    renderConversation();
    save();
    m_input->setFocus();
}

void ChatWindow::selectConversation(int index)
{
    if (index < 0 || index >= m_conversations.size() || index == m_current) {
        return;
    }
    if (m_client->busy()) {
        m_client->cancel();
        m_streamingIndex = -1;
    }
    m_current = index;
    renderConversation();
}

void ChatWindow::updateSidebar()
{
    const QSignalBlocker blocker(m_sidebar);
    m_sidebar->clear();
    for (const ChatConversation &conversation : m_conversations) {
        m_sidebar->addItem(conversation.title);
    }
    if (m_current >= 0 && m_current < m_sidebar->count()) {
        m_sidebar->setCurrentRow(m_current);
    }
}

void ChatWindow::renderConversation()
{
    QString html;
    html += QStringLiteral(
        "<style>body{color:#e9e9e9;font-size:14px;} .role{font-size:11px;color:#8a8a8a;"
        "margin-top:14px;} .user{color:#9ecbff;} .assistant{color:#efefef;}"
        ".welcome{color:#b9b9b9;line-height:1.6;} pre{background:rgba(255,255,255,0.07);"
        "padding:8px;border-radius:6px;} h1,h2,h3{margin:8px 0 4px 0;} .li{margin-left:10px;} "
        ".gap{height:6px;} code{background:rgba(255,255,255,0.1);"
        "border-radius:4px;padding:1px 4px;}</style>");

    if (m_current < 0 || m_conversations.isEmpty()) {
        html += QStringLiteral(
            "<div class='welcome'><h3>你好，我是小埃同学。</h3>"
            "<p>把小埃当成桌面上的一个抽屉：<br/>"
            "· 把文件、链接、文字拖到左侧列表空白处 = 新建对话<br/>"
            "· 拖到左侧某个对话 = 给那个对话补充内容<br/>"
            "· 拖到右侧对话区 = 发给当前对话</p>"
            "<p>第一次使用需要配置 DeepSeek API Key：点左下角「设置」，"
            "在 <a href='https://platform.deepseek.com'>platform.deepseek.com</a> "
            "创建 Key 后填进来。</p></div>");
        m_view->setHtml(html);
        return;
    }

    const ChatConversation &conversation = m_conversations.at(m_current);
    if (conversation.messages.isEmpty()) {
        html += QStringLiteral("<div class='welcome'><h3>%1</h3>"
                               "<p>直接输入问题，或者把内容拖进来。</p></div>")
                    .arg(escapedHtml(conversation.title));
    }

    for (int i = 0; i < conversation.messages.size(); ++i) {
        const ChatMessage &message = conversation.messages.at(i);
        const QString role = message.role == QStringLiteral("user") ? tr("你") : tr("小埃");
        const QString cssClass =
            message.role == QStringLiteral("user") ? QStringLiteral("user")
                                                   : QStringLiteral("assistant");
        html += QStringLiteral("<div class='role'>%1</div>").arg(role);
        const QString body = message.role == QStringLiteral("assistant")
                                 ? markdownToHtml(message.content)
                                 : escapedHtml(message.content);
        html += QStringLiteral("<div class='%1'>%2</div>").arg(cssClass, body);

        html += QStringLiteral("<div class='actions'>");
        if (message.role == QStringLiteral("user")) {
            html += QStringLiteral("<a href='gxde-ai-see:edit/%1'>编辑</a>").arg(i);
        } else if (message.role == QStringLiteral("assistant")) {
            html += QStringLiteral("<a href='gxde-ai-see:copy/%1'>复制</a> · ").arg(i);
            html += QStringLiteral("<a href='gxde-ai-see:fork/%1'>分支</a>").arg(i);
            if (i == conversation.messages.size() - 1) {
                html += QStringLiteral(" · <a href='gxde-ai-see:retry'>重试</a>");
            }
        }
        html += QStringLiteral("</div>");
    }

    if (m_client->busy() && m_streamingIndex >= 0) {
        html += QStringLiteral("<div class='role'>%1</div><div class='assistant'>…</div>")
                    .arg(tr("小埃"));
    }

    m_view->setHtml(html);
    m_view->moveCursor(QTextCursor::End);
    m_view->ensureCursorVisible();
}

void ChatWindow::sendMessage()
{
    if (m_client->busy() || m_compressing) {
        return;
    }
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }
    if (m_current < 0) {
        newConversation();
    }

    ChatConversation &conversation = m_conversations[m_current];
    if (conversation.title == tr("新对话")) {
        conversation.title = text.left(24);
    }
    conversation.messages.append(ChatMessage{ QStringLiteral("user"), text });
    conversation.updatedAt = QDateTime::currentMSecsSinceEpoch();
    m_skipCompress = false;
    m_toolScaffold = QJsonArray();
    m_toolRounds = 0;

    m_input->clear();
    m_status->hide();
    updateSidebar();
    requestCompletion();
}

void ChatWindow::requestCompletion()
{
    if (m_current < 0) {
        return;
    }

    if (needsCompression()) {
        beginCompression(true);
        return;
    }

    ChatConversation &conversation = m_conversations[m_current];
    conversation.messages.append(ChatMessage{ QStringLiteral("assistant"), QString() });
    m_streamingIndex = conversation.messages.size() - 1;
    m_sendButton->setEnabled(false);
    renderConversation();
    sendPayload();
}

void ChatWindow::sendPayload()
{
    if (m_current < 0) {
        return;
    }
    const ChatConversation &conversation = m_conversations[m_current];

    QVector<ChatMessage> payload;
    const QString systemPrompt =
        Config::instance()->get(QStringLiteral("chat/systemPrompt")).toString().trimmed();
    if (!systemPrompt.isEmpty()) {
        payload.append(ChatMessage{ QStringLiteral("system"), systemPrompt });
    }

    const int keep = Config::instance()->getInt(QStringLiteral("chat/contextLength"), 40);
    int start = 0;
    if (keep > 0 && conversation.messages.size() > keep) {
        start = conversation.messages.size() - keep;
    }
    for (int i = start; i < conversation.messages.size() - 1; ++i) {
        payload.append(conversation.messages.at(i));
    }

    if (m_toolRounds >= 5) {
        m_client->setTools(QJsonArray());
    } else {
        m_client->setTools(McpManager::instance()->toolDefinitions());
    }
    m_client->send(payload, m_toolScaffold);
}

void ChatWindow::runToolCalls(const QVector<ToolCall> &calls, int index,
                              const QJsonArray &results)
{
    if (index >= calls.size()) {
        QJsonObject assistant;
        assistant.insert(QStringLiteral("role"), QStringLiteral("assistant"));
        assistant.insert(QStringLiteral("content"), QJsonValue::Null);
        QJsonArray toolCalls;
        for (const ToolCall &call : calls) {
            QJsonObject function;
            function.insert(QStringLiteral("name"), call.name);
            function.insert(QStringLiteral("arguments"), call.arguments);
            toolCalls.append(QJsonObject{ { QStringLiteral("id"), call.id },
                                          { QStringLiteral("type"), QStringLiteral("function") },
                                          { QStringLiteral("function"), function } });
        }
        assistant.insert(QStringLiteral("tool_calls"), toolCalls);
        m_toolScaffold.append(assistant);
        for (const QJsonValue &result : results) {
            m_toolScaffold.append(result);
        }
        sendPayload();
        return;
    }

    const ToolCall call = calls.at(index);
    QJsonObject arguments;
    const QJsonDocument document = QJsonDocument::fromJson(call.arguments.toUtf8());
    if (document.isObject()) {
        arguments = document.object();
    }

    McpManager::instance()->callTool(
        call.name, arguments,
        [this, calls, index, results](const QJsonObject &response) {
            QString text;
            const QJsonArray content = response.value(QStringLiteral("content")).toArray();
            for (const QJsonValue &value : content) {
                const QJsonObject object = value.toObject();
                if (object.value(QStringLiteral("type")).toString() ==
                    QStringLiteral("text")) {
                    text += object.value(QStringLiteral("text")).toString();
                    text += QLatin1Char('\n');
                }
            }
            if (text.isEmpty()) {
                text = response.contains(QStringLiteral("error"))
                           ? response.value(QStringLiteral("error")).toString()
                           : QString::fromUtf8(
                                 QJsonDocument(response).toJson(QJsonDocument::Compact));
            }

            QJsonArray next = results;
            next.append(QJsonObject{ { QStringLiteral("role"), QStringLiteral("tool") },
                                     { QStringLiteral("tool_call_id"), calls.at(index).id },
                                     { QStringLiteral("content"), text } });
            runToolCalls(calls, index + 1, next);
        });
}

bool ChatWindow::needsCompression() const
{
    if (m_current < 0 || m_skipCompress) {
        return false;
    }
    if (!Config::instance()->getBool(QStringLiteral("chat/autoCompress"), true)) {
        return false;
    }
    const int keep = Config::instance()->getInt(QStringLiteral("chat/contextLength"), 40);
    return m_conversations.at(m_current).messages.size() > keep + 4;
}

void ChatWindow::beginCompression(bool thenSend)
{
    if (m_current < 0 || m_compressing) {
        return;
    }
    ChatConversation &conversation = m_conversations[m_current];
    const int keep = qMax(2, Config::instance()
                                  ->getInt(QStringLiteral("chat/contextLength"), 40) /
                                  4);
    if (conversation.messages.size() <= keep + 2) {
        if (thenSend) {
            m_skipCompress = true;
            requestCompletion();
        }
        return;
    }

    const QVector<ChatMessage> older =
        conversation.messages.mid(0, conversation.messages.size() - keep);
    QString transcript;
    for (const ChatMessage &message : older) {
        transcript += (message.role == QStringLiteral("user") ? QStringLiteral("用户：")
                                                              : QStringLiteral("助手："));
        transcript += message.content;
        transcript += QLatin1Char('\n');
    }
    transcript = transcript.left(20000);

    QVector<ChatMessage> request;
    request.append(ChatMessage{
        QStringLiteral("system"),
        QStringLiteral("你是一个对话摘要助手。请用中文把下面的对话压缩成要点，保留事实、结论、"
                       "待办和用户偏好，不要编造。") });
    request.append(ChatMessage{ QStringLiteral("user"), transcript });

    m_compressing = true;
    m_afterCompressSend = thenSend;
    m_status->setText(tr("正在压缩上下文…"));
    m_status->show();
    m_client->sendOnce(request);
}

void ChatWindow::editMessage(int index)
{
    if (m_client->busy() || m_compressing || m_current < 0) {
        return;
    }
    ChatConversation &conversation = m_conversations[m_current];
    if (index < 0 || index >= conversation.messages.size()) {
        return;
    }
    if (conversation.messages.at(index).role != QStringLiteral("user")) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("编辑消息"));
    dialog.setMinimumWidth(520);
    auto *layout = new QVBoxLayout(&dialog);
    auto *edit = new QPlainTextEdit(&dialog);
    edit->setPlainText(conversation.messages.at(index).content);
    edit->setFixedHeight(160);
    layout->addWidget(edit);
    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString text = edit->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }

    conversation.messages = conversation.messages.mid(0, index);
    conversation.messages.append(ChatMessage{ QStringLiteral("user"), text });
    conversation.updatedAt = QDateTime::currentMSecsSinceEpoch();
    save();
    updateSidebar();
    requestCompletion();
}

void ChatWindow::retryLast()
{
    if (m_client->busy() || m_compressing || m_current < 0) {
        return;
    }
    ChatConversation &conversation = m_conversations[m_current];
    int lastUser = -1;
    for (int i = conversation.messages.size() - 1; i >= 0; --i) {
        if (conversation.messages.at(i).role == QStringLiteral("user")) {
            lastUser = i;
            break;
        }
    }
    if (lastUser < 0) {
        return;
    }
    conversation.messages = conversation.messages.mid(0, lastUser + 1);
    requestCompletion();
}

void ChatWindow::forkFrom(int index)
{
    if (m_current < 0) {
        return;
    }
    const ChatConversation &source = m_conversations[m_current];
    if (index < 0 || index >= source.messages.size()) {
        return;
    }

    ChatConversation copy;
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.title = tr("%1 · 分支").arg(source.title);
    copy.updatedAt = QDateTime::currentMSecsSinceEpoch();
    copy.messages = source.messages.mid(0, index + 1);

    m_conversations.prepend(copy);
    m_current = 0;
    updateSidebar();
    save();
    renderConversation();
}

void ChatWindow::handleAnchor(const QUrl &url)
{
    const QString target = url.toString();
    if (target.startsWith(QStringLiteral("http://")) ||
        target.startsWith(QStringLiteral("https://"))) {
        QDesktopServices::openUrl(url);
        return;
    }
    if (target.startsWith(QStringLiteral("gxde-ai-see:edit/"))) {
        editMessage(target.section(QLatin1Char('/'), -1).toInt());
    } else if (target.startsWith(QStringLiteral("gxde-ai-see:fork/"))) {
        forkFrom(target.section(QLatin1Char('/'), -1).toInt());
    } else if (target.startsWith(QStringLiteral("gxde-ai-see:copy/"))) {
        const int index = target.section(QLatin1Char('/'), -1).toInt();
        if (m_current >= 0 && index >= 0 &&
            index < m_conversations.at(m_current).messages.size()) {
            QApplication::clipboard()->setText(
                m_conversations.at(m_current).messages.at(index).content);
        }
    } else if (target == QStringLiteral("gxde-ai-see:retry")) {
        retryLast();
    }
}

void ChatWindow::showAbout()
{
    QMessageBox::about(
        this, tr("关于小埃同学"),
        tr("<h3>GXDE 小埃同学</h3>"
           "<p>版本 %1</p>"
           "<p>GXDE 桌面的 AI 助手：传送门、内容工厂、待办与对话。</p>"
           "<p>许可证：GPL-3.0-or-later<br/>"
           "仓库：<a href='https://gitee.com/GXDE-OS/gxde-ai-see'>"
           "gitee.com/GXDE-OS/gxde-ai-see</a></p>")
            .arg(QCoreApplication::applicationVersion()));
}

void ChatWindow::appendError(const QString &message)
{
    m_status->setText(message);
    m_status->show();
}

void ChatWindow::openSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("小埃同学设置"));
    dialog.setMinimumWidth(460);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;

    auto *keyEdit = new QLineEdit(Config::instance()
                                      ->get(QStringLiteral("chat/apiKey"))
                                      .toString(),
                                  &dialog);
    keyEdit->setEchoMode(QLineEdit::Password);
    keyEdit->setPlaceholderText(QStringLiteral("sk-…"));
    form->addRow(tr("DeepSeek API Key"), keyEdit);

    auto *baseEdit = new QLineEdit(Config::instance()
                                       ->get(QStringLiteral("chat/baseUrl"),
                                             QStringLiteral("https://api.deepseek.com"))
                                       .toString(),
                                   &dialog);
    form->addRow(tr("API 地址"), baseEdit);

    auto *modelBox = new QComboBox(&dialog);
    modelBox->setEditable(true);
    modelBox->addItems({ QStringLiteral("deepseek-chat"),
                         QStringLiteral("deepseek-reasoner") });
    modelBox->setCurrentText(Config::instance()
                                 ->get(QStringLiteral("chat/model"),
                                       QStringLiteral("deepseek-chat"))
                                 .toString());
    form->addRow(tr("模型"), modelBox);

    auto *contextSpin = new QSpinBox(&dialog);
    contextSpin->setRange(4, 200);
    contextSpin->setValue(
        Config::instance()->getInt(QStringLiteral("chat/contextLength"), 40));
    form->addRow(tr("上下文条数"), contextSpin);

    auto *autoCompressBox = new QCheckBox(tr("超出后自动压缩旧消息"), &dialog);
    autoCompressBox->setChecked(
        Config::instance()->getBool(QStringLiteral("chat/autoCompress"), true));
    form->addRow(QString(), autoCompressBox);

    auto *mcpEdit = new QPlainTextEdit(&dialog);
    mcpEdit->setFixedHeight(90);
    mcpEdit->setPlaceholderText(
        QStringLiteral("[{\"name\":\"fs\",\"command\":\"npx\","
                       "\"args\":[\"-y\",\"@modelcontextprotocol/server-filesystem\","
                       "\"/tmp\"]}]"));
    mcpEdit->setPlainText(
        Config::instance()->get(QStringLiteral("chat/mcpServers")).toString());
    form->addRow(tr("MCP 服务器（JSON）"), mcpEdit);

    auto *systemEdit = new QPlainTextEdit(&dialog);
    systemEdit->setFixedHeight(80);
    systemEdit->setPlainText(Config::instance()
                                 ->get(QStringLiteral("chat/systemPrompt"))
                                 .toString());
    form->addRow(tr("系统提示"), systemEdit);

    layout->addLayout(form);

    auto *hint = new QLabel(
        tr("还没有账号？前往 <a href='https://platform.deepseek.com'>platform.deepseek.com</a> "
           "开通并创建 API Key。Key 只保存在本机配置文件中。"),
        &dialog);
    hint->setWordWrap(true);
    hint->setOpenExternalLinks(true);
    hint->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.55); font-size: 12px;"));
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        Config::instance()->set(QStringLiteral("chat/apiKey"), keyEdit->text().trimmed());
        Config::instance()->set(QStringLiteral("chat/baseUrl"), baseEdit->text().trimmed());
        Config::instance()->set(QStringLiteral("chat/model"), modelBox->currentText());
        Config::instance()->set(QStringLiteral("chat/systemPrompt"),
                                systemEdit->toPlainText());
        Config::instance()->set(QStringLiteral("chat/contextLength"), contextSpin->value());
        Config::instance()->set(QStringLiteral("chat/autoCompress"),
                                autoCompressBox->isChecked());
        const QJsonDocument mcpDocument =
            QJsonDocument::fromJson(mcpEdit->toPlainText().toUtf8());
        if (mcpDocument.isArray() || mcpEdit->toPlainText().trimmed().isEmpty()) {
            Config::instance()->set(QStringLiteral("chat/mcpServers"),
                                    mcpEdit->toPlainText().trimmed());
            McpManager::instance()->reload();
        } else {
            m_status->setText(tr("MCP 配置不是合法的 JSON 数组，已忽略"));
            m_status->show();
        }
        Config::instance()->sync();
        renderConversation();
    }
}

void ChatWindow::handleDroppedMime(const QMimeData *mime)
{
    if (!mime) {
        return;
    }

    QString text;
    if (mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            if (!text.isEmpty()) {
                text += QLatin1Char('\n');
            }
            text += url.isLocalFile() ? QStringLiteral("[文件] %1").arg(url.toLocalFile())
                                      : url.toString();
        }
    } else if (mime->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            const QString dir = Config::instance()->cacheDir() + QStringLiteral("/chat");
            QDir().mkpath(dir);
            const QString path = dir + QLatin1Char('/')
                + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".png");
            image.save(path);
            text = QStringLiteral("[图片] %1").arg(path);
        }
    } else if (mime->hasText()) {
        text = mime->text();
    }

    if (text.isEmpty()) {
        return;
    }

    m_input->moveCursor(QTextCursor::End);
    if (!m_input->toPlainText().isEmpty()) {
        m_input->insertPlainText(QStringLiteral("\n"));
    }
    m_input->insertPlainText(text);
    m_input->setFocus();
}

void ChatWindow::handleDropOnConversation(int index, const QMimeData *mime)
{
    if (index < 0) {
        newConversation();
    } else {
        selectConversation(index);
    }
    handleDroppedMime(mime);
    show();
    raise();
    activateWindow();
}

void ChatWindow::activateForAsk()
{
    show();
    raise();
    activateWindow();
    m_input->setFocus();
}

void ChatWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_input->setFocus();
}

#include "chatwindow.moc"
