/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "castdevicemodel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkInterface>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {

const QHostAddress MdnsGroup(QStringLiteral("224.0.0.251"));
const quint16 MdnsPort = 5353;
const char *CastService = "_googlecast._tcp.local";
const qint64 DeviceExpiryMs = 120000;

quint16 read16(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 1 >= data.size()) {
        return 0;
    }
    return (quint16(quint8(data.at(offset))) << 8)
            | quint16(quint8(data.at(offset + 1)));
}

QByteArray encodedDnsName(const QString &name)
{
    QByteArray result;
    const QStringList labels = name.split(QLatin1Char('.'), QString::SkipEmptyParts);
    for (int i = 0; i < labels.size(); ++i) {
        const QByteArray label = labels.at(i).toUtf8();
        if (label.isEmpty() || label.size() > 63) {
            return QByteArray();
        }
        result.append(char(label.size()));
        result.append(label);
    }
    result.append(char(0));
    return result;
}

QString normalizedName(const QString &name)
{
    QString value = name.trimmed();
    while (value.endsWith(QLatin1Char('.'))) {
        value.chop(1);
    }
    return value.toLower();
}

QString defaultStorageDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        base = QDir::home().filePath(QStringLiteral(".local/share"));
    }

    const QString organization = QCoreApplication::organizationName().trimmed().isEmpty()
            ? QStringLiteral("org.edp17")
            : QCoreApplication::organizationName().trimmed();
    const QString application = QCoreApplication::applicationName().trimmed().isEmpty()
            ? QStringLiteral("SailVideo")
            : QCoreApplication::applicationName().trimmed();
    return QDir(base).filePath(organization + QLatin1Char('/') + application);
}

} // namespace

CastDeviceModel::CastDeviceModel(const QString &storageDirectory, QObject *parent)
    : QAbstractListModel(parent)
    , m_storageDirectory(storageDirectory.trimmed().isEmpty()
                         ? defaultStorageDirectory()
                         : storageDirectory.trimmed())
{
    connect(&m_socket, SIGNAL(readyRead()),
            this, SLOT(readPendingDatagrams()));

    m_queryTimer.setInterval(2500);
    connect(&m_queryTimer, SIGNAL(timeout()),
            this, SLOT(sendQuery()));

    m_expiryTimer.setInterval(15000);
    connect(&m_expiryTimer, SIGNAL(timeout()),
            this, SLOT(expireOldDevices()));

    loadCachedDevices();
}

CastDeviceModel::~CastDeviceModel()
{
    stopDiscovery();
}

int CastDeviceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_devices.size();
}

QVariant CastDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size()) {
        return QVariant();
    }

    const Device &device = m_devices.at(index.row());
    switch (role) {
    case NameRole:
    case Qt::DisplayRole:
        return device.name;
    case HostRole:
        return device.host;
    case PortRole:
        return device.port;
    case ModelRole:
        return device.model;
    case UuidRole:
        return device.uuid;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> CastDeviceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "name");
    roles.insert(HostRole, "host");
    roles.insert(PortRole, "port");
    roles.insert(ModelRole, "modelName");
    roles.insert(UuidRole, "uuid");
    return roles;
}

int CastDeviceModel::count() const
{
    return m_devices.size();
}

bool CastDeviceModel::discovering() const
{
    return m_discovering;
}

QString CastDeviceModel::lastError() const
{
    return m_lastError;
}

QString CastDeviceModel::storagePath() const
{
    return QDir(m_storageDirectory).filePath(QStringLiteral("chromecast-devices.json"));
}

void CastDeviceModel::startDiscovery()
{
    if (m_discovering) {
        sendQuery();
        return;
    }

    if (!bindSocket()) {
        return;
    }

    setLastError(QString());
    setDiscovering(true);
    m_queryTimer.start();
    m_expiryTimer.start();

    qInfo() << "SailVideo Cast: mDNS discovery started for" << CastService;
    sendQuery();
}

void CastDeviceModel::refresh()
{
    if (!m_discovering) {
        startDiscovery();
        return;
    }

    setLastError(QString());
    sendQuery();
}

void CastDeviceModel::stopDiscovery()
{
    m_queryTimer.stop();
    m_expiryTimer.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
    }
    setDiscovering(false);
}

