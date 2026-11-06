// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatview.h"

#include <QMimeData>

#include "chattheme.h"
#include "markdown.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QLabel>
#include <QTextBrowser>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QMimeData>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kAvatarSize = 30;
constexpr int kBubbleMaxWidth = 580;

QString avatarStyle(const ChatPalette &palette, bool user)
{
    const QColor background = user ? palette.avatarUserBg : palette.avatarAssistantBg;
    const QColor foreground = user ? palette.avatarUserText : palette.avatarAssistantText;
    return QStringLiteral("QLabel { background: %1; color: %2;"
                          " border-radius: %3px; font-size: 12px; }")
        .arg(ChatTheme::css(background), ChatTheme::css(foreground))
        .arg(kAvatarSize / 2);
}

QToolButton *actionButton(const QString &text, QWidget *parent)
{
    const ChatPalette &palette = ChatTheme::palette();
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setAutoRaise(true);
    button->setStyleSheet(
        QStringLiteral("QToolButton { color: %1; background: transparent; border: none;"
                       " font-size: 11px; padding: 2px 4px; }"
                       "QToolButton:hover { color: %2; }")
            .arg(ChatTheme::css(palette.actionText), ChatTheme::css(palette.actionTextHover)));
    return button;
}

QString noteStyle()
{
    const ChatPalette &palette = ChatTheme::palette();
    return QStringLiteral("QLabel { color: %1; background: %2; border-radius: 8px;"
                          " padding: 6px 10px; font-size: 11px; }")
        .arg(ChatTheme::css(palette.textMuted), ChatTheme::css(palette.noteBg));
}

} // namespace

/** QTextBrowser that grows to fit its document. */
class BubbleText : public QTextBrowser
{
public:
    explicit BubbleText(QWidget *parent = nullptr)
        : QTextBrowser(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setOpenExternalLinks(false);
        setMinimumWidth(160);
        setMaximumWidth(kBubbleMaxWidth);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        setStyleSheet(QStringLiteral("QTextBrowser { background: transparent; border: none; }"));
        document()->setDocumentMargin(0);
        document()->setDefaultStyleSheet(
            ChatTheme::assistantDocumentStyle(ChatTheme::palette()));
        connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                this, [this]() { fitHeight(); });
    }

    void setContent(const QString &html)
    {
        setHtml(html);
        fitHeight();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QTextBrowser::resizeEvent(event);
        fitHeight();
    }

private:
    void fitHeight()
    {
        const int wanted = int(document()->size().height()) + 4;
        if (wanted > 0 && height() != wanted) {
            setFixedHeight(wanted);
        }
    }
};

