// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatwindow.h"

#include "config.h"
#include "attachmentuploader.h"
#include "service.h"
#include "chatview.h"
#include "chattheme.h"

#include "../ui/aboutdialog.h"
#include "../dialogs/flowlayout.h"
#include "markdown.h"
#include "mcpclient.h"

#include <DFileDialog>

#include <DGuiApplicationHelper>
#include <QCheckBox>
#include <QClipboard>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
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
#include <QResizeEvent>
#include <QSaveFile>
#include <QScreen>
#include <QSpinBox>
#include <QShortcut>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTextBrowser>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

DGUI_USE_NAMESPACE

namespace {

bool mimeOk(const QMimeData *mime)
{
    return mime && (mime->hasUrls() || mime->hasText() || mime->hasImage());
}

QString attachmentStoreDir()
{
    return Config::instance()->dataDir() + QStringLiteral("/chat/attachments");
}

QJsonObject attachmentToJson(const ChatAttachment &attachment)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), attachment.id);
    object.insert(QStringLiteral("name"), attachment.name);
    object.insert(QStringLiteral("path"), attachment.path);
    object.insert(QStringLiteral("mimeType"), attachment.mimeType);
    object.insert(QStringLiteral("size"), static_cast<double>(attachment.size));
    object.insert(QStringLiteral("kind"), static_cast<int>(attachment.kind));
    object.insert(QStringLiteral("fileId"), attachment.fileId);
    object.insert(QStringLiteral("truncated"), attachment.truncated);
    if (!attachment.text.isEmpty()) {
        object.insert(QStringLiteral("text"), attachment.text);
    }
    return object;
}

