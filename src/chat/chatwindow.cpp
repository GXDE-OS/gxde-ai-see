// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatwindow.h"

#include "config.h"
#include "chatview.h"
#include "chattheme.h"

#include "../ui/aboutdialog.h"
#include "aicore.h"
#include "aicoreaccounts.h"
#include "markdown.h"

#include <DGuiApplicationHelper>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGroupBox>
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
#include <QTimer>
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
#include <QShortcut>
#include <QTextCursor>
#include <QTextBrowser>
#include <QIcon>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>

DGUI_USE_NAMESPACE

namespace {

bool mimeOk(const QMimeData *mime)
{
    return mime && (mime->hasUrls() || mime->hasText() || mime->hasImage());
}

/** A combo box the wheel only changes once it was clicked: scrolling past the
 *  approval mode must not switch it to "allow everything". */
class ClickOnlyComboBox : public QComboBox
{
public:
    using QComboBox::QComboBox;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        if (hasFocus()) {
            QComboBox::wheelEvent(event);
        } else {
            event->ignore();
        }
    }
};

} // namespace

class ChatSidebar : public QListWidget
{
    Q_OBJECT

public:
    explicit ChatSidebar(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setAcceptDrops(true);
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::MoveAction);
        setDropIndicatorShown(true);
        setDragDropOverwriteMode(false);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

signals:
    void droppedAt(int index, const QMimeData *mime);
    void left();
    void reordered();
    void deleteRequested(const QList<int> &rows);
    void renameRequested(int row);

protected:
    void leaveEvent(QEvent *event) override
    {
        Q_EMIT left();
        QListWidget::leaveEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->source() == this || mimeOk(event->mimeData())) {
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->source() == this || mimeOk(event->mimeData())) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        if (event->source() == this) {
            const QModelIndex dropIndex = indexAt(event->position().toPoint());
            const int to = dropIndex.isValid() ? dropIndex.row() : count();
            const int from = currentRow();
            if (from >= 0 && to >= 0 && to != from && to != from + 1) {
                QListWidgetItem *item = takeItem(from);
                const int target = to > from ? to - 1 : to;
                insertItem(target, item);
                setCurrentRow(target);
                Q_EMIT reordered();
            }
            event->acceptProposedAction();
            return;
        }
        if (!mimeOk(event->mimeData())) {
            event->ignore();
            return;
        }
        const QModelIndex index = indexAt(event->position().toPoint());
        Q_EMIT droppedAt(index.isValid() ? index.row() : -1, event->mimeData());
        event->acceptProposedAction();
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        const QModelIndex index = indexAt(event->pos());
        QMenu menu(this);
        QAction *rename = nullptr;
        if (index.isValid()) {
            rename = menu.addAction(tr("重命名对话"));
        }
        QAction *remove = menu.addAction(tr("删除选中"));
        QAction *chosen = menu.exec(event->globalPos());
        if (!chosen) {
            return;
        }
        if (chosen == rename) {
            Q_EMIT renameRequested(index.row());
            return;
        }
        if (chosen == remove) {
            QList<int> rows;
            for (const QModelIndex &selected : selectionModel()->selectedRows()) {
                rows.append(selected.row());
            }
            if (rows.isEmpty() && index.isValid()) {
                rows.append(index.row());
            }
            if (!rows.isEmpty()) {
                Q_EMIT deleteRequested(rows);
            }
        }
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

