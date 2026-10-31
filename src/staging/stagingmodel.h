// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QFileIconProvider>
#include <QHash>
#include <QIcon>
#include <QTimer>

#include "stagingitem.h"

class QMimeData;

/**
 * Model backing the staging area.
 *
 * Entries are persisted as JSON in the application data directory and entries
 * whose backing file disappeared are pruned periodically.
 */
class StagingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TypeRole = Qt::UserRole + 1,
        IdRole,
        NameRole,
        UriRole,
        TextRole,
        CheckedRole,
        PinnedRole,
        ExistsRole,
        IconRole,
    };

    explicit StagingModel(QObject *parent = nullptr);
    ~StagingModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addMimeData(const QMimeData *mimeData, int *addedRow = nullptr);
    void addFiles(const QStringList &localPaths, int *addedRow = nullptr);
    void addText(const QString &text, int *addedRow = nullptr);
    void addImage(const QImage &image, int *addedRow = nullptr);

    void setChecked(int row, bool checked);
    void setPinned(int row, bool pinned);
    void moveItem(int from, int to);
    void clearAll();

    const StagingItem *itemAt(int row) const;
    bool isEmpty() const { return m_items.isEmpty(); }
    int count() const { return m_items.size(); }

    void load();
    void save() const;

signals:
    void countChanged();
    void itemAdded(int row);

private:
    void insertItem(const StagingItem &item, int *addedRow);
    QIcon iconFor(const StagingItem &item) const;
    QString imageCacheDir() const;
    void pruneMissing();

    QList<StagingItem> m_items;
    QTimer m_pruneTimer;
    mutable QHash<QString, QIcon> m_iconCache;
    mutable QFileIconProvider m_iconProvider;
    QString m_storePath;
};