ChatAttachment attachmentFromJson(const QJsonObject &object)
{
    ChatAttachment attachment;
    attachment.id = object.value(QStringLiteral("id")).toString();
    attachment.name = object.value(QStringLiteral("name")).toString();
    attachment.path = object.value(QStringLiteral("path")).toString();
    attachment.mimeType = object.value(QStringLiteral("mimeType")).toString();
    attachment.size =
        static_cast<qint64>(object.value(QStringLiteral("size")).toDouble());
    const int kind = object.value(QStringLiteral("kind")).toInt();
    attachment.kind = kind == ChatAttachment::Image
        ? ChatAttachment::Image
        : (kind == ChatAttachment::Text ? ChatAttachment::Text : ChatAttachment::File);
    attachment.fileId = object.value(QStringLiteral("fileId")).toString();
    attachment.truncated = object.value(QStringLiteral("truncated")).toBool();
    attachment.text = object.value(QStringLiteral("text")).toString();
    if (attachment.id.isEmpty()) {
        attachment.id = AttachmentUtils::newId();
    }
    return attachment;
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

/**
 * Input box that turns files, images and very long text into attachments
 * instead of pasting a list of paths or a wall of text.
 */
class ChatInput : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ChatInput(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
    {
    }

signals:
    void attachMime(const QMimeData *mime);

protected:
    static bool wantsAttachment(const QMimeData *mime)
    {
        if (!mime) {
            return false;
        }
        return mime->hasUrls() || mime->hasImage() ||
            (mime->hasText() && AttachmentUtils::isLongText(mime->text()));
    }

    void insertFromMimeData(const QMimeData *source) override
    {
        if (wantsAttachment(source)) {
            Q_EMIT attachMime(source);
            return;
        }
        QPlainTextEdit::insertFromMimeData(source);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (wantsAttachment(event->mimeData())) {
            event->acceptProposedAction();
            return;
        }
        QPlainTextEdit::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (wantsAttachment(event->mimeData())) {
            event->acceptProposedAction();
            return;
        }
        QPlainTextEdit::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        if (wantsAttachment(event->mimeData())) {
            Q_EMIT attachMime(event->mimeData());
            event->acceptProposedAction();
            return;
        }
        QPlainTextEdit::dropEvent(event);
    }
};

/** One removable attachment in the composer strip. */
class AttachmentChip : public QFrame
{
    Q_OBJECT

public:
    AttachmentChip(const ChatAttachment &attachment, bool uploading,
                   QWidget *parent = nullptr)
        : QFrame(parent)
        , m_id(attachment.id)
    {
        const ChatPalette &palette = ChatTheme::palette();

        setObjectName(QStringLiteral("attachmentChip"));
        setFixedHeight(52);
        setMinimumWidth(170);
        setMaximumWidth(286);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 6, 4, 6);
        layout->setSpacing(8);

        auto *preview = new QLabel(this);
        preview->setFixedSize(38, 38);
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet(QStringLiteral("background: transparent;"));

        QPixmap pixmap;
        if (attachment.isImage() && !attachment.path.isEmpty()) {
            const QPixmap source(attachment.path);
            if (!source.isNull()) {
                pixmap = source.scaled(38, 38, Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
            }
        }
        if (pixmap.isNull()) {
            QIcon icon;
            if (!attachment.path.isEmpty()) {
                icon = QFileIconProvider().icon(QFileInfo(attachment.path));
            }
            if (icon.isNull()) {
                icon = style()->standardIcon(QStyle::SP_FileIcon);
            }
            if (!icon.isNull()) {
                pixmap = icon.pixmap(28, 28);
            }
        }
        if (!pixmap.isNull()) {
            preview->setPixmap(pixmap);
        } else {
            preview->setText(attachment.isImage() ? tr("图") : tr("文"));
            preview->setStyleSheet(
                QStringLiteral("QLabel { color: %1; background: transparent;"
                               " font-size: 12px; }")
                    .arg(ChatTheme::css(palette.textMuted)));
        }
        layout->addWidget(preview, 0, Qt::AlignVCenter);

        auto *column = new QVBoxLayout;
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(2);

        auto *name = new QLabel(this);
        QFont nameFont = name->font();
        nameFont.setPixelSize(12);
        nameFont.setWeight(QFont::Medium);
        name->setFont(nameFont);
        name->setText(QFontMetrics(nameFont).elidedText(attachment.name, Qt::ElideMiddle, 156));
        name->setToolTip(attachment.path.isEmpty() ? attachment.name : attachment.path);
        column->addWidget(name);

        auto *meta = new QLabel(this);
        QFont metaFont = meta->font();
        metaFont.setPixelSize(10);
        meta->setFont(metaFont);
        QString metaText;
        if (uploading) {
            metaText = tr("上传中…");
        } else if (attachment.isImage() && !attachment.fileId.isEmpty()) {
            metaText = tr("已上传 · %1")
                           .arg(AttachmentUtils::displaySize(attachment.size));
        } else if (attachment.kind == ChatAttachment::Text) {
            metaText = tr("长文本 · %1 字").arg(attachment.text.size());
        } else if (attachment.truncated) {
            metaText = tr("%1 · 已截断")
                           .arg(AttachmentUtils::displaySize(attachment.size));
        } else {
            metaText = AttachmentUtils::displaySize(attachment.size);
        }
        meta->setText(metaText);
        column->addWidget(meta);
        layout->addLayout(column, 1);

        auto *remove = new QToolButton(this);
        remove->setText(QStringLiteral("×"));
        remove->setToolTip(tr("移除附件"));
        remove->setCursor(Qt::PointingHandCursor);
        remove->setFocusPolicy(Qt::NoFocus);
        remove->setFixedSize(20, 20);
        remove->setStyleSheet(
            QStringLiteral("QToolButton { color: %1; background: transparent;"
                           " border: none; border-radius: 5px; font-size: 14px; }"
                           "QToolButton:hover { background: %2; color: %3; }")
                .arg(ChatTheme::css(palette.textMuted),
                     ChatTheme::css(palette.hoverStrong),
                     ChatTheme::css(palette.textPrimary)));
        connect(remove, &QToolButton::clicked, this,
                [this]() { Q_EMIT removeRequested(m_id); });
        layout->addWidget(remove, 0, Qt::AlignTop);

        setStyleSheet(
            QStringLiteral("QFrame#attachmentChip { background: %1; border: 1px solid %2;"
                           " border-radius: 9px; }")
                .arg(ChatTheme::css(palette.inputBg),
                     ChatTheme::css(palette.inputBorder)));
        name->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(ChatTheme::css(palette.textPrimary)));
        meta->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(ChatTheme::css(palette.textMuted)));
    }