        const ChatPalette &palette = ChatTheme::palette();
        const QRect row = option.rect.adjusted(2, 2, -2, -2);
        const bool selected = option.state & QStyle::State_Selected;
        if (selected || index.row() == m_hoverRow) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? palette.highlight : palette.hover);
            painter->drawRoundedRect(row, 8, 8);
        }

        QFont titleFont = option.font;
        titleFont.setPixelSize(13);
        titleFont.setWeight(QFont::DemiBold);
        painter->setFont(titleFont);
        painter->setPen(selected ? palette.textPrimary : palette.listTitle);
        const QFontMetrics titleMetrics(titleFont);
        painter->drawText(QRect(row.left() + 10, row.top() + 9, row.width() - 70, 18),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                  Qt::ElideRight, row.width() - 70));

        QFont smallFont = option.font;
        smallFont.setPixelSize(10);
        painter->setFont(smallFont);
        painter->setPen(palette.textFaint);
        painter->drawText(QRect(row.right() - 56, row.top() + 10, 46, 14),
                          Qt::AlignRight | Qt::AlignVCenter,
                          index.data(kTimeRole).toString());

        smallFont.setPixelSize(11);
        painter->setFont(smallFont);
        painter->setPen(palette.textMuted);
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
    load();

    /* DeepSeek renamed the API models; map the old presets before the key
     * of older versions moves into an AI Core account. */
    const QString storedModel =
        Config::instance()->get(QStringLiteral("chat/model")).toString();
    if (storedModel == QStringLiteral("deepseek-chat")) {
        Config::instance()->set(QStringLiteral("chat/model"), QStringLiteral("deepseek-flash"));
        Config::instance()->sync();
    } else if (storedModel == QStringLiteral("deepseek-reasoner")) {
        Config::instance()->set(QStringLiteral("chat/model"), QStringLiteral("deepseek-v4-pro"));
        Config::instance()->sync();
    }
    AiCoreAccountsPanel::importLegacyKey();
    refreshAccountLabel();
    connect(AiCore::instance(), &AiCore::accountsChanged, this,
            &ChatWindow::refreshAccountLabel);
    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("ActionInvoked"), this,
        SLOT(onNotificationAction(uint, QString)));

    m_current = m_conversations.isEmpty() ? -1 : qBound(0, m_current, m_conversations.size() - 1);
    updateSidebar();
    renderConversation();

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        move(screen->availableGeometry().center() - rect().center());
    }

    connect(m_client, &ChatClient::delta, this, [this](const QString &text) {
        ChatMessage *message = streamingMessage();
        if (!message) {
            return;
        }
        stopThinking();
        message->content += text;
        if (viewingStream()) {
            m_messages->updateMessage(m_streamingIndex, message->content);
        }
    });
    connect(m_client, &ChatClient::finished, this, [this](const QString &stopReason) {
        if (ChatMessage *streamed = streamingMessage()) {
            ChatMessage &message = *streamed;
            QString note;
            if (stopReason == QStringLiteral("max_tokens")) {
                note = tr("（回复达到长度上限，已截断）");
            } else if (stopReason == QStringLiteral("max_turn_requests")) {
                note = tr("（工具调用轮数达到上限，已停止）");
            } else if (stopReason == QStringLiteral("refusal")) {
                note = tr("（模型拒绝回答）");
            } else if (stopReason == QStringLiteral("cancelled")) {
                note = tr("（已停止）");
            }
            if (!note.isEmpty()) {
                message.content += (message.content.isEmpty() ? QString() : QStringLiteral("\n\n"))
                    + note;
            }
            m_conversations[streamingConversation()].updatedAt =
                QDateTime::currentMSecsSinceEpoch();
        }
        endTurn();
    });
    connect(m_client, &ChatClient::toolCall, this,
            [this](const QString &toolCallId, const QString &title, const QString &kind,
                   const QString &status) {
                if (!streamingMessage()) {
                    return;
                }
                const bool done = status == QStringLiteral("completed") ||
                                  status == QStringLiteral("failed");
                /* the model is busy again once the tool handed back its result */
                if (done) {
                    startThinking();
                } else {
                    stopThinking();
                }
                ChatMessage &message = *streamingMessage();
                const QString name = title.isEmpty() ? kind : title;
                QString line;
                if (!m_turnTools.contains(toolCallId)) {
                    m_turnTools.insert(toolCallId);
                    line = tr("🔧 %1").arg(name);
                } else if (status == QStringLiteral("failed")) {
                    line = tr("⚠️ 工具未完成：%1").arg(name);
                }
                if (line.isEmpty()) {
                    return;
                }
                if (!message.content.isEmpty()) {
                    message.content += message.content.endsWith(QLatin1Char('\n'))
                                           ? QStringLiteral("\n")
                                           : QStringLiteral("\n\n");
                }
                message.content += line + QLatin1Char('\n');
                if (viewingStream()) {
                    m_messages->updateMessage(m_streamingIndex, message.content);
                }
            });
    connect(m_client, &ChatClient::permissionRequested, this, &ChatWindow::askPermission);
    /* AI Core retries failed model requests silently: say so */
    m_waitTimer = new QTimer(this);
    m_waitTimer->setInterval(5000);
    connect(m_waitTimer, &QTimer::timeout, this, &ChatWindow::onTurnWait);
    connect(m_client, &ChatClient::activity, this, &ChatWindow::noteTurnActivity);
    m_thinkTimer = new QTimer(this);
    m_thinkTimer->setInterval(1000);
    connect(m_thinkTimer, &QTimer::timeout, this, &ChatWindow::showThinking);
    connect(m_client, &ChatClient::thought, this, &ChatWindow::startThinking);
    connect(m_client, &ChatClient::permissionDropped, this, [this](const QString &key) {
        m_messages->removePermission(key);
        m_permissionKeys.remove(key);
    });
    connect(m_messages, &ChatMessageList::permissionAnswered, this,
            [this](const QString &key, const QString &optionId) {
                m_client->answerPermission(key, optionId);
            });
    connect(m_client, &ChatClient::failed, this, [this](const QString &error) {
        if (ChatMessage *streamed = streamingMessage()) {
            ChatMessage &message = *streamed;
            /* keep what already arrived; a bare error starts with the marker
             * so it is left out when the conversation is replayed */
            message.content = message.content.trimmed().isEmpty()
                                  ? tr("⚠️ %1").arg(error)
                                  : message.content + QStringLiteral("\n\n") +
                                        tr("⚠️ %1").arg(error);
        }
        endTurn();
    });
}

