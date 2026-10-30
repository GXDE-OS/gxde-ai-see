// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

struct TodoItem
{
    QString id;
    QString text;
    bool done = false;
    qint64 createdAt = 0;
};

/**
 * Persisted todo list backing the todo window and the "add to todo" action.
 */
class TodoModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        TextRole = Qt::UserRole + 1,
        DoneRole,
        IdRole,
    };

    static TodoModel *instance();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void add(const QString &text);
    void setDone(int row, bool done);
    const TodoItem *itemAt(int row) const;
    int count() const { return m_items.size(); }

    void load();
    void save() const;

signals:
    void countChanged();

private:
    explicit TodoModel(QObject *parent = nullptr);

    QList<TodoItem> m_items;
    QString m_path;
};