signals:
    void removeRequested(const QString &id);

private:
    QString m_id;
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
    , m_uploader(new AttachmentUploader(this))
{
    setWindowTitle(tr("小埃同学"));
    setAcceptDrops(true);
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
        updateSendState();
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
        updateSendState();
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
        updateSendState();
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
        updateSendState();
    });

    connect(m_uploader, &AttachmentUploader::uploaded, this,
            [this](const QString &attachmentId, const QString &fileId) {
                m_uploading.remove(attachmentId);
                bool stored = false;
                for (ChatAttachment &attachment : m_pendingAttachments) {
                    if (attachment.id == attachmentId) {
                        attachment.fileId = fileId;
                    }
                }
                for (ChatConversation &conversation : m_conversations) {
                    for (ChatMessage &message : conversation.messages) {
                        for (ChatAttachment &attachment : message.attachments) {
                            if (attachment.id == attachmentId) {
                                attachment.fileId = fileId;
                                stored = true;
                            }
                        }
                    }
                }
                if (stored) {
                    save();
                    renderConversation();
                }
                updateAttachmentBar();
                if (m_sendAfterUploads && !waitingForUploads()) {
                    m_sendAfterUploads = false;
                    m_status->hide();
                    sendMessage();
                }
            });
    connect(m_uploader, &AttachmentUploader::failed, this,
            [this](const QString &attachmentId, const QString &error) {
                m_uploading.remove(attachmentId);
                updateAttachmentBar();
                QTimer::singleShot(0, this, [this, error]() {
                    showTransientStatus(
                        tr("附件上传失败：%1；发送时将直接内联图片").arg(error), 6000);
                });
                if (m_sendAfterUploads && !waitingForUploads()) {
                    m_sendAfterUploads = false;
                    sendMessage();
                }
            });
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
    /* QIcon::pixmap() already applies the screen's device pixel ratio, so at
     * fractional scale factors (1.25, 1.5) the result is larger than the label
     * unless the ratio is normalised to the label's logical size — otherwise
     * the bottom/right edge of the icon is clipped away. */
    QPixmap logoPixmap = QApplication::windowIcon().pixmap(QSize(34, 34));
    logoPixmap.setDevicePixelRatio(logoPixmap.width() / 34.0);
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

    m_modelLabel = new QLabel(QStringLiteral("deepseek-flash"), header);
    headerLayout->addWidget(m_modelLabel);
    headerLayout->addStretch();
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

    m_attachmentBar = new QWidget(inputPane);
    m_attachmentBar->setObjectName(QStringLiteral("attachmentBar"));
    m_attachmentBar->setStyleSheet(QStringLiteral("background: transparent;"));
    m_attachmentBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_attachmentLayout = new FlowLayout(m_attachmentBar, 0, 6);
    m_attachmentBar->hide();
    inputLayout->addWidget(m_attachmentBar);

    auto *inputFrame = new QFrame(inputPane);
    m_inputFrame = inputFrame;
    auto *frameLayout = new QHBoxLayout(inputFrame);
    frameLayout->setContentsMargins(8, 8, 8, 8);
    frameLayout->setSpacing(8);

    m_attachButton = new QToolButton(inputFrame);
    m_attachButton->setText(QStringLiteral("📎"));
    m_attachButton->setToolTip(tr("添加附件（也可以直接拖入文件、图片或长文本）"));
    m_attachButton->setCursor(Qt::PointingHandCursor);
    m_attachButton->setFocusPolicy(Qt::NoFocus);
    m_attachButton->setFixedSize(32, 34);
    frameLayout->addWidget(m_attachButton, 0, Qt::AlignBottom);

    m_input = new ChatInput(inputFrame);
    m_input->setPlaceholderText(
        tr("输入消息，Ctrl+Enter 发送；拖入文件、图片或长文本即可添加附件"));
    m_input->setFixedHeight(64);
    frameLayout->addWidget(m_input, 1);

    m_sendButton = new QPushButton(tr("发送"), inputFrame);
    m_sendButton->setFixedSize(72, 34);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    frameLayout->addWidget(m_sendButton, 0, Qt::AlignBottom);
    inputLayout->addWidget(inputFrame);

    auto *hint = new QLabel(
        tr("Enter 换行 · Ctrl+Enter 发送 · 文件、图片和超长文字会作为附件上传"), inputPane);
    hint->setAlignment(Qt::AlignRight);
    m_hint = hint;
    m_hintDefault = hint->text();
    inputLayout->addWidget(hint);
    paneLayout->addWidget(inputPane);

    root->addWidget(pane, 1);

    /* ---------------- connections ---------------- */
    connect(newButton, &QPushButton::clicked, this, &ChatWindow::newConversation);
    connect(settingsButton, &QPushButton::clicked, this, &ChatWindow::openSettings);
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(m_input, &QPlainTextEdit::textChanged, this, &ChatWindow::updateSendState);
    connect(m_input, &ChatInput::attachMime, this, &ChatWindow::handleDroppedMime);
    connect(m_attachButton, &QToolButton::clicked, this, &ChatWindow::openAttachmentPicker);
    updateSendState();

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
    m_attachButton->setStyleSheet(
        QStringLiteral("QToolButton { color: %1; background: transparent; border: none;"
                       " border-radius: 7px; font-size: 16px; }"
                       "QToolButton:hover { background: %2; }")
            .arg(ChatTheme::css(palette.textSecondary),
                 ChatTheme::css(palette.hoverStrong)));

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

    /* Chips pick colours from the palette, so rebuild them on theme changes. */
    updateAttachmentBar();
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
            for (const QJsonValue &attachmentValue :
                 messageObject.value(QStringLiteral("attachments")).toArray()) {
                ChatAttachment attachment =
                    attachmentFromJson(attachmentValue.toObject());
                if (attachment.isValid()) {
                    message.attachments.append(attachment);
                }
            }
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
            if (!message.attachments.isEmpty()) {
                QJsonArray attachments;
                for (const ChatAttachment &attachment : message.attachments) {
                    attachments.append(attachmentToJson(attachment));
                }
                object.insert(QStringLiteral("attachments"), attachments);
            }
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
    /* A send deferred until an attachment upload finishes must not leak into
     * the conversation the user just switched to. */
    m_sendAfterUploads = false;

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
    m_sendAfterUploads = false;
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
    if (m_client->busy()) {
        m_client->cancel();
        m_streamingIndex = -1;
    }
    QList<int> sorted = rows;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int row : sorted) {
        if (row >= 0 && row < m_conversations.size()) {
            m_conversations.removeAt(row);
        }
    }
    m_current = m_conversations.isEmpty() ? -1
                                         : qBound(0, m_current, m_conversations.size() - 1);
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
            if (!message.attachments.isEmpty()) {
                const ChatAttachment &attachment = message.attachments.first();
                preview = tr("[附件] %1").arg(attachment.name);
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
    if (m_client->busy() || m_compressing || m_sendAfterUploads) {
        return;
    }
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() && m_pendingAttachments.isEmpty()) {
        return;
    }

    /* Give in-flight Files API uploads a chance to finish so the message can
     * reference file_id instead of repeating base64 image data. */
    if (waitingForUploads()) {
        m_sendAfterUploads = true;
        m_status->setText(tr("正在上传附件…"));
        m_status->show();
        return;
    }

    if (m_current < 0) {
        newConversation();
    }

    ChatConversation &conversation = m_conversations[m_current];
    if (conversation.title == tr("新对话")) {
        QString title = text;
        if (title.isEmpty() && !m_pendingAttachments.isEmpty()) {
            title = m_pendingAttachments.first().name;
        }
        conversation.title = title.left(24);
    }
    conversation.messages.append(
        ChatMessage{ QStringLiteral("user"), text, m_pendingAttachments });
    conversation.updatedAt = QDateTime::currentMSecsSinceEpoch();
    m_skipCompress = false;
    m_toolScaffold = QJsonArray();
    m_toolRounds = 0;

    m_pendingAttachments.clear();
    m_input->clear();
    m_status->hide();
    updateAttachmentBar();
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
    updateSendState();
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
    const QVector<ChatMessage> &messages = m_conversations.at(m_current).messages;

    const int keep = Config::instance()->getInt(QStringLiteral("chat/contextLength"), 40);
    if (messages.size() > keep + 4) {
        return true;
    }

    /* also honour a token budget (128k / 1024k); a rough estimate is enough */
    const qint64 tokenLimit =
        Config::instance()->getInt(QStringLiteral("chat/contextTokens"), 128000);
    if (tokenLimit <= 0) {
        return false;
    }
    qint64 tokens = 0;
    for (const ChatMessage &message : messages) {
        for (const QChar &ch : message.content) {
            tokens += ch.unicode() < 128 ? 1 : 2;
        }
    }
    return tokens / 4 > tokenLimit;
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
        transcript += AttachmentUtils::promptText(message.content, message.attachments);
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
    updateSendState();
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
    const QVector<ChatAttachment> attachments =
        conversation.messages.at(index).attachments;
    if (text.isEmpty() && attachments.isEmpty()) {
        return;
    }

    conversation.messages = conversation.messages.mid(0, index);
    conversation.messages.append(ChatMessage{ QStringLiteral("user"), text, attachments });
    conversation.updatedAt = QDateTime::currentMSecsSinceEpoch();
    save();
    updateSidebar();
    requestCompletion();
}