void ChatWindow::noteTurnActivity()
{
    m_lastActivity.restart();
    if (m_waitNoteShown) {
        m_waitNoteShown = false;
        m_status->hide();
    }
}

void ChatWindow::onTurnWait()
{
    if (!m_client->busy()) {
        m_waitTimer->stop();
        return;
    }
    /* waiting for the user, not for the model */
    if (!m_permissionKeys.isEmpty()) {
        m_lastActivity.restart();
        return;
    }
    const qint64 seconds = m_lastActivity.elapsed() / 1000;
    if (seconds < 20) {
        return;
    }
    QString text = tr("还在等待模型回应（%1 秒）…").arg(seconds);
    if (seconds >= 45) {
        text += QLatin1Char('\n') +
                tr("GXDE AI Core 遇到网络或接口错误时会在后台反复重试，可能持续几分钟。"
                   "请在「设置」里检查当前账号的 API 地址（OpenAI 兼容地址一般以 /v1 结尾）"
                   "和网络，或在「⋯」菜单里选「停止回复」。");
    }
    m_status->setText(text);
    m_status->show();
    m_waitNoteShown = true;
}

namespace {

QString thinkingDuration(qint64 ms)
{
    const qint64 seconds = ms / 1000;
    if (seconds < 60) {
        return QCoreApplication::translate("ChatWindow", "%1秒").arg(seconds);
    }
    return QCoreApplication::translate("ChatWindow", "%1分%2秒")
        .arg(seconds / 60)
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

void ChatWindow::startThinking()
{
    if (m_thinking || !streamingMessage()) {
        return;
    }
    m_thinking = true;
    m_thinkCard = true;
    m_thinkClock.start();
    m_thinkTimer->start();
    showThinking();
}

void ChatWindow::stopThinking()
{
    if (!m_thinking) {
        return;
    }
    m_thinking = false;
    m_thoughtMs += m_thinkClock.elapsed();
    m_thinkTimer->stop();
    showThinking();
}

void ChatWindow::resetThinking()
{
    m_thinkTimer->stop();
    m_thinking = false;
    m_thinkCard = false;
    m_thoughtMs = 0;
    m_messages->setThinking(-1, QString(), false);
}

void ChatWindow::showThinking()
{
    if (!m_client->busy() || !streamingMessage()) {
        resetThinking();
        return;
    }
    if (!m_thinkCard || !viewingStream()) {
        m_messages->setThinking(-1, QString(), false);
        return;
    }
    const qint64 ms = m_thoughtMs + (m_thinking ? m_thinkClock.elapsed() : 0);
    const QString text = m_thinking ? tr("💭 思考中 %1").arg(thinkingDuration(ms))
                                    : tr("💭 已思考 %1").arg(thinkingDuration(ms));
    m_messages->setThinking(m_streamingIndex, text, m_thinking);
}

int ChatWindow::streamingConversation() const
{
    if (m_streamingId.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_conversations.size(); ++i) {
        if (m_conversations.at(i).id == m_streamingId) {
            return i;
        }
    }
    return -1;
}

ChatMessage *ChatWindow::streamingMessage()
{
    const int conversation = streamingConversation();
    if (conversation < 0 || m_streamingIndex < 0 ||
        m_streamingIndex >= m_conversations.at(conversation).messages.size()) {
        return nullptr;
    }
    return &m_conversations[conversation].messages[m_streamingIndex];
}

bool ChatWindow::viewingStream() const
{
    return !m_streamingId.isEmpty() && m_current >= 0 && m_current < m_conversations.size() &&
           m_conversations.at(m_current).id == m_streamingId;
}

void ChatWindow::endTurn()
{
    resetThinking();
    m_waitTimer->stop();
    if (m_waitNoteShown) {
        m_waitNoteShown = false;
        m_status->hide();
    }
    m_streamingId.clear();
    m_streamingIndex = -1;
    m_turnTools.clear();
    for (const QString &key : std::as_const(m_permissionKeys)) {
        m_messages->removePermission(key);
    }
    m_permissionKeys.clear();
    save();
    updateSidebar();
    renderConversation();
    updateSendButton();
}

void ChatWindow::updateSendButton()
{
    m_sendButton->setEnabled(!m_input->toPlainText().trimmed().isEmpty() && !m_client->busy());
}

void ChatWindow::refreshAccountLabel()
{
    AiCoreAccountsPanel::describeActive(this, [this](const QString &label) {
        m_accountLabel = label;
        updateHeader();
    });
}

void ChatWindow::askPermission(const QString &key, const AiCorePermission &request)
{
    stopThinking();
    const QJsonObject call = request.toolCall;
    QString title = call.value(QStringLiteral("title")).toString().trimmed();
    if (title.isEmpty()) {
        title = call.value(QStringLiteral("kind")).toString();
    }

    QStringList details;
    for (const QJsonValue &value : call.value(QStringLiteral("content")).toArray()) {
        const QJsonObject item = value.toObject();
        const QString type = item.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("content")) {
            const QString text = item.value(QStringLiteral("content"))
                                     .toObject()
                                     .value(QStringLiteral("text"))
                                     .toString()
                                     .trimmed();
            if (!text.isEmpty()) {
                details.append(text);
            }
        } else if (type == QStringLiteral("diff")) {
            details.append(tr("修改文件：%1").arg(item.value(QStringLiteral("path")).toString()));
        }
    }
    for (const QJsonValue &value : call.value(QStringLiteral("locations")).toArray()) {
        const QString path = value.toObject().value(QStringLiteral("path")).toString();
        if (!path.isEmpty() && !details.join(QLatin1Char('\n')).contains(path)) {
            details.append(path);
        }
    }
    QString detail = details.join(QLatin1Char('\n'));
    if (detail.size() > 600) {
        detail = detail.left(600) + QStringLiteral("…");
    }

    /* allow first: the highlighted button is the one most answers pick */
    QList<QPair<QString, QString>> options;
    const QStringList order{ QStringLiteral("allow_once"), QStringLiteral("allow_always"),
                             QStringLiteral("reject_once"), QStringLiteral("reject_always") };
    for (const QString &kind : order) {
        for (const QJsonObject &option : request.options) {
            if (option.value(QStringLiteral("kind")).toString() != kind) {
                continue;
            }
            QString label = option.value(QStringLiteral("name")).toString();
            if (kind == QStringLiteral("allow_once")) {
                label = tr("允许");
            } else if (kind == QStringLiteral("allow_always")) {
                label = tr("本对话内总是允许");
            } else if (kind == QStringLiteral("reject_once")) {
                label = tr("拒绝");
            } else if (kind == QStringLiteral("reject_always")) {
                label = tr("总是拒绝");
            }
            options.append({ option.value(QStringLiteral("optionId")).toString(), label });
        }
    }
    if (options.isEmpty()) {
        m_client->answerPermission(key, QString());
        return;
    }

    m_permissionKeys.insert(key);
    m_messages->showPermission(key, title, detail, options);

    m_messages->setPermissionsVisible(viewingStream());
    if (!isActiveWindow() || !viewingStream()) {
        notifyPermission(title);
    }
}

void ChatWindow::notifyPermission(const QString &title)
{
    QDBusMessage notification = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    notification << QStringLiteral("gxde-ai-see") << m_notificationId
                 << QStringLiteral("gxde-ai-see") << tr("小埃同学需要你确认")
                 << tr("小埃想要执行：%1\n请回到对话窗口允许或拒绝。").arg(title)
                 << QStringList{ QStringLiteral("default"), tr("查看") } << QVariantMap()
                 << 0;
    QDBusConnection::sessionBus().callWithCallback(notification, this,
                                                   SLOT(onNotificationSent(uint)));
}

void ChatWindow::onNotificationSent(uint id)
{
    m_notificationId = id;
}

void ChatWindow::onNotificationAction(uint id, const QString &action)
{
    if (id == m_notificationId && !action.isEmpty()) {
        activateForAsk();
        /* the confirmation waits in the conversation of the running turn */
        const int streaming = streamingConversation();
        if (streaming >= 0) {
            selectConversation(streaming);
        }
    }
}

void ChatWindow::setupUi()
{
    /* Pick the palette before any child widget is built: message rows and the
     * welcome page read ChatTheme::palette() from their constructors.
     * UnknownType means "follow the system palette", which ChatTheme derives
     * from QApplication::palette(). */
    const DGuiApplicationHelper::ColorType themeType =
        DGuiApplicationHelper::instance()->themeType();
    if (themeType != DGuiApplicationHelper::UnknownType) {
        ChatTheme::setDark(themeType == DGuiApplicationHelper::DarkType);
    }
    ChatTheme::refresh();

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    /* ---------------- sidebar ---------------- */
    auto *sidebarPane = new QWidget(this);
    sidebarPane->setObjectName(QStringLiteral("sidebar"));
    sidebarPane->setFixedWidth(264);
    m_sidebarPane = sidebarPane;

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
    m_brand = brand;
    brandColumn->addWidget(brand);
    auto *brandSub = new QLabel(QStringLiteral("GXDE AI See"), sidebarPane);
    m_brandSub = brandSub;
    brandColumn->addWidget(brandSub);
    brandRow->addLayout(brandColumn);
    brandRow->addStretch();
    sidebarLayout->addLayout(brandRow);

    auto *newButton = new QPushButton(tr("＋ 新建对话"), sidebarPane);
    newButton->setCursor(Qt::PointingHandCursor);
    newButton->setFixedHeight(34);
    m_newButton = newButton;
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
    m_settingsButton = settingsButton;
    bottomRow->addWidget(settingsButton, 1);

    auto *menuButton = new QPushButton(QStringLiteral("⋯"), sidebarPane);
    menuButton->setCursor(Qt::PointingHandCursor);
    menuButton->setFixedWidth(38);
    m_menuButton = menuButton;
    bottomRow->addWidget(menuButton);
    sidebarLayout->addLayout(bottomRow);

    root->addWidget(sidebarPane);

    /* ---------------- chat pane ---------------- */
    auto *pane = new QWidget(this);
    m_chatPane = pane;
    auto *paneLayout = new QVBoxLayout(pane);
    paneLayout->setContentsMargins(0, 0, 0, 0);
    paneLayout->setSpacing(0);

    auto *header = new QWidget(pane);
    header->setFixedHeight(56);
    m_header = header;
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 16, 0);
    headerLayout->setSpacing(10);