bool CastDeviceModel::bindSocket()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
    }

    if (!m_socket.bind(QHostAddress::AnyIPv4,
                       MdnsPort,
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        setLastError(tr("Could not listen for Chromecast discovery replies: %1")
                     .arg(m_socket.errorString()));
        qWarning() << "SailVideo Cast: mDNS bind failed:" << m_socket.errorString();
        return false;
    }

    int joined = 0;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (int i = 0; i < interfaces.size(); ++i) {
        const QNetworkInterface iface = interfaces.at(i);
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp)
                || !(flags & QNetworkInterface::IsRunning)
                || (flags & QNetworkInterface::IsLoopBack)
                || !(flags & QNetworkInterface::CanMulticast)) {
            continue;
        }

        if (m_socket.joinMulticastGroup(MdnsGroup, iface)) {
            ++joined;
        }
    }

    if (joined == 0) {
        qWarning() << "SailVideo Cast: no explicit mDNS multicast interface joined";
    }
    return true;
}

void CastDeviceModel::sendQuery()
{
    if (!m_discovering || m_socket.state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    QByteArray packet(12, char(0));
    packet[5] = char(1); // QDCOUNT = 1

    const QByteArray qname = encodedDnsName(QString::fromLatin1(CastService));
    if (qname.isEmpty()) {
        return;
    }

    packet.append(qname);
    packet.append(char(0));
    packet.append(char(12)); // PTR
    packet.append(char(0));
    packet.append(char(1));  // IN

    const qint64 written = m_socket.writeDatagram(packet, MdnsGroup, MdnsPort);
    if (written != packet.size()) {
        qWarning() << "SailVideo Cast: mDNS query write failed:" << m_socket.errorString();
    }
}

bool CastDeviceModel::readName(const QByteArray &packet,
                               int *offset,
                               QString *name,
                               int depth) const
{
    if (!offset || !name || depth > 16 || *offset < 0 || *offset >= packet.size()) {
        return false;
    }

    int pos = *offset;
    QStringList labels;
    bool jumped = false;
    int returnOffset = pos;

    for (int guard = 0; guard < 128 && pos < packet.size(); ++guard) {
        const quint8 length = quint8(packet.at(pos));
        if (length == 0) {
            ++pos;
            if (!jumped) {
                returnOffset = pos;
            }
            *offset = returnOffset;
            *name = labels.join(QLatin1Char('.'));
            return true;
        }

        if ((length & 0xc0) == 0xc0) {
            if (pos + 1 >= packet.size()) {
                return false;
            }
            const int pointer = ((length & 0x3f) << 8)
                    | quint8(packet.at(pos + 1));
            if (pointer < 0 || pointer >= packet.size()) {
                return false;
            }

            if (!jumped) {
                returnOffset = pos + 2;
                jumped = true;
            }

            int pointedOffset = pointer;
            QString pointedName;
            if (!readName(packet, &pointedOffset, &pointedName, depth + 1)) {
                return false;
            }
            if (!pointedName.isEmpty()) {
                labels.append(pointedName);
            }
            *offset = returnOffset;
            *name = labels.join(QLatin1Char('.'));
            return true;
        }

        if (length > 63 || pos + 1 + length > packet.size()) {
            return false;
        }

        labels.append(QString::fromUtf8(packet.constData() + pos + 1, length));
        pos += 1 + length;
        if (!jumped) {
            returnOffset = pos;
        }
    }

    return false;
}

void CastDeviceModel::readPendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        QByteArray packet;
        packet.resize(int(m_socket.pendingDatagramSize()));

        QHostAddress senderAddress;
        quint16 senderPort = 0;
        const qint64 received = m_socket.readDatagram(packet.data(),
                                                      packet.size(),
                                                      &senderAddress,
                                                      &senderPort);
        Q_UNUSED(senderPort)
        if (received <= 0) {
            continue;
        }
        packet.resize(int(received));
        parsePacket(packet, senderAddress);
    }
}

