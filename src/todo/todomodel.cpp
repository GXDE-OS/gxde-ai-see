// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todomodel.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include "config.h"

TodoModel *TodoModel::instance()
{
    static TodoModel *self = new TodoModel;
    return self;
}

TodoModel::TodoModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_path = Config::instance()->dataDir() + QStringLiteral("/todo.json");
    load();
}

int TodoModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant TodoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size()) {
        return QVariant();
    }
    const TodoItem &item = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return item.text;
    case DoneRole:
        return item.done;
    case IdRole:
        return item.id;
    default:
        return QVariant();
    }
}

bool TodoModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row < 0 || count <= 0 || row + count > m_items.size()) {
        return false;
    }
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        m_items.removeAt(row);
    }
    endRemoveRows();
    save();
    Q_EMIT countChanged();
    return true;
}

Qt::ItemFlags TodoModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
}

void TodoModel::add(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    TodoItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.text = trimmed;
    item.createdAt = QDateTime::currentMSecsSinceEpoch();

    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(item);
    endInsertRows();
    save();
    Q_EMIT countChanged();
}

void TodoModel::setDone(int row, bool done)
{
    if (row < 0 || row >= m_items.size() || m_items[row].done == done) {
        return;
    }
    m_items[row].done = done;
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, { DoneRole });
    save();
}

const TodoItem *TodoModel::itemAt(int row) const
{
    return (row >= 0 && row < m_items.size()) ? &m_items.at(row) : nullptr;
}

void TodoModel::load()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }

    QList<TodoItem> items;
    const QJsonArray array = doc.object().value(QStringLiteral("items")).toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        TodoItem item;
        item.id = object.value(QStringLiteral("id")).toString();
        item.text = object.value(QStringLiteral("text")).toString();
        item.done = object.value(QStringLiteral("done")).toBool();
        item.createdAt =
            static_cast<qint64>(object.value(QStringLiteral("createdAt")).toDouble());
        if (!item.text.isEmpty()) {
            items.append(item);
        }
    }

    if (items.isEmpty()) {
        return;
    }
    beginResetModel();
    m_items = items;
    endResetModel();
    Q_EMIT countChanged();
}

void TodoModel::save() const
{
    QJsonArray array;
    for (const TodoItem &item : m_items) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), item.id);
        object.insert(QStringLiteral("text"), item.text);
        object.insert(QStringLiteral("done"), item.done);
        object.insert(QStringLiteral("createdAt"), static_cast<double>(item.createdAt));
        array.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("items"), array);

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}