    m_titleLabel = new QLabel(tr("小埃同学"), header);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPixelSize(15);
    titleFont.setWeight(QFont::DemiBold);
    m_titleLabel->setFont(titleFont);
    headerLayout->addWidget(m_titleLabel);

    m_modelLabel = new QLabel(header);
    m_modelLabel->setToolTip(tr("GXDE AI Core 当前账号与模型，可在「设置」里切换"));
    headerLayout->addWidget(m_modelLabel);
    headerLayout->addStretch();

    /* how the agent's tool calls get approved (ACP session mode) */
    auto *modeLabel = new QLabel(tr("工具权限"), header);
    headerLayout->addWidget(modeLabel);
    m_modeBox = new ClickOnlyComboBox(header);
    m_modeBox->setFocusPolicy(Qt::StrongFocus);
    m_modeBox->setToolTip(tr("每次确认：执行命令、改文件前在对话里询问\n"
                             "自动允许编辑：改文件不再询问，执行命令仍会询问\n"
                             "全部自动允许：不询问，直接执行\n"
                             "只读：只允许读取类工具"));
    for (const QString &mode : ChatClient::modeIds()) {
        m_modeBox->addItem(ChatClient::modeLabel(mode), mode);
    }
    headerLayout->addWidget(m_modeBox);
    paneLayout->addWidget(header);