class MessageRow : public QWidget
{
    Q_OBJECT

public:
    MessageRow(const ChatMessage &message, int index, bool isLast, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_message(message)
        , m_index(index)
        , m_isUser(message.role == QStringLiteral("user"))
        , m_isLast(isLast)
    {
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(4, 2, 4, 2);
        row->setSpacing(8);

        const ChatPalette &palette = ChatTheme::palette();

        auto *avatar = new QLabel(m_isUser ? tr("你") : tr("埃"), this);
        avatar->setFixedSize(kAvatarSize, kAvatarSize);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(avatarStyle(palette, m_isUser));

        auto *column = new QVBoxLayout;
        m_column = column;
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(3);

        auto *meta = new QLabel(m_isUser ? tr("你") : tr("小埃"), this);
        meta->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;")
                                .arg(ChatTheme::css(palette.textMuted)));
        meta->setAlignment(m_isUser ? Qt::AlignRight : Qt::AlignLeft);
        column->addWidget(meta);

        auto *bubble = new QFrame(this);
        m_bubble = bubble;
        bubble->setMaximumWidth(kBubbleMaxWidth + 26);
        bubble->setStyleSheet(
            QStringLiteral("QFrame { background: %1; border-radius: 12px; }")
                .arg(ChatTheme::css(m_isUser ? palette.bubbleUser
                                             : palette.bubbleAssistant)));
        auto *bubbleLayout = new QVBoxLayout(bubble);
        bubbleLayout->setContentsMargins(13, 9, 13, 9);
        bubbleLayout->setSpacing(0);

        if (m_isUser) {
            auto *label = new QLabel(bubble);
            label->setTextFormat(Qt::RichText);
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            label->setMaximumWidth(kBubbleMaxWidth);
            label->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; }")
                                     .arg(ChatTheme::css(palette.bubbleUserText)));
            label->setText(escapedHtml(message.content));
            m_label = label;
            bubbleLayout->addWidget(label);
        } else {
            m_body = new BubbleText(bubble);
            m_body->setContent(markdownToHtml(message.content));
            connect(m_body, &QTextBrowser::anchorClicked, this,
                    [this](const QUrl &url) { Q_EMIT linkActivated(url); });
            bubbleLayout->addWidget(m_body);
        }
        column->addWidget(bubble);

        m_actions = new QWidget(this);
        auto *actions = new QHBoxLayout(m_actions);
        actions->setContentsMargins(2, 0, 2, 0);
        actions->setSpacing(6);

        auto *copy = actionButton(tr("复制"), m_actions);
        connect(copy, &QToolButton::clicked, this,
                [this]() { Q_EMIT copyRequested(m_index); });
        actions->addWidget(copy);

        if (m_isUser) {
            auto *edit = actionButton(tr("编辑"), m_actions);
            connect(edit, &QToolButton::clicked, this,
                    [this]() { Q_EMIT editRequested(m_index); });
            actions->addWidget(edit);
        } else {
            auto *fork = actionButton(tr("分支"), m_actions);
            connect(fork, &QToolButton::clicked, this,
                    [this]() { Q_EMIT forkRequested(m_index); });
            actions->addWidget(fork);
            auto *edit = actionButton(tr("编辑"), m_actions);
            connect(edit, &QToolButton::clicked, this,
                    [this]() { Q_EMIT editRequested(m_index); });
            actions->addWidget(edit);
            auto *retry = actionButton(tr("重试"), m_actions);
            connect(retry, &QToolButton::clicked, this,
                    [this]() { Q_EMIT retryRequested(m_index); });
            actions->addWidget(retry);
        }
        actions->addStretch();
        column->addWidget(m_actions);

        if (m_isUser) {
            row->addStretch(1);
            row->addLayout(column);
            row->addWidget(avatar, 0, Qt::AlignTop);
        } else {
            row->addWidget(avatar, 0, Qt::AlignTop);
            row->addLayout(column);
            row->addStretch(1);
        }
    }

    int rowIndex() const { return m_index; }

    void setContent(const QString &text)
    {
        m_message.content = text;
        if (m_body) {
            m_body->setContent(markdownToHtml(text));
        } else if (m_label) {
            m_label->setText(escapedHtml(text));
        }
        updateBubble();
    }

    void setThinking(const QString &text, bool active)
    {
        if (text.isEmpty()) {
            if (m_thinking) {
                m_thinking->hide();
            }
        } else {
            if (!m_thinking) {
                m_thinking = new QLabel(this);
                /* below the name, above the bubble */
                m_column->insertWidget(1, m_thinking, 0, Qt::AlignLeft);
            }
            const ChatPalette &palette = ChatTheme::palette();
            m_thinking->setStyleSheet(
                QStringLiteral("QLabel { color: %1; background: %2; border-radius: 10px;"
                               " padding: 5px 12px; font-size: 12px; }")
                    .arg(ChatTheme::css(active ? palette.accent : palette.textMuted),
                         ChatTheme::css(palette.noteBg)));
            m_thinking->setText(text);
            m_thinking->show();
        }
        updateBubble();
    }

signals:
    void copyRequested(int index);
    void editRequested(int index);
    void forkRequested(int index);
    void retryRequested(int index);
    void linkActivated(const QUrl &url);

