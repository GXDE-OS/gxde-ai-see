// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatwindow.h"

#include "config.h"
#include "chatview.h"
#include "markdown.h"
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
#include <QFontMetrics>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QPainter>
#include <QStyledItemDelegate>
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
#include <DTitlebar>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSpinBox>
#include <QShortcut>
#include <QTextCursor>
#include <QTextBrowser>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

namespace {

bool mimeOk(const QMimeData *mime)
{
    return mime && (mime->hasUrls() || mime->hasText() || mime->hasImage());
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
    void left();

protected:
    void leaveEvent(QEvent *event) override
    {
        Q_EMIT left();
        QListWidget::leaveEvent(event);
    }

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

namespace {
constexpr int kPreviewRole = Qt::UserRole + 1;
constexpr int kTimeRole = Qt::UserRole + 2;
}

class ConversationDelegate : public QStyledItemDelegate
{
public:
    explicit ConversationDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void setHoverRow(int row) { m_hoverRow = row; }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(0, 62);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect row = option.rect.adjusted(2, 2, -2, -2);
        const bool selected = option.state & QStyle::State_Selected;
        if (selected || index.row() == m_hoverRow) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor(120, 180, 255, 62) : QColor(255, 255, 255, 16));
            painter->drawRoundedRect(row, 8, 8);
        }

        QFont titleFont = option.font;
        titleFont.setPixelSize(13);
        titleFont.setWeight(QFont::DemiBold);
        painter->setFont(titleFont);
        painter->setPen(selected ? QColor(255, 255, 255, 245) : QColor(255, 255, 255, 215));
        const QFontMetrics titleMetrics(titleFont);
        painter->drawText(QRect(row.left() + 10, row.top() + 9, row.width() - 70, 18),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                  Qt::ElideRight, row.width() - 70));

        QFont smallFont = option.font;
        smallFont.setPixelSize(10);
        painter->setFont(smallFont);
        painter->setPen(QColor(255, 255, 255, 95));
        painter->drawText(QRect(row.right() - 56, row.top() + 10, 46, 14),
                          Qt::AlignRight | Qt::AlignVCenter,
                          index.data(kTimeRole).toString());

        smallFont.setPixelSize(11);
        painter->setFont(smallFont);
        painter->setPen(QColor(255, 255, 255, 115));
        const QFontMetrics previewMetrics(smallFont);
        painter->drawText(QRect(row.left() + 10, row.top() + 30, row.width() - 20, 16),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          previewMetrics.elidedText(index.data(kPreviewRole).toString(),
                                                    Qt::ElideRight, row.width() - 20));

        painter->restore();
    }

private:
    int m_hoverRow = -1;
};