    m_messages = new ChatMessageList(pane);
    paneLayout->addWidget(m_messages, 1);

    m_status = new QLabel(pane);
    m_status->setContentsMargins(20, 0, 20, 0);
    m_status->hide();
    paneLayout->addWidget(m_status);

    auto *inputPane = new QWidget(pane);
    auto *inputLayout = new QVBoxLayout(inputPane);
    inputLayout->setContentsMargins(20, 6, 20, 14);
    inputLayout->setSpacing(5);

    auto *inputFrame = new QFrame(inputPane);
    m_inputFrame = inputFrame;
    auto *frameLayout = new QHBoxLayout(inputFrame);
    frameLayout->setContentsMargins(12, 8, 8, 8);
    frameLayout->setSpacing(8);

    m_input = new QPlainTextEdit(inputFrame);
    m_input->setPlaceholderText(tr("输入消息，Ctrl+Enter 发送；也可以把文件/文字拖进窗口"));
    m_input->setFixedHeight(64);
    frameLayout->addWidget(m_input, 1);

    m_sendButton = new QPushButton(tr("发送"), inputFrame);
    m_sendButton->setFixedSize(72, 34);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    frameLayout->addWidget(m_sendButton, 0, Qt::AlignBottom);
    inputLayout->addWidget(inputFrame);