private:
    /* an empty answer bubble under the thinking card is just noise */
    void updateBubble()
    {
        const bool thinking = m_thinking && !m_thinking->isHidden();
        m_bubble->setVisible(m_isUser || !thinking || !m_message.content.isEmpty());
    }

    ChatMessage m_message;
    int m_index = -1;
    bool m_isUser = false;
    bool m_isLast = false;
    QWidget *m_actions = nullptr;
    BubbleText *m_body = nullptr;
    QLabel *m_label = nullptr;
    QVBoxLayout *m_column = nullptr;
    QFrame *m_bubble = nullptr;
    QLabel *m_thinking = nullptr;
};

ChatMessageList::ChatMessageList(QWidget *parent)
    : QScrollArea(parent)
{
    setAcceptDrops(true);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    m_container = new QWidget;
    m_container->setStyleSheet(QStringLiteral("background: transparent;"));
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(8, 10, 8, 10);
    m_layout->setSpacing(6);
    m_layout->addStretch(1);
    setWidget(m_container);

    m_welcome = new QWidget(m_container);
    auto *welcomeLayout = new QVBoxLayout(m_welcome);
    welcomeLayout->setContentsMargins(24, 40, 24, 24);
    welcomeLayout->setSpacing(10);
    welcomeLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    auto *bigAvatar = new QLabel(tr("埃"), m_welcome);
    bigAvatar->setFixedSize(64, 64);
    bigAvatar->setAlignment(Qt::AlignCenter);
    m_welcomeAvatar = bigAvatar;
    welcomeLayout->addWidget(bigAvatar, 0, Qt::AlignHCenter);

    auto *title = new QLabel(tr("你好，我是小埃同学"), m_welcome);
    QFont titleFont = title->font();
    titleFont.setPixelSize(20);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    m_welcomeTitle = title;
    welcomeLayout->addWidget(title, 0, Qt::AlignHCenter);

    auto *hint = new QLabel(
        tr("把文件、链接或文字拖进窗口，或者直接输入问题。\n"
           "第一次使用请在左下角「设置」里填入 DeepSeek API Key。"),
        m_welcome);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    m_welcomeHint = hint;
    welcomeLayout->addWidget(hint, 0, Qt::AlignHCenter);

    restyleWelcome();

    m_layout->insertWidget(0, m_welcome);
}

void ChatMessageList::restyleWelcome()
{
    const ChatPalette &palette = ChatTheme::palette();
    m_welcomeAvatar->setStyleSheet(
        QStringLiteral("QLabel { background: %1; color: %2; border-radius: 32px;"
                       " font-size: 26px; }")
            .arg(ChatTheme::css(palette.welcomeAvatarBg),
                 ChatTheme::css(palette.welcomeAvatarText)));
    m_welcomeTitle->setStyleSheet(
        QStringLiteral("color: %1;").arg(ChatTheme::css(palette.textPrimary)));
    m_welcomeHint->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                     .arg(ChatTheme::css(palette.textMuted)));
}

void ChatMessageList::refreshTheme()
{
    /* Restyle in place of a user-initiated jump: keep the reading position. */
    const int scroll = verticalScrollBar()->value();
    restyleWelcome();
    rebuild(false);
    verticalScrollBar()->setValue(scroll);
}

void ChatMessageList::setMessages(const QVector<ChatMessage> &messages)
{
    m_messages = messages;
    rebuild();
}