void CastDeviceModel::parsePacket(const QByteArray &packet,
                                  const QHostAddress &senderAddress)
{
    if (packet.size() < 12) {
        return;
    }

    const int questionCount = read16(packet, 4);
    const int answerCount = read16(packet, 6);
    const int authorityCount = read16(packet, 8);
    const int additionalCount = read16(packet, 10);
    const int recordCount = answerCount + authorityCount + additionalCount;

    int offset = 12;
    for (int i = 0; i < questionCount; ++i) {
        QString ignored;
        if (!readName(packet, &offset, &ignored) || offset + 4 > packet.size()) {
            return;
        }
        offset += 4;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (int i = 0; i < recordCount; ++i) {
        QString owner;
        if (!readName(packet, &offset, &owner) || offset + 10 > packet.size()) {
            return;
        }

        const quint16 type = read16(packet, offset);
        const quint16 dataLength = read16(packet, offset + 8);
        offset += 10;

        if (offset + dataLength > packet.size()) {
            return;
        }

        const int dataOffset = offset;
        const QString ownerKey = normalizedName(owner);

        if (type == 12) { // PTR
            int nameOffset = dataOffset;
            QString instance;
            if (readName(packet, &nameOffset, &instance)
                    && ownerKey == QString::fromLatin1(CastService)) {
                const QString key = normalizedName(instance);
                Service service = m_services.value(key);
                service.instance = instance;
                service.lastSeenMs = now;
                if (service.address.isNull()
                        && senderAddress.protocol() == QAbstractSocket::IPv4Protocol) {
                    service.address = senderAddress;
                }
                m_services.insert(key, service);
            }
        } else if (type == 33 && dataLength >= 6) { // SRV
            Service service = m_services.value(ownerKey);
            service.instance = owner;
            service.port = read16(packet, dataOffset + 4);
            int nameOffset = dataOffset + 6;
            QString target;
            if (readName(packet, &nameOffset, &target)) {
                service.target = target;
                const QHostAddress known = m_hostAddresses.value(normalizedName(target));
                if (!known.isNull()) {
                    service.address = known;
                } else if (senderAddress.protocol() == QAbstractSocket::IPv4Protocol) {
                    service.address = senderAddress;
                }
            }
            service.lastSeenMs = now;
            m_services.insert(ownerKey, service);
        } else if (type == 16) { // TXT
            Service service = m_services.value(ownerKey);
            service.instance = owner;
            int pos = dataOffset;
            const int end = dataOffset + dataLength;
            while (pos < end) {
                const int length = quint8(packet.at(pos++));
                if (length <= 0 || pos + length > end) {
                    break;
                }
                const QByteArray pair = packet.mid(pos, length);
                pos += length;
                const int equals = pair.indexOf('=');
                const QByteArray key = (equals >= 0 ? pair.left(equals) : pair).toLower();
                const QString value = equals >= 0
                        ? QString::fromUtf8(pair.mid(equals + 1))
                        : QString();
                if (key == "fn") {
                    service.friendlyName = value;
                } else if (key == "md") {
                    service.model = value;
                } else if (key == "id") {
                    service.uuid = value;
                }
            }
            service.lastSeenMs = now;
            m_services.insert(ownerKey, service);
        } else if (type == 1 && dataLength == 4) { // A
            const quint32 address = (quint32(quint8(packet.at(dataOffset))) << 24)
                    | (quint32(quint8(packet.at(dataOffset + 1))) << 16)
                    | (quint32(quint8(packet.at(dataOffset + 2))) << 8)
                    | quint32(quint8(packet.at(dataOffset + 3)));
            const QHostAddress hostAddress(address);
            m_hostAddresses.insert(ownerKey, hostAddress);

            QHash<QString, Service>::iterator it = m_services.begin();
            for (; it != m_services.end(); ++it) {
                if (normalizedName(it.value().target) == ownerKey) {
                    it.value().address = hostAddress;
                    it.value().lastSeenMs = now;
                }
            }
        }

        offset += dataLength;
    }

    rebuildDevices();
}

QString CastDeviceModel::displayNameForService(const Service &service) const
{
    if (!service.friendlyName.trimmed().isEmpty()) {
        return service.friendlyName.trimmed();
    }

    QString name = service.instance;
    const QString suffix = QStringLiteral("._googlecast._tcp.local");
    if (name.toLower().endsWith(suffix)) {
        name.chop(suffix.size());
    }
    return name.trimmed().isEmpty() ? tr("Chromecast") : name.trimmed();
}

void CastDeviceModel::rebuildDevices()
{
    QVector<Device> devices = m_devices;

    QHash<QString, Service>::const_iterator it = m_services.constBegin();
    for (; it != m_services.constEnd(); ++it) {
        const Service &service = it.value();
        if (service.address.isNull() || service.port == 0) {
            continue;
        }

        Device device;
        device.name = displayNameForService(service);
        device.host = service.address.toString();
        device.port = int(service.port);
        device.model = service.model;
        device.uuid = service.uuid;
        device.key = !device.uuid.isEmpty()
                ? device.uuid
                : device.host + QLatin1Char(':') + QString::number(device.port);

        int existing = -1;
        for (int i = 0; i < devices.size(); ++i) {
            const Device &known = devices.at(i);
            if ((!device.uuid.isEmpty() && known.uuid == device.uuid)
                    || known.key == device.key) {
                existing = i;
                break;
            }
        }

        if (existing >= 0) {
            devices[existing] = device;
        } else {
            devices.append(device);
        }
    }

    std::sort(devices.begin(), devices.end(), [](const Device &a, const Device &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    bool changed = devices.size() != m_devices.size();
    if (!changed) {
        for (int i = 0; i < devices.size(); ++i) {
            const Device &a = devices.at(i);
            const Device &b = m_devices.at(i);
            if (a.name != b.name || a.host != b.host || a.port != b.port
                    || a.model != b.model || a.uuid != b.uuid || a.key != b.key) {
                changed = true;
                break;
            }
        }
    }

    if (!changed) {
        return;
    }

    beginResetModel();
    m_devices = devices;
    endResetModel();
    emit countChanged();
    saveCachedDevices();

    qInfo() << "SailVideo Cast: remembered Chromecast devices:" << m_devices.size();
}

void CastDeviceModel::expireOldDevices()
{
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - DeviceExpiryMs;
    bool changed = false;

    QHash<QString, Service>::iterator it = m_services.begin();
    while (it != m_services.end()) {
        if (it.value().lastSeenMs > 0 && it.value().lastSeenMs < cutoff) {
            it = m_services.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        qInfo() << "SailVideo Cast: expired stale live discovery records;"
                << "remembered devices are retained";
    }
}

int CastDeviceModel::deviceIndexForKey(const QString &key) const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices.at(i).key == key) {
            return i;
        }
    }
    return -1;
}

void CastDeviceModel::loadCachedDevices()
{
    QFile file(storagePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "SailVideo Cast: could not parse remembered device list"
                   << storagePath() << parseError.errorString();
        return;
    }

    const QJsonArray array = document.object().value(QStringLiteral("devices")).toArray();
    QVector<Device> devices;
    QHash<QString, bool> seen;
    for (int i = 0; i < array.size(); ++i) {
        const QJsonObject object = array.at(i).toObject();
        Device device;
        device.name = object.value(QStringLiteral("name")).toString().trimmed();
        device.host = object.value(QStringLiteral("host")).toString().trimmed();
        device.port = object.value(QStringLiteral("port")).toInt(8009);
        device.model = object.value(QStringLiteral("model")).toString().trimmed();
        device.uuid = object.value(QStringLiteral("uuid")).toString().trimmed();
        device.key = !device.uuid.isEmpty()
                ? device.uuid
                : device.host + QLatin1Char(':') + QString::number(device.port);

        if (device.host.isEmpty() || device.port <= 0 || seen.contains(device.key)) {
            continue;
        }
        if (device.name.isEmpty()) {
            device.name = tr("Chromecast");
        }
        seen.insert(device.key, true);
        devices.append(device);
    }

    std::sort(devices.begin(), devices.end(), [](const Device &a, const Device &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    beginResetModel();
    m_devices = devices;
    endResetModel();

    qInfo() << "SailVideo Cast: loaded" << m_devices.size()
            << "remembered Chromecast device(s) from" << storagePath();
}

bool CastDeviceModel::saveCachedDevices() const
{
    QDir directory(m_storageDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        qWarning() << "SailVideo Cast: could not create device-cache directory"
                   << m_storageDirectory;
        return false;
    }

    QJsonArray array;
    for (int i = 0; i < m_devices.size(); ++i) {
        const Device &device = m_devices.at(i);
        QJsonObject object;
        object.insert(QStringLiteral("name"), device.name);
        object.insert(QStringLiteral("host"), device.host);
        object.insert(QStringLiteral("port"), device.port);
        object.insert(QStringLiteral("model"), device.model);
        object.insert(QStringLiteral("uuid"), device.uuid);
        array.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("devices"), array);

    QSaveFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "SailVideo Cast: could not write remembered device list"
                   << storagePath() << file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning() << "SailVideo Cast: could not commit remembered device list"
                   << storagePath() << file.errorString();
        return false;
    }
    return true;
}

void CastDeviceModel::setDiscovering(bool discovering)
{
    if (m_discovering == discovering) {
        return;
    }
    m_discovering = discovering;
    emit discoveringChanged();
}

void CastDeviceModel::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}