    auto *hint = new QLabel(tr("Enter 换行 · Ctrl+Enter 发送 · 支持拖入文件/链接/图片"), inputPane);
    hint->setAlignment(Qt::AlignRight);
    m_hint = hint;
    inputLayout->addWidget(hint);
    paneLayout->addWidget(inputPane);

    root->addWidget(pane, 1);

    /* ---------------- connections ---------------- */
    connect(newButton, &QPushButton::clicked, this, &ChatWindow::newConversation);
    connect(settingsButton, &QPushButton::clicked, this, &ChatWindow::openSettings);
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(m_input, &QPlainTextEdit::textChanged, this, &ChatWindow::updateSendButton);
    m_modeBox->setCurrentIndex(qMax(0, m_modeBox->findData(m_client->mode())));
    connect(m_modeBox, &QComboBox::activated, this, [this]() {
        m_client->setMode(m_modeBox->currentData().toString());
    });
    m_sendButton->setEnabled(false);

    connect(m_sidebar, &QListWidget::currentRowChanged, this, &ChatWindow::selectConversation);
    connect(static_cast<ChatSidebar *>(m_sidebar), &ChatSidebar::droppedAt, this,
            &ChatWindow::handleDropOnConversation);
    connect(static_cast<ChatSidebar *>(m_sidebar), &ChatSidebar::reordered, this,
            &ChatWindow::reorderConversations);
    connect(static_cast<ChatSidebar *>(m_sidebar), &ChatSidebar::deleteRequested, this,
            &ChatWindow::deleteConversations);
    connect(static_cast<ChatSidebar *>(m_sidebar), &ChatSidebar::renameRequested, this,
            &ChatWindow::renameConversation);
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
    connect(m_messages, &ChatMessageList::retryRequested, this, [this](int index) {
        retryFrom(index);
    });

    auto *menu = new QMenu(menuButton);
    menu->addAction(tr("停止回复"), this, [this]() {
        if (m_client->busy()) {
            m_client->cancel();
            endTurn();
        }
    });
    menu->addAction(tr("重试上一条"), this, &ChatWindow::retryLast);
    menu->addAction(tr("重启 GXDE AI Core"), this, [this]() {
        if (m_client->busy()) {
            m_client->cancel();
            endTurn();
        }
        AiCore::instance()->stop();
        refreshAccountLabel(); // starts it again
        m_status->setText(tr("GXDE AI Core 已重启，MCP 服务器会在下一条消息时重新连接"));
        m_status->show();
    });
    menu->addSeparator();
    menu->addAction(tr("关于"), this, &ChatWindow::showAbout);
    menuButton->setMenu(menu);

    auto *sendShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), m_input);
    connect(sendShortcut, &QShortcut::activated, this, &ChatWindow::sendMessage);
    auto *sendShortcut2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), m_input);
    connect(sendShortcut2, &QShortcut::activated, this, &ChatWindow::sendMessage);

    applyTheme();
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, [this](DGuiApplicationHelper::ColorType type) {
                if (type == DGuiApplicationHelper::UnknownType) {
                    ChatTheme::followSystem();
                } else {
                    ChatTheme::setDark(type == DGuiApplicationHelper::DarkType);
                }
                ChatTheme::refresh();
                applyTheme();
                m_messages->refreshTheme();
            });
}