void ChatWindow::retryFrom(int index)
{
    if (m_client->busy() || m_compressing || m_current < 0) {
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
    showAboutDialog(this, tr("GXDE 桌面的 AI 助手：传送门、内容工厂、待办与对话。"));
}

void ChatWindow::appendError(const QString &message)
{
    if (m_statusTimer) {
        m_statusTimer->stop();
    }
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

    auto *serviceBox =
        new QCheckBox(tr("启用传送门服务（开机自启与后台保活）"), &dialog);
    serviceBox->setChecked(PortalService::instance()->enabled());
    form->addRow(tr("后台服务"), serviceBox);

    auto *serviceHint = new QLabel(
        tr("关闭后会取消开机自启与后台保活，并停用传送门面板。可随时在这里重新打开。"),
        &dialog);
    serviceHint->setWordWrap(true);
    serviceHint->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                   .arg(ChatTheme::css(
                                       ChatTheme::palette().textSecondary)));
    form->addRow(QString(), serviceHint);

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

    auto *tokenBox = new QComboBox(&dialog);
    tokenBox->addItem(tr("128k"), 128000);
    tokenBox->addItem(tr("1024k"), 1024000);
    const int storedTokens =
        Config::instance()->getInt(QStringLiteral("chat/contextTokens"), 128000);
    const int tokenIndex = tokenBox->findData(storedTokens);
    if (tokenIndex >= 0) {
        tokenBox->setCurrentIndex(tokenIndex);
    }
    form->addRow(tr("上下文长度"), tokenBox);

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
    hint->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
                            .arg(ChatTheme::css(ChatTheme::palette().textSecondary)));
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
        Config::instance()->set(QStringLiteral("chat/contextTokens"),
                                tokenBox->currentData().toInt());
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
        PortalService::instance()->setEnabled(serviceBox->isChecked());
        renderConversation();
    }
}

