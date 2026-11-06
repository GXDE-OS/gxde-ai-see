// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>

/**
 * Thin wrapper around QSettings for the application configuration.
 *
 * Stored at ~/.config/gxde-ai-see/config.ini.  A GSettings schema will be
 * added once the setting set is stable.
 */
class Config : public QObject
{
    Q_OBJECT

public:
    static Config *instance();

    void init();

    QVariant get(const QString &key, const QVariant &fallback = QVariant()) const;
    bool getBool(const QString &key, bool fallback = false) const;
    int getInt(const QString &key, int fallback = 0) const;
    void set(const QString &key, const QVariant &value);
    void remove(const QString &key);
    void sync();

    QString dataDir() const;
    QString cacheDir() const;

private:
    explicit Config(QObject *parent = nullptr);

    QSettings *m_settings = nullptr;
    QString m_dataDir;
    QString m_cacheDir;
};