void ChatWindow::applyTheme()
{
    const ChatPalette &palette = ChatTheme::palette();

    m_chatPane->setStyleSheet(
        QStringLiteral("background: %1;").arg(ChatTheme::css(palette.window)));

    m_sidebarPane->setStyleSheet(
        QStringLiteral("#sidebar { background: %1; border-right: 1px solid %2; }")
            .arg(ChatTheme::css(palette.sidebar), ChatTheme::css(palette.separator)));
    m_brand->setStyleSheet(QStringLiteral("color: %1;").arg(ChatTheme::css(palette.textPrimary)));
    m_brandSub->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;")
                                  .arg(ChatTheme::css(palette.textFaint)));

    m_newButton->setStyleSheet(
        QStringLiteral("QPushButton { color: %1; background: %2; border: none;"
                       " border-radius: 9px; font-size: 12px; }"
                       "QPushButton:hover { background: %3; }")
            .arg(ChatTheme::css(palette.accentText),
                 ChatTheme::css(palette.accentStrong),
                 ChatTheme::css(palette.accentHover)));

    const QString secondaryButton =
        QStringLiteral("QPushButton { color: %1; background: %2; border: none;"
                       " border-radius: 8px; padding: 6px 8px; font-size: 12px; }"
                       "QPushButton:hover { background: %3; color: %4; }")
            .arg(ChatTheme::css(palette.textSecondary),
                 ChatTheme::css(palette.inputBg),
                 ChatTheme::css(palette.hoverStrong),
                 ChatTheme::css(palette.textPrimary));
    m_settingsButton->setStyleSheet(secondaryButton);
    m_menuButton->setStyleSheet(secondaryButton);

    m_header->setStyleSheet(
        QStringLiteral("QWidget { background: %1; border-bottom: 1px solid %2; }")
            .arg(ChatTheme::css(palette.headerBg), ChatTheme::css(palette.separator)));

    m_titleLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(ChatTheme::css(palette.textPrimary)));

    QColor modelBadge = palette.accent;
    modelBadge.setAlphaF(0.13);
    m_modelLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; background: %2; border-radius: 8px;"
                       " padding: 2px 8px; font-size: 10px; }")
            .arg(ChatTheme::css(palette.badgeText), ChatTheme::css(modelBadge)));

    m_status->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                                .arg(ChatTheme::css(palette.status)));

    m_inputFrame->setStyleSheet(
        QStringLiteral("QFrame { background: %1; border: 1px solid %2;"
                       " border-radius: 12px; }")
            .arg(ChatTheme::css(palette.inputBg), ChatTheme::css(palette.inputBorder)));
    m_input->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: transparent; color: %1;"
                       " border: none; font-size: 13px; }")
            .arg(ChatTheme::css(palette.textPrimary)));

    m_sendButton->setStyleSheet(
        QStringLiteral("QPushButton { color: %1; background: %2; border: none;"
                       " border-radius: 9px; font-size: 13px; }"
                       "QPushButton:hover { background: %3; }"
                       "QPushButton:disabled { background: %4; color: %5; }")
            .arg(ChatTheme::css(palette.accentText),
                 ChatTheme::css(palette.accentStrong),
                 ChatTheme::css(palette.accentHover),
                 ChatTheme::css(palette.disabledBg),
                 ChatTheme::css(palette.textFaint)));

    m_hint->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;")
                              .arg(ChatTheme::css(palette.textFaint)));
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
    /* a running answer goes on in the background */
    m_current = index;
    renderConversation();
}

void ChatWindow::reorderConversations()
{
    if (m_conversations.isEmpty()) {
        return;
    }
    QHash<QString, ChatConversation> byId;
    for (const ChatConversation &conversation : m_conversations) {
        byId.insert(conversation.id, conversation);
    }
    QVector<ChatConversation> ordered;
    for (int i = 0; i < m_sidebar->count(); ++i) {
        const QString id = m_sidebar->item(i)->data(Qt::UserRole).toString();
        auto it = byId.find(id);
        if (it != byId.end()) {
            ordered.append(it.value());
            byId.erase(it);
        }
    }
    for (auto it = byId.begin(); it != byId.end(); ++it) {
        ordered.append(it.value());
    }
    m_conversations = ordered;
    const int row = m_sidebar->currentRow();
    if (row >= 0 && row < m_conversations.size()) {
        m_current = row;
    }
    save();
    updateSidebar();
}

void ChatWindow::deleteConversations(const QList<int> &rows)
{
    if (rows.isEmpty()) {
        return;
    }
    const QString currentId =
        m_current >= 0 && m_current < m_conversations.size() ? m_conversations.at(m_current).id
                                                             : QString();
    bool stopped = false;
    QList<int> sorted = rows;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int row : sorted) {
        if (row >= 0 && row < m_conversations.size()) {
            const QString id = m_conversations.at(row).id;
            if (id == m_streamingId && m_client->busy()) {
                m_client->cancel();
                stopped = true;
            }
            m_client->forget(id);
            m_conversations.removeAt(row);
        }
    }
    /* stay on the shown conversation when it survived */
    int current = -1;
    for (int i = 0; i < m_conversations.size(); ++i) {
        if (m_conversations.at(i).id == currentId) {
            current = i;
        }
    }
    m_current = current >= 0 ? current
                : m_conversations.isEmpty() ? -1
                                            : qBound(0, m_current, m_conversations.size() - 1);
    if (stopped) {
        endTurn(); // saves and redraws
        return;
    }
    save();
    updateSidebar();
    renderConversation();
}

void ChatWindow::renameConversation(int row)
{
    if (row < 0 || row >= m_conversations.size()) {
        return;
    }
    bool ok = false;
    const QString title = QInputDialog::getText(nullptr, tr("重命名对话"), tr("名称"),
                                                QLineEdit::Normal,
                                                m_conversations.at(row).title, &ok);
    if (ok && !title.trimmed().isEmpty()) {
        m_conversations[row].title = title.trimmed();
        save();
        updateSidebar();
        renderConversation();
    }
}

