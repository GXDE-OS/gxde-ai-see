// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stagingmodel.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>

#include "config.h"

namespace {

constexpr int kPruneIntervalMs = 2000;
constexpr int kDefaultMaxItems = 200;

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool isImageFile(const QString &path)
{
    QImageReader reader(path);
    return reader.canRead();
}

QString elidedName(const QString &text)
{
    QString line = text.simplified();
    if (line.size() > 60) {
        line = line.left(60) + QStringLiteral("…");
    }
    return line;
}

} // namespace

bool StagingItem::exists() const
{
    switch (type) {
    case Text:
        return !text.isEmpty();
    case File:
    case Image:
        return !uri.isEmpty() && QFileInfo::exists(uri);
    }
    return false;
}

QString StagingItem::dedupeKey() const
{
    switch (type) {
    case File:
        return QStringLiteral("f:") + uri;
    case Image:
        return QStringLiteral("i:") + uri;
    case Text:
        return QStringLiteral("t:") + text;
    }
    return id;
}

QJsonObject StagingItem::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), static_cast<int>(type));
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("uri"), uri);
    obj.insert(QStringLiteral("text"), text);
    obj.insert(QStringLiteral("name"), name);
    obj.insert(QStringLiteral("checked"), checked);
    obj.insert(QStringLiteral("createdAt"), createdAt);
    return obj;
}

StagingItem StagingItem::fromJson(const QJsonObject &obj)
{
    StagingItem item;
    item.type = static_cast<Type>(obj.value(QStringLiteral("type")).toInt());
    item.id = obj.value(QStringLiteral("id")).toString();
    item.uri = obj.value(QStringLiteral("uri")).toString();
    item.text = obj.value(QStringLiteral("text")).toString();
    item.name = obj.value(QStringLiteral("name")).toString();
    item.checked = obj.value(QStringLiteral("checked")).toBool();
    item.createdAt = static_cast<qint64>(obj.value(QStringLiteral("createdAt")).toDouble());
    if (item.id.isEmpty()) {
        item.id = newId();
    }
    return item;
}

StagingModel::StagingModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_storePath = Config::instance()->dataDir() + QStringLiteral("/staging.json");
    load();

    m_pruneTimer.setInterval(kPruneIntervalMs);
    connect(&m_pruneTimer, &QTimer::timeout, this, &StagingModel::pruneMissing);
    m_pruneTimer.start();
}

StagingModel::~StagingModel() = default;

int StagingModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant StagingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }

    const StagingItem &item = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return item.name;
    case TypeRole:
        return static_cast<int>(item.type);
    case IdRole:
        return item.id;
    case UriRole:
        return item.uri;
    case TextRole:
        return item.text;
    case CheckedRole:
        return item.checked;
    case ExistsRole:
        return item.exists();
    case IconRole:
        return iconFor(item);
    default:
        return QVariant();
    }
}

bool StagingModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row < 0 || count <= 0 || row + count > m_items.size()) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        m_iconCache.remove(m_items.at(row).id);
        m_items.removeAt(row);
    }
    endRemoveRows();

    save();
    Q_EMIT countChanged();
    return true;
}

Qt::ItemFlags StagingModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void StagingModel::addMimeData(const QMimeData *mimeData, int *addedRow)
{
    if (!mimeData) {
        return;
    }

    if (mimeData->hasUrls()) {
        QStringList paths;
        const QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            }
        }
        if (!paths.isEmpty()) {
            addFiles(paths, addedRow);
            return;
        }
    }

    if (mimeData->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (!image.isNull()) {
            addImage(image, addedRow);
            return;
        }
    }

    if (mimeData->hasText()) {
        addText(mimeData->text(), addedRow);
    }
}

void StagingModel::addFiles(const QStringList &localPaths, int *addedRow)
{
    QString lastPath;
    for (const QString &path : localPaths) {
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            continue;
        }

        StagingItem item;
        item.id = newId();
        item.uri = info.absoluteFilePath();
        item.name = info.fileName();
        item.createdAt = QDateTime::currentMSecsSinceEpoch();
        item.type = isImageFile(item.uri) ? StagingItem::Image : StagingItem::File;

        insertItem(item, addedRow);
        lastPath = item.uri;
    }
    Q_UNUSED(lastPath)
}

void StagingModel::addText(const QString &text, int *addedRow)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    StagingItem item;
    item.id = newId();
    item.type = StagingItem::Text;
    item.text = text;
    item.name = elidedName(trimmed);
    item.createdAt = QDateTime::currentMSecsSinceEpoch();

    insertItem(item, addedRow);
}