void ChatMessageList::rebuild(bool scrollToBottom)
{
    for (MessageRow *row : m_rows) {
        m_layout->removeWidget(row);
        row->hide();
        row->deleteLater();
    }
    m_rows.clear();

    for (QWidget *note : m_notes) {
        m_layout->removeWidget(note);
        note->hide();
        note->deleteLater();
    }
    m_notes.clear();

    bool hasConversation = false;
    for (const ChatMessage &message : m_messages) {
        if (message.role == QStringLiteral("system")) {
            continue;
        }
        hasConversation = true;
    }
    m_welcome->setVisible(!hasConversation);

    const int lastAssistant = [this]() {
        int last = -1;
        for (int i = 0; i < m_messages.size(); ++i) {
            if (m_messages.at(i).role == QStringLiteral("assistant")) {
                last = i;
            }
        }
        return last;
    }();

    for (int i = 0; i < m_messages.size(); ++i) {
        const ChatMessage &message = m_messages.at(i);
        if (message.role == QStringLiteral("system")) {
            if (!message.content.isEmpty()) {
                auto *note = new QLabel(message.content, m_container);
                note->setWordWrap(true);
                note->setAlignment(Qt::AlignCenter);
                note->setStyleSheet(noteStyle());
                m_layout->insertWidget(m_layout->count() - 1, note);
                m_notes.append(note);
            }
            continue;
        }

        auto *row = new MessageRow(message, i, i == lastAssistant, m_container);
        connect(row, &MessageRow::copyRequested, this, &ChatMessageList::copyRequested);
        connect(row, &MessageRow::editRequested, this, &ChatMessageList::editRequested);
        connect(row, &MessageRow::forkRequested, this, &ChatMessageList::forkRequested);
        connect(row, &MessageRow::retryRequested, this, &ChatMessageList::retryRequested);
        connect(row, &MessageRow::linkActivated, this, &ChatMessageList::linkActivated);
        if (i == m_thinkingIndex) {
            row->setThinking(m_thinkingText, m_thinkingActive);
        }
        m_layout->insertWidget(m_layout->count() - 1, row);
        m_rows.append(row);
    }

    /* pending confirmations stay below the conversation */
    for (QWidget *card : std::as_const(m_permissions)) {
        m_layout->removeWidget(card);
        m_layout->insertWidget(m_layout->count() - 1, card);
    }

    if (scrollToBottom) {
        QTimer::singleShot(0, this, &ChatMessageList::scrollToBottom);
    }
}

void ChatMessageList::appendMessage(const ChatMessage &message)
{
    m_messages.append(message);
    rebuild();
}

void ChatMessageList::updateMessage(int index, const QString &content)
{
    if (index < 0 || index >= m_messages.size()) {
        return;
    }
    m_messages[index].content = content;

    for (MessageRow *row : m_rows) {
        if (row->rowIndex() == index) {
            row->setContent(content);
            break;
        }
    }
    QTimer::singleShot(0, this, &ChatMessageList::scrollToBottom);
}

void ChatMessageList::appendNote(const QString &text)
{
    auto *note = new QLabel(text, m_container);
    note->setWordWrap(true);
    note->setAlignment(Qt::AlignCenter);
    note->setStyleSheet(noteStyle());
    m_layout->insertWidget(m_layout->count() - 1, note);
    m_notes.append(note);
    QTimer::singleShot(0, this, &ChatMessageList::scrollToBottom);
}