void ChatWindow::updateSidebar()
{
    const QSignalBlocker blocker(m_sidebar);
    m_sidebar->clear();
    for (const ChatConversation &conversation : m_conversations) {
        auto *item = new QListWidgetItem(conversation.title, m_sidebar);
        item->setData(Qt::UserRole, conversation.id);
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
    /* the turn's card and confirmations belong to its own conversation */
    showThinking();
    m_messages->setPermissionsVisible(viewingStream());
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
    m_modelLabel->setText(m_accountLabel.isEmpty() ? tr("未配置 AI Core 账号")
                                                   : m_accountLabel);
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
    conversation.updatedAt = QDateTime::currentMSecsSinceEpoch();
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

    ChatConversation &conversation = m_conversations[m_current];
    const QVector<ChatMessage> history = conversation.messages;
    conversation.messages.append(ChatMessage{ QStringLiteral("assistant"), QString() });
    m_streamingId = conversation.id;
    m_streamingIndex = conversation.messages.size() - 1;
    m_turnTools.clear();
    m_status->hide();
    renderConversation();
    /* GXDE AI Core keeps the session's history and compresses it itself */
    m_waitNoteShown = false;
    m_lastActivity.start();
    m_waitTimer->start();
    m_client->send(conversation.id, history);
    /* not every model streams its reasoning: the wait for the first word
     * counts as thinking too */
    startThinking();
    updateSendButton();
}

void ChatWindow::editMessage(int index)
{
    if (m_client->busy() || m_current < 0) {
        return;
    }
    ChatConversation &conversation = m_conversations[m_current];
    if (index < 0 || index >= conversation.messages.size()) {
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

void ChatWindow::retryFrom(int index)
{
    if (m_client->busy() || m_current < 0) {
        return;
    }
    ChatConversation &conversation = m_conversations[m_current];
    if (index < 0 || index >= conversation.messages.size()) {
        return;
    }
    if (conversation.messages.at(index).role != QStringLiteral("assistant")) {
        return;
    }

    /* drop everything from the assistant answer on, then regenerate it */
    conversation.messages.resize(index);
    conversation.updatedAt = QDateTime::currentMSecsSinceEpoch();
    save();
    renderConversation();
    requestCompletion();
}

void ChatWindow::retryLast()
{
    if (m_client->busy() || m_current < 0) {
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
    showAboutDialog(this, tr("GXDE 桌面的 AI 助手：传送门、内容工厂、待办与对话。"));
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
    dialog.setMinimumWidth(500);

    auto *layout = new QVBoxLayout(&dialog);

    /* accounts, keys and models belong to GXDE AI Core */
    auto *accountsGroup = new QGroupBox(tr("GXDE AI Core 账号"), &dialog);
    auto *accountsLayout = new QVBoxLayout(accountsGroup);
    accountsLayout->addWidget(new AiCoreAccountsPanel(accountsGroup));
    layout->addWidget(accountsGroup);

    auto *form = new QFormLayout;
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
    form->addRow(tr("额外要求"), systemEdit);
    layout->addLayout(form);

    auto *hint = new QLabel(
        tr("对话由 GXDE AI Core 处理：它保存账号与 API Key、管理上下文压缩，并运行工具和上面的 "
           "MCP 服务器。修改 MCP 或额外要求后，从下一条消息起生效。"),
        &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                            .arg(ChatTheme::css(ChatTheme::palette().textSecondary)));
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    const int result = dialog.exec();
    refreshAccountLabel();
    if (result != QDialog::Accepted) {
        return;
    }

    const QString oldMcp = Config::instance()->get(QStringLiteral("chat/mcpServers")).toString();
    const QString oldSystem =
        Config::instance()->get(QStringLiteral("chat/systemPrompt")).toString();
    Config::instance()->set(QStringLiteral("chat/systemPrompt"), systemEdit->toPlainText());
    const QString mcp = mcpEdit->toPlainText().trimmed();
    if (mcp.isEmpty() || QJsonDocument::fromJson(mcp.toUtf8()).isArray()) {
        Config::instance()->set(QStringLiteral("chat/mcpServers"), mcp);
    } else {
        m_status->setText(tr("MCP 配置不是合法的 JSON 数组，已忽略"));
        m_status->show();
    }
    Config::instance()->sync();

    /* sessions get MCP servers and instructions when they start */
    if (Config::instance()->get(QStringLiteral("chat/mcpServers")).toString() != oldMcp ||
        systemEdit->toPlainText() != oldSystem) {
        m_client->resetSessions();
    }
    renderConversation();
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