void StagingModel::addImage(const QImage &image, int *addedRow)
{
    if (image.isNull()) {
        return;
    }

    QByteArray png;
    {
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
    }

    const QString hash =
        QString::fromLatin1(QCryptographicHash::hash(png, QCryptographicHash::Sha1).toHex());
    const QString dir = imageCacheDir();
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/') + hash + QStringLiteral(".png");
    if (!QFileInfo::exists(path)) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(png);
            file.close();
        }
    }

    StagingItem item;
    item.id = newId();
    item.type = StagingItem::Image;
    item.uri = path;
    item.name = QStringLiteral("图片 %1")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    item.createdAt = QDateTime::currentMSecsSinceEpoch();

    insertItem(item, addedRow);
}

void StagingModel::setChecked(int row, bool checked)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    if (m_items[row].checked == checked) {
        return;
    }
    m_items[row].checked = checked;
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {CheckedRole});
    save();
}

void StagingModel::clearAll()
{
    if (m_items.isEmpty()) {
        return;
    }
    beginResetModel();
    m_items.clear();
    m_iconCache.clear();
    endResetModel();
    save();
    Q_EMIT countChanged();
}

const StagingItem *StagingModel::itemAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return nullptr;
    }
    return &m_items.at(row);
}

void StagingModel::insertItem(const StagingItem &item, int *addedRow)
{
    const QString key = item.dedupeKey();
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).dedupeKey() != key) {
            continue;
        }

        if (i != 0) {
            beginMoveRows(QModelIndex(), i, i, QModelIndex(), 0);
            m_items.move(i, 0);
            endMoveRows();
        }
        m_items[0].createdAt = item.createdAt;
        const QModelIndex idx = index(0);
        Q_EMIT dataChanged(idx, idx);
        save();
        if (addedRow) {
            *addedRow = 0;
        }
        Q_EMIT itemAdded(0);
        return;
    }

    const int maxItems = Config::instance()->getInt(QStringLiteral("staging/maxItems"),
                                                    kDefaultMaxItems);
    while (m_items.size() >= maxItems && !m_items.isEmpty()) {
        beginRemoveRows(QModelIndex(), m_items.size() - 1, m_items.size() - 1);
        m_iconCache.remove(m_items.last().id);
        m_items.removeLast();
        endRemoveRows();
    }

    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(item);
    endInsertRows();

    save();
    if (addedRow) {
        *addedRow = 0;
    }
    Q_EMIT countChanged();
    Q_EMIT itemAdded(0);
}

QIcon StagingModel::iconFor(const StagingItem &item) const
{
    auto it = m_iconCache.constFind(item.id);
    if (it != m_iconCache.constEnd()) {
        return it.value();
    }

    QIcon icon;
    switch (item.type) {
    case StagingItem::File:
        icon = m_iconProvider.icon(QFileInfo(item.uri));
        break;
    case StagingItem::Image: {
        QImageReader reader(item.uri);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (!image.isNull()) {
            icon = QIcon(QPixmap::fromImage(
                image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
        break;
    }
    case StagingItem::Text:
        icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
        break;
    }

    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
    }

    if (m_iconCache.size() > 512) {
        m_iconCache.clear();
    }
    m_iconCache.insert(item.id, icon);
    return icon;
}

QString StagingModel::imageCacheDir() const
{
    return Config::instance()->cacheDir() + QStringLiteral("/images");
}

void StagingModel::pruneMissing()
{
    QList<int> stale;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).exists()) {
            stale.append(i);
        }
    }
    if (stale.isEmpty()) {
        return;
    }

    for (int i = stale.size() - 1; i >= 0; --i) {
        removeRows(stale.at(i), 1);
    }
}

void StagingModel::load()
{
    QFile file(m_storePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }

    const QJsonArray array = doc.object().value(QStringLiteral("items")).toArray();
    QList<StagingItem> items;
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        StagingItem item = StagingItem::fromJson(value.toObject());
        if (item.exists()) {
            items.append(item);
        }
    }

    if (items.isEmpty()) {
        return;
    }

    beginResetModel();
    m_items = items;
    endResetModel();
}

void StagingModel::save() const
{
    QJsonArray array;
    for (const StagingItem &item : m_items) {
        array.append(item.toJson());
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("items"), array);

    QSaveFile file(m_storePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}
