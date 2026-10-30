// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatwindow.h"

#include "config.h"

#include <QComboBox>
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
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QShortcut>
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
        if (m_current >= 0) {
            m_conversations[m_current].updatedAt = QDateTime::currentMSecsSinceEpoch();
        }
        save();
        renderConversation();
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
    m_view->setOpenExternalLinks(true);
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
        ".welcome{color:#b9b9b9;line-height:1.6;} code{background:rgba(255,255,255,0.1);"
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

    for (const ChatMessage &message : conversation.messages) {
        const QString role = message.role == QStringLiteral("user") ? tr("你") : tr("小埃");
        const QString cssClass =
            message.role == QStringLiteral("user") ? QStringLiteral("user")
                                                   : QStringLiteral("assistant");
        html += QStringLiteral("<div class='role'>%1</div>").arg(role);
        html += QStringLiteral("<div class='%1'>%2</div>")
                    .arg(cssClass, escapedHtml(message.content));
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
    if (m_client->busy()) {
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
    conversation.messages.append(ChatMessage{ QStringLiteral("assistant"), QString() });
    conversation.updatedAt = QDateTime::currentMSecsSinceEpoch();
    m_streamingIndex = conversation.messages.size() - 1;

    m_input->clear();
    m_status->hide();
    m_sendButton->setEnabled(false);
    updateSidebar();
    renderConversation();

    QVector<ChatMessage> payload;
    const QString systemPrompt =
        Config::instance()->get(QStringLiteral("chat/systemPrompt")).toString().trimmed();
    if (!systemPrompt.isEmpty()) {
        payload.append(ChatMessage{ QStringLiteral("system"), systemPrompt });
    }
    for (int i = 0; i < conversation.messages.size() - 1; ++i) {
        payload.append(conversation.messages.at(i));
    }
    m_client->send(payload);
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