void ChatWindow::addAttachment(const ChatAttachment &attachment)
{
    if (!attachment.isValid()) {
        showTransientStatus(tr("无法添加该附件"));
        return;
    }

    for (const ChatAttachment &existing : m_pendingAttachments) {
        if (!attachment.path.isEmpty() && existing.path == attachment.path &&
            existing.kind == attachment.kind) {
            showTransientStatus(tr("该附件已在列表中：%1").arg(attachment.name));
            return;
        }
        if (attachment.kind == ChatAttachment::Text &&
            existing.kind == ChatAttachment::Text && existing.text == attachment.text) {
            showTransientStatus(tr("该文本已在附件列表中"));
            return;
        }
    }

    m_pendingAttachments.append(attachment);
    if (attachment.isImage()) {
        startUpload(attachment);
    }
    updateAttachmentBar();
    showTransientStatus(
        attachment.kind == ChatAttachment::Text
            ? tr("已添加长文本附件（%1 字）").arg(attachment.text.size())
            : tr("已添加附件：%1").arg(attachment.name));
}

void ChatWindow::addAttachmentFromFile(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        showTransientStatus(tr("文件不存在：%1").arg(path));
        return;
    }
    if (info.isDir()) {
        /* Directories cannot be sent to the model; keep the path in the input. */
        insertInputText(info.absoluteFilePath());
        return;
    }

    const ChatAttachment attachment =
        AttachmentUtils::fromFile(info.absoluteFilePath(), attachmentStoreDir());
    if (!attachment.isValid()) {
        showTransientStatus(tr("无法读取附件：%1").arg(info.fileName()));
        return;
    }
    addAttachment(attachment);
}