void ChatMessageList::showPermission(const QString &key, const QString &title,
                                     const QString &detail,
                                     const QList<QPair<QString, QString>> &options)
{
    removePermission(key);
    const ChatPalette &palette = ChatTheme::palette();

    auto *card = new QFrame(m_container);
    card->setObjectName(QStringLiteral("permissionCard"));
    card->setMaximumWidth(kBubbleMaxWidth + 26);
    card->setStyleSheet(
        QStringLiteral("QFrame#permissionCard { background: %1; border: 1px solid %2;"
                       " border-radius: 12px; }")
            .arg(ChatTheme::css(palette.bubbleAssistant), ChatTheme::css(palette.accent)));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(6);

    auto *heading = new QLabel(tr("🔐 小埃想要使用工具，需要你确认"), card);
    heading->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;")
                               .arg(ChatTheme::css(palette.accent)));
    layout->addWidget(heading);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setWordWrap(true);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;"
                                             " font-family: monospace;")
                                  .arg(ChatTheme::css(palette.bubbleAssistantText)));
    layout->addWidget(titleLabel);

    if (!detail.isEmpty()) {
        auto *detailLabel = new QLabel(detail, card);
        detailLabel->setWordWrap(true);
        detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        detailLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                       .arg(ChatTheme::css(palette.textSecondary)));
        layout->addWidget(detailLabel);
    }

    auto *buttonRow = new QWidget(card);
    auto *buttons = new QHBoxLayout(buttonRow);
    buttons->setContentsMargins(0, 4, 0, 0);
    buttons->setSpacing(6);
    auto *decision = new QLabel(card);
    decision->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                .arg(ChatTheme::css(palette.textMuted)));
    decision->hide();

    bool first = true;
    for (const auto &option : options) {
        auto *button = new QPushButton(option.second, buttonRow);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        if (first) {
            button->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; color: %2; border: none;"
                               " border-radius: 6px; padding: 4px 12px; }"
                               "QPushButton:hover { background: %3; }")
                    .arg(ChatTheme::css(palette.accentStrong), ChatTheme::css(palette.accentText),
                         ChatTheme::css(palette.accentHover)));
            first = false;
        }
        const QString optionId = option.first;
        const QString label = option.second;
        connect(button, &QPushButton::clicked, this,
                [this, key, optionId, label, buttonRow, decision]() {
                    buttonRow->hide();
                    decision->setText(tr("已选择：%1").arg(label));
                    decision->show();
                    Q_EMIT permissionAnswered(key, optionId);
                });
        buttons->addWidget(button);
    }
    buttons->addStretch();
    layout->addWidget(buttonRow);
    layout->addWidget(decision);

    /* lined up with the assistant bubbles */
    auto *row = new QWidget(m_container);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(4 + kAvatarSize + 8, 2, 4, 2);
    rowLayout->addWidget(card);
    rowLayout->addStretch(1);

    m_layout->insertWidget(m_layout->count() - 1, row);
    row->setVisible(m_permissionsVisible);
    m_permissions.insert(key, row);
    QTimer::singleShot(0, this, &ChatMessageList::scrollToBottom);
}

void ChatMessageList::removePermission(const QString &key)
{
    if (QWidget *row = m_permissions.take(key)) {
        m_layout->removeWidget(row);
        row->hide();
        row->deleteLater();
    }
}

void ChatMessageList::setPermissionsVisible(bool visible)
{
    m_permissionsVisible = visible;
    for (QWidget *row : std::as_const(m_permissions)) {
        row->setVisible(visible);
    }
}

void ChatMessageList::setThinking(int index, const QString &text, bool active)
{
    if (index < 0 || text.isEmpty()) {
        index = -1;
    }
    const int previous = m_thinkingIndex;
    m_thinkingIndex = index;
    m_thinkingText = index < 0 ? QString() : text;
    m_thinkingActive = index >= 0 && active;

    for (MessageRow *row : std::as_const(m_rows)) {
        if (row->rowIndex() == index) {
            row->setThinking(m_thinkingText, m_thinkingActive);
        } else if (row->rowIndex() == previous) {
            row->setThinking(QString(), false);
        }
    }
    if (previous != index) {
        QTimer::singleShot(0, this, &ChatMessageList::scrollToBottom);
    }
}

void ChatMessageList::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData() &&
        (event->mimeData()->hasUrls() || event->mimeData()->hasText() ||
         event->mimeData()->hasImage())) {
        event->acceptProposedAction();
        return;
    }
    QScrollArea::dragEnterEvent(event);
}

void ChatMessageList::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData() &&
        (event->mimeData()->hasUrls() || event->mimeData()->hasText() ||
         event->mimeData()->hasImage())) {
        event->acceptProposedAction();
        return;
    }
    QScrollArea::dragMoveEvent(event);
}

void ChatMessageList::dropEvent(QDropEvent *event)
{
    if (event->mimeData() &&
        (event->mimeData()->hasUrls() || event->mimeData()->hasText() ||
         event->mimeData()->hasImage())) {
        Q_EMIT dropped(event->mimeData());
        event->acceptProposedAction();
        return;
    }
    QScrollArea::dropEvent(event);
}

void ChatMessageList::clear()
{
    m_messages.clear();
    rebuild();
}

void ChatMessageList::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

#include "chatview.moc"
