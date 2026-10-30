// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QStringList>

namespace {
constexpr const char *kService = "org.gxde.Portal";
constexpr const char *kPath = "/org/gxde/Portal";
constexpr const char *kInterface = "org.gxde.Portal";
}

PortalClient *PortalClient::instance()
{
    static PortalClient *self = new PortalClient;
    return self;
}

PortalClient::PortalClient(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();

    if (auto *iface = bus.interface()) {
        m_available = iface->isServiceRegistered(QString::fromLatin1(kService));
        connect(iface, &QDBusConnectionInterface::serviceRegistered, this,
                [this](const QString &name) {
                    if (name == QString::fromLatin1(kService)) {
                        m_available = true;
                    }
                });
        connect(iface, &QDBusConnectionInterface::serviceUnregistered, this,
                [this](const QString &name) {
                    if (name == QString::fromLatin1(kService)) {
                        m_available = false;
                    }
                });
    }

    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface), QStringLiteral("DragStarted"), this,
                SLOT(onDragStarted(QString)));
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface), QStringLiteral("DragEnded"), this,
                SLOT(onDragEnded()));
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface), QStringLiteral("PortalDropped"), this,
                SLOT(onDropped(QString, bool)));
}

bool PortalClient::beginPortal(const QRect &panelRect)
{
    QDBusMessage message =
        QDBusMessage::createMethodCall(QString::fromLatin1(kService),
                                       QString::fromLatin1(kPath),
                                       QString::fromLatin1(kInterface),
                                       QStringLiteral("BeginPortal"));
    message << panelRect.x() << panelRect.y() << panelRect.width() << panelRect.height();
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 500);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return false;
    }
    return reply.arguments().constFirst().toBool();
}

void PortalClient::cancelPortal()
{
    QDBusMessage message =
        QDBusMessage::createMethodCall(QString::fromLatin1(kService),
                                       QString::fromLatin1(kPath),
                                       QString::fromLatin1(kInterface),
                                       QStringLiteral("CancelPortal"));
    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
}

bool PortalClient::mimeInteresting(const QString &mimeTypes)
{
    const QStringList types = mimeTypes.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &type : types) {
        if (type.startsWith(QLatin1String("text/")) || type.startsWith(QLatin1String("image/"))) {
            return true;
        }
    }
    return false;
}

void PortalClient::onDragStarted(const QString &mimeTypes)
{
    Q_EMIT dragStarted(mimeTypes);
}

void PortalClient::onDragEnded()
{
    Q_EMIT dragEnded();
}

void PortalClient::onDropped(const QString &uuid, bool accepted)
{
    Q_EMIT dropped(uuid, accepted);
}