ChatWindow::ChatWindow(QWidget *parent)
    : Dtk::Widget::DMainWindow(parent)
    , m_client(new ChatClient(this))
{
    setWindowTitle(tr("小埃同学"));
    titlebar()->setTitle(tr("小埃同学"));
    titlebar()->setIcon(QApplication::windowIcon());
    titlebar()->setAutoHideOnFullscreen(true);
    setMinimumSize(760, 520);
    resize(1000, 700);
    m_storePath = Config::instance()->dataDir() + QStringLiteral("/chat.json");

    setupUi();
    McpManager::instance()->reload();
    load();

    /* DeepSeek renamed the API models; map the old presets once. */
    const QString storedModel =
        Config::instance()->get(QStringLiteral("chat/model")).toString();
    if (storedModel == QStringLiteral("deepseek-chat")) {
        Config::instance()->set(QStringLiteral("chat/model"), QStringLiteral("deepseek-flash"));
        Config::instance()->sync();
    } else if (storedModel == QStringLiteral("deepseek-reasoner")) {
        Config::instance()->set(QStringLiteral("chat/model"), QStringLiteral("deepseek-v4-pro"));
        Config::instance()->sync();
    }

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
        m_messages->updateMessage(
            m_streamingIndex,
            m_conversations[m_current].messages[m_streamingIndex].content);
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
        m_messages->updateMessage(m_streamingIndex, message.content);

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
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    /* ---------------- sidebar ---------------- */
    auto *sidebarPane = new QWidget(this);
    sidebarPane->setObjectName(QStringLiteral("sidebar"));
    sidebarPane->setFixedWidth(264);
    sidebarPane->setStyleSheet(QStringLiteral(
        "#sidebar { background: #17171b; border-right: 1px solid rgba(255,255,255,0.06); }"));

    auto *sidebarLayout = new QVBoxLayout(sidebarPane);
    sidebarLayout->setContentsMargins(14, 16, 14, 12);
    sidebarLayout->setSpacing(10);

    auto *brandRow = new QHBoxLayout;
    brandRow->setSpacing(10);
    auto *logo = new QLabel(sidebarPane);
    logo->setFixedSize(34, 34);
    const qreal ratio = logo->devicePixelRatioF();
    QPixmap logoPixmap = QApplication::windowIcon().pixmap(
        QSize(qRound(34 * ratio), qRound(34 * ratio)));
    logoPixmap.setDevicePixelRatio(ratio);
    logo->setPixmap(logoPixmap);
    logo->setStyleSheet(QStringLiteral("border-radius: 8px;"));
    brandRow->addWidget(logo);

    auto *brandColumn = new QVBoxLayout;
    brandColumn->setSpacing(1);
    auto *brand = new QLabel(tr("小埃同学"), sidebarPane);
    QFont brandFont = brand->font();
    brandFont.setPixelSize(15);
    brandFont.setWeight(QFont::DemiBold);
    brand->setFont(brandFont);
    brand->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.92);"));
    brandColumn->addWidget(brand);
    auto *brandSub = new QLabel(QStringLiteral("GXDE AI See"), sidebarPane);
    brandSub->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.35); font-size: 10px;"));
    brandColumn->addWidget(brandSub);
    brandRow->addLayout(brandColumn);
    brandRow->addStretch();
    sidebarLayout->addLayout(brandRow);

    auto *newButton = new QPushButton(tr("＋ 新建对话"), sidebarPane);
    newButton->setCursor(Qt::PointingHandCursor);
    newButton->setFixedHeight(34);
    newButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; background: rgba(74,144,255,0.85); border: none;"
        " border-radius: 9px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(90,160,255,0.95); }"));
    sidebarLayout->addWidget(newButton);

    m_sidebar = new ChatSidebar(sidebarPane);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setMouseTracking(true);
    m_sidebar->setStyleSheet(
        QStringLiteral("QListWidget { background: transparent; border: none; }"));
    auto *conversationDelegate = new ConversationDelegate(m_sidebar);
    m_sidebar->setItemDelegate(conversationDelegate);
    connect(m_sidebar, &QListWidget::entered, this,
            [this, conversationDelegate](const QModelIndex &index) {
                conversationDelegate->setHoverRow(index.row());
                m_sidebar->viewport()->update();
            });
    connect(static_cast<ChatSidebar *>(m_sidebar), &ChatSidebar::left, this,
            [this, conversationDelegate]() {
                conversationDelegate->setHoverRow(-1);
                m_sidebar->viewport()->update();
            });
    sidebarLayout->addWidget(m_sidebar, 1);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(6);
    auto *settingsButton = new QPushButton(tr("⚙ 设置"), sidebarPane);
    settingsButton->setCursor(Qt::PointingHandCursor);
    settingsButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: rgba(255,255,255,0.7); background: rgba(255,255,255,0.06);"
        " border: none; border-radius: 8px; padding: 6px 8px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.12); color: white; }"));
    bottomRow->addWidget(settingsButton, 1);

    auto *menuButton = new QPushButton(QStringLiteral("⋯"), sidebarPane);
    menuButton->setCursor(Qt::PointingHandCursor);
    menuButton->setFixedWidth(38);
    menuButton->setStyleSheet(settingsButton->styleSheet());
    bottomRow->addWidget(menuButton);
    sidebarLayout->addLayout(bottomRow);

    root->addWidget(sidebarPane);

    /* ---------------- chat pane ---------------- */
    auto *pane = new QWidget(this);
    pane->setStyleSheet(QStringLiteral("background: #1e1e22;"));
    auto *paneLayout = new QVBoxLayout(pane);
    paneLayout->setContentsMargins(0, 0, 0, 0);
    paneLayout->setSpacing(0);

    auto *header = new QWidget(pane);
    header->setFixedHeight(56);
    header->setStyleSheet(QStringLiteral(
        "QWidget { background: rgba(255,255,255,0.02);"
        " border-bottom: 1px solid rgba(255,255,255,0.06); }"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 16, 0);
    headerLayout->setSpacing(10);

    m_titleLabel = new QLabel(tr("小埃同学"), header);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPixelSize(15);
    titleFont.setWeight(QFont::DemiBold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.92);"));
    headerLayout->addWidget(m_titleLabel);

    m_modelLabel = new QLabel(QStringLiteral("deepseek-flash"), header);
    m_modelLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: rgba(160,200,240,0.75); background: rgba(74,144,255,0.13);"
        " border-radius: 8px; padding: 2px 8px; font-size: 10px; }"));
    headerLayout->addWidget(m_modelLabel);
    headerLayout->addStretch();
    paneLayout->addWidget(header);

    m_messages = new ChatMessageList(pane);
    paneLayout->addWidget(m_messages, 1);

    m_status = new QLabel(pane);
    m_status->setContentsMargins(20, 0, 20, 0);
    m_status->setStyleSheet(QStringLiteral("color: rgba(255,170,120,0.9); font-size: 12px;"));
    m_status->hide();
    paneLayout->addWidget(m_status);

    auto *inputPane = new QWidget(pane);
    auto *inputLayout = new QVBoxLayout(inputPane);
    inputLayout->setContentsMargins(20, 6, 20, 14);
    inputLayout->setSpacing(5);

    auto *inputFrame = new QFrame(inputPane);
    inputFrame->setStyleSheet(QStringLiteral(
        "QFrame { background: rgba(255,255,255,0.055);"
        " border: 1px solid rgba(255,255,255,0.08); border-radius: 12px; }"));
    auto *frameLayout = new QHBoxLayout(inputFrame);
    frameLayout->setContentsMargins(12, 8, 8, 8);
    frameLayout->setSpacing(8);

    m_input = new QPlainTextEdit(inputFrame);
    m_input->setPlaceholderText(tr("输入消息，Ctrl+Enter 发送；也可以把文件/文字拖进窗口"));
    m_input->setFixedHeight(64);
    m_input->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: transparent; color: rgba(245,245,245,0.95);"
        " border: none; font-size: 13px; }"));
    frameLayout->addWidget(m_input, 1);

    m_sendButton = new QPushButton(tr("发送"), inputFrame);
    m_sendButton->setFixedSize(72, 34);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; background: rgba(74,144,255,0.9); border: none;"
        " border-radius: 9px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(90,160,255,1.0); }"
        "QPushButton:disabled { background: rgba(255,255,255,0.1);"
        " color: rgba(255,255,255,0.35); }"));
    frameLayout->addWidget(m_sendButton, 0, Qt::AlignBottom);
    inputLayout->addWidget(inputFrame);

    auto *hint = new QLabel(tr("Enter 换行 · Ctrl+Enter 发送 · 支持拖入文件/链接/图片"), inputPane);
    hint->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.3); font-size: 10px;"));
    hint->setAlignment(Qt::AlignRight);
    inputLayout->addWidget(hint);
    paneLayout->addWidget(inputPane);

    root->addWidget(pane, 1);

    /* ---------------- connections ---------------- */
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
    connect(m_messages, &ChatMessageList::dropped, this, &ChatWindow::handleDroppedMime);
    connect(m_messages, &ChatMessageList::linkActivated, this, &ChatWindow::handleAnchor);
    connect(m_messages, &ChatMessageList::copyRequested, this, [this](int index) {
        if (m_current >= 0 && index >= 0 &&
            index < m_conversations.at(m_current).messages.size()) {
            QApplication::clipboard()->setText(
                m_conversations.at(m_current).messages.at(index).content);
        }
    });
    connect(m_messages, &ChatMessageList::editRequested, this, &ChatWindow::editMessage);
    connect(m_messages, &ChatMessageList::forkRequested, this, &ChatWindow::forkFrom);
    connect(m_messages, &ChatMessageList::retryRequested, this, &ChatWindow::retryLast);

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
        auto *item = new QListWidgetItem(conversation.title, m_sidebar);
        QString preview = tr("新对话");
        for (int i = conversation.messages.size() - 1; i >= 0; --i) {
            const ChatMessage &message = conversation.messages.at(i);
            if (!message.content.isEmpty()) {
                preview = message.content.simplified().left(60);
                break;
            }
        }
        item->setData(kPreviewRole, preview);
        item->setData(kTimeRole,
                      QDateTime::fromMSecsSinceEpoch(conversation.updatedAt)
                          .toString(QStringLiteral("HH:mm")));
    }
    if (m_current >= 0 && m_current < m_sidebar->count()) {
        m_sidebar->setCurrentRow(m_current);
    }
    updateHeader();
}

