// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include <QDir>
#include <QStandardPaths>

Config *Config::instance()
{
    static Config *self = new Config;
    return self;
}

Config::Config(QObject *parent)
    : QObject(parent)
{
}

void Config::init()
{
    if (m_settings) {
        return;
    }

    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);

    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_dataDir);

    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(m_cacheDir);

    m_settings = new QSettings(configDir + QStringLiteral("/config.ini"),
                               QSettings::IniFormat, this);
}

QVariant Config::get(const QString &key, const QVariant &fallback) const
{
    if (!m_settings) {
        return fallback;
    }
    return m_settings->value(key, fallback);
}

bool Config::getBool(const QString &key, bool fallback) const
{
    return get(key, fallback).toBool();
}

int Config::getInt(const QString &key, int fallback) const
{
    return get(key, fallback).toInt();
}

void Config::set(const QString &key, const QVariant &value)
{
    if (!m_settings) {
        return;
    }
    m_settings->setValue(key, value);
}

void Config::sync()
{
    if (m_settings) {
        m_settings->sync();
    }
}

QString Config::dataDir() const
{
    return m_dataDir;
}

QString Config::cacheDir() const
{
    return m_cacheDir;
}