void ChatWindow::openAttachmentPicker()
{
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QStringList files = Dtk::Widget::DFileDialog::getOpenFileNames(
        this, tr("添加附件"), documents,
        tr("所有文件 (*);;图片 (*.png *.jpg *.jpeg *.gif *.webp);;"
           "文本 (*.txt *.md *.json *.csv *.log *.xml *.yaml *.yml)"));
    for (const QString &file : files) {
        addAttachmentFromFile(file);
    }
}

void ChatWindow::removeAttachment(const QString &id)
{
    for (int i = 0; i < m_pendingAttachments.size(); ++i) {
        if (m_pendingAttachments.at(i).id == id) {
            m_pendingAttachments.removeAt(i);
            break;
        }
    }
    if (m_sendAfterUploads && !waitingForUploads()) {
        /* The user removed the attachment that the send was waiting for. */
        m_sendAfterUploads = false;
        m_status->hide();
    }
    updateAttachmentBar();
}

void ChatWindow::startUpload(const ChatAttachment &attachment)
{
    if (!attachment.isImage() || attachment.path.isEmpty()) {
        return;
    }
    if (m_uploader->isUploading(attachment.id)) {
        return;
    }
    m_uploading.insert(attachment.id);
    m_uploader->upload(attachment);
}

void ChatWindow::updateAttachmentBar()
{
    if (!m_attachmentLayout || !m_attachmentBar) {
        return;
    }

    while (QLayoutItem *item = m_attachmentLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    for (const ChatAttachment &attachment : m_pendingAttachments) {
        auto *chip = new AttachmentChip(attachment, m_uploading.contains(attachment.id),
                                        m_attachmentBar);
        connect(chip, &AttachmentChip::removeRequested, this,
                &ChatWindow::removeAttachment);
        m_attachmentLayout->addWidget(chip);
    }

    m_attachmentBar->setVisible(!m_pendingAttachments.isEmpty());
    QTimer::singleShot(0, this, &ChatWindow::syncAttachmentBarHeight);
    updateSendState();
}

void ChatWindow::syncAttachmentBarHeight()
{
    if (!m_attachmentBar || !m_attachmentLayout || !m_attachmentBar->isVisible()) {
        return;
    }
    const int width = m_attachmentBar->width();
    if (width <= 0) {
        return;
    }
    const int height = m_attachmentLayout->heightForWidth(width);
    if (height > 0 && m_attachmentBar->height() != height) {
        m_attachmentBar->setFixedHeight(height);
    }
}

void ChatWindow::updateSendState()
{
    if (!m_sendButton || !m_input) {
        return;
    }
    const bool hasText = !m_input->toPlainText().trimmed().isEmpty();
    const bool hasAttachments = !m_pendingAttachments.isEmpty();
    m_sendButton->setEnabled((hasText || hasAttachments) && !m_client->busy() &&
                             !m_compressing);
}

void ChatWindow::showTransientStatus(const QString &text, int milliseconds)
{
    if (!m_status) {
        return;
    }
    if (!m_statusTimer) {
        m_statusTimer = new QTimer(this);
        m_statusTimer->setSingleShot(true);
        connect(m_statusTimer, &QTimer::timeout, this, [this]() {
            if (m_status) {
                m_status->hide();
            }
        });
    }
    m_status->setText(text);
    m_status->show();
    m_statusTimer->start(qMax(1200, milliseconds));
}

bool ChatWindow::waitingForUploads() const
{
    for (const ChatAttachment &attachment : m_pendingAttachments) {
        if (m_uploading.contains(attachment.id)) {
            return true;
        }
    }
    return false;
}

void ChatWindow::insertInputText(const QString &text)
{
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

void ChatWindow::handleDroppedMime(const QMimeData *mime)
{
    if (!mime) {
        return;
    }

    bool handled = false;
    if (mime->hasUrls()) {
        QStringList links;
        bool localFile = false;
        for (const QUrl &url : mime->urls()) {
            if (url.isLocalFile()) {
                const QFileInfo info(url.toLocalFile());
                if (info.isFile()) {
                    addAttachmentFromFile(info.absoluteFilePath());
                    localFile = true;
                } else {
                    links.append(info.absoluteFilePath());
                }
            } else if (url.isValid() && !url.scheme().isEmpty()) {
                links.append(url.toString());
            }
        }

        if (!localFile && mime->hasImage()) {
            /* Dragging an image out of a browser carries a URL *and* the
             * pixels; attach the pixels instead of pasting the URL. */
            const QImage image = qvariant_cast<QImage>(mime->imageData());
            if (!image.isNull()) {
                const QString name =
                    tr("图片 %1.png")
                        .arg(QDateTime::currentDateTime().toString(
                            QStringLiteral("yyyyMMdd-HHmmss")));
                const ChatAttachment attachment =
                    AttachmentUtils::fromImage(image, name, attachmentStoreDir());
                if (attachment.isValid()) {
                    addAttachment(attachment);
                    handled = true;
                }
            }
        }

        if (!handled) {
            if (!links.isEmpty()) {
                insertInputText(links.join(QLatin1Char('\n')));
            }
            handled = true;
        }
    }

    if (!handled && mime->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            const QString name =
                tr("图片 %1.png")
                    .arg(QDateTime::currentDateTime().toString(
                        QStringLiteral("yyyyMMdd-HHmmss")));
            const ChatAttachment attachment =
                AttachmentUtils::fromImage(image, name, attachmentStoreDir());
            if (attachment.isValid()) {
                addAttachment(attachment);
                handled = true;
            }
        }
    }

    if (!handled && mime->hasText()) {
        const QString text = mime->text();
        if (!text.trimmed().isEmpty()) {
            if (AttachmentUtils::isLongText(text)) {
                addAttachment(AttachmentUtils::fromText(text));
            } else {
                insertInputText(text);
            }
        }
    }

    updateSendState();
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

    QStringList added;
    for (const QString &file : files) {
        const QFileInfo info(file);
        if (info.isFile()) {
            const ChatAttachment attachment =
                AttachmentUtils::fromFile(info.absoluteFilePath(), attachmentStoreDir());
            if (attachment.isValid()) {
                addAttachment(attachment);
                added.append(attachment.name);
            } else {
                insertInputText(file);
            }
        } else if (!file.isEmpty()) {
            insertInputText(file);
        }
    }

    if (!text.isEmpty()) {
        if (AttachmentUtils::isLongText(text)) {
            addAttachment(AttachmentUtils::fromText(text));
        } else {
            insertInputText(text);
        }
    }

    QString title = text;
    if (title.isEmpty() && !added.isEmpty()) {
        title = added.first();
    }
    if (m_current >= 0 && !title.isEmpty()) {
        m_conversations[m_current].title = title.simplified().left(24);
        save();
        updateSidebar();
        renderConversation();
    }
    updateSendState();
    m_input->setFocus();
}

void ChatWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (mimeOk(event->mimeData())) {
        event->acceptProposedAction();
        m_hint->setText(tr("松开即可添加为附件"));
        return;
    }
    QWidget::dragEnterEvent(event);
}

void ChatWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if (mimeOk(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragMoveEvent(event);
}

void ChatWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_hint->setText(m_hintDefault);
    QWidget::dragLeaveEvent(event);
}

void ChatWindow::dropEvent(QDropEvent *event)
{
    m_hint->setText(m_hintDefault);
    if (!mimeOk(event->mimeData())) {
        event->ignore();
        return;
    }
    handleDroppedMime(event->mimeData());
    event->acceptProposedAction();
}

void ChatWindow::resizeEvent(QResizeEvent *event)
{
    Dtk::Widget::DMainWindow::resizeEvent(event);
    syncAttachmentBarHeight();
}

void ChatWindow::showSettings()
{
    openSettings();
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
    syncAttachmentBarHeight();
}

#include "chatwindow.moc"