void ChatWindow::renderConversation()
{
    if (m_current < 0 || m_conversations.isEmpty()) {
        m_messages->setMessages({});
        updateHeader();
        return;
    }
    m_messages->setMessages(m_conversations.at(m_current).messages);
    updateHeader();
}

void ChatWindow::updateHeader()
{
    const bool valid = m_current >= 0 && m_current < m_conversations.size();
    m_titleLabel->setText(valid ? m_conversations.at(m_current).title : tr("小埃同学"));
    m_modelLabel->setText(Config::instance()
                              ->get(QStringLiteral("chat/model"),
                                    QStringLiteral("deepseek-flash"))
                              .toString());
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
    modelBox->addItems({ QStringLiteral("deepseek-flash"),
                         QStringLiteral("deepseek-reasoner") });
    modelBox->setCurrentText(Config::instance()
                                 ->get(QStringLiteral("chat/model"),
                                       QStringLiteral("deepseek-flash"))
                                 .toString());
    auto *modelRow = new QHBoxLayout;
    modelRow->setSpacing(6);
    modelRow->addWidget(modelBox, 1);
    auto *fetchModels = new QPushButton(tr("获取模型列表"), &dialog);
    modelRow->addWidget(fetchModels);
    form->addRow(tr("模型"), modelRow);

    connect(fetchModels, &QPushButton::clicked, &dialog, [=, &dialog]() {
        QString base = baseEdit->text().trimmed();
        while (base.endsWith(QLatin1Char('/'))) {
            base.chop(1);
        }
        if (base.endsWith(QStringLiteral("/chat/completions"))) {
            base.chop(int(qstrlen("/chat/completions")));
        }
        QNetworkRequest request{QUrl(base + QStringLiteral("/models"))};
        request.setRawHeader("Authorization",
                             "Bearer " + keyEdit->text().trimmed().toUtf8());
        auto *manager = new QNetworkAccessManager(&dialog);
        QNetworkReply *reply = manager->get(request);
        fetchModels->setEnabled(false);
        connect(reply, &QNetworkReply::finished, &dialog, [=]() {
            fetchModels->setEnabled(true);
            const QByteArray body = reply->readAll();
            const QJsonDocument document = QJsonDocument::fromJson(body);
            const QJsonArray models =
                document.object().value(QStringLiteral("data")).toArray();
            if (reply->error() != QNetworkReply::NoError || models.isEmpty()) {
                fetchModels->setText(tr("获取失败"));
                fetchModels->setToolTip(QString::fromUtf8(body).left(200));
                reply->deleteLater();
                return;
            }
            const QString current = modelBox->currentText();
            modelBox->clear();
            for (const QJsonValue &value : models) {
                const QJsonObject object = value.toObject();
                const QString id = object.value(QStringLiteral("id")).toString();
                if (id.isEmpty()) {
                    continue;
                }
                modelBox->addItem(id);
                const QString name = object.value(QStringLiteral("name")).toString();
                if (!name.isEmpty()) {
                    modelBox->setItemData(modelBox->count() - 1, name, Qt::ToolTipRole);
                }
            }
            const int index = modelBox->findText(current);
            if (index >= 0) {
                modelBox->setCurrentIndex(index);
            }
            fetchModels->setText(tr("已更新"));
            reply->deleteLater();
        });
    });

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

void ChatWindow::startConversationWith(const QString &text, const QStringList &files)
{
    activateForAsk();
    newConversation();

    QString content = text;
    if (!files.isEmpty()) {
        if (!content.isEmpty()) {
            content += QLatin1Char('\n');
        }
        content += files.join(QLatin1Char('\n'));
    }
    if (!content.isEmpty()) {
        m_input->setPlainText(content);
        m_input->moveCursor(QTextCursor::End);
        const QString title = text.isEmpty() ? QFileInfo(files.value(0)).fileName()
                                             : text.simplified().left(24);
        if (m_current >= 0 && !title.isEmpty()) {
            m_conversations[m_current].title = title;
            save();
            updateSidebar();
            renderConversation();
        }
    }
    m_input->setFocus();

    m_status->setText(tr("拖到左侧空白处＝新对话，拖到左侧会话＝追加到该会话，拖到右侧对话区＝发给当前会话"));
    m_status->show();
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
