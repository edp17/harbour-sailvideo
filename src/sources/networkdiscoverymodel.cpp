/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "networkdiscoverymodel.h"

#include <QFutureWatcher>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QtConcurrent>

#include <algorithm>

namespace {

const int SmbPort = 445;
const int ConnectTimeoutMs = 140;
const int MaximumHostsToProbe = 254;

} // namespace

NetworkDiscoveryModel::NetworkDiscoveryModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_status(tr("Not scanned yet"))
{
}

NetworkDiscoveryModel::~NetworkDiscoveryModel()
{
    if (m_watcher) {
        m_watcher->cancel();
        m_watcher->waitForFinished();
        delete m_watcher;
        m_watcher = nullptr;
    }
}

int NetworkDiscoveryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant NetworkDiscoveryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return entry.name;
    case HostRole:
        return entry.host;
    case AddressRole:
        return entry.address;
    case PortRole:
        return entry.port;
    case SubtitleRole:
        return entry.subtitle;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> NetworkDiscoveryModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "name");
    roles.insert(HostRole, "host");
    roles.insert(AddressRole, "address");
    roles.insert(PortRole, "port");
    roles.insert(SubtitleRole, "subtitle");
    return roles;
}

bool NetworkDiscoveryModel::scanning() const
{
    return m_scanning;
}

int NetworkDiscoveryModel::count() const
{
    return m_entries.size();
}

QString NetworkDiscoveryModel::status() const
{
    return m_status;
}

void NetworkDiscoveryModel::scan()
{
    if (m_scanning) {
        return;
    }

    setStatus(tr("Searching this Wi-Fi/LAN for SMB servers..."));
    replaceEntries(QVector<Entry>());
    setScanning(true);

    QFutureWatcher<ScanResult> *watcher = new QFutureWatcher<ScanResult>(this);
    m_watcher = watcher;
    connect(watcher, SIGNAL(finished()), this, SLOT(handleScanFinished()));
    watcher->setFuture(QtConcurrent::run(&NetworkDiscoveryModel::scanSync));
}

void NetworkDiscoveryModel::clear()
{
    replaceEntries(QVector<Entry>());
    setStatus(tr("Not scanned yet"));
}

void NetworkDiscoveryModel::handleScanFinished()
{
    QFutureWatcher<ScanResult> *watcher = static_cast<QFutureWatcher<ScanResult> *>(sender());
    if (!watcher) {
        return;
    }

    const ScanResult result = watcher->result();
    watcher->deleteLater();
    if (m_watcher == watcher) {
        m_watcher = nullptr;
    }

    replaceEntries(result.entries);
    setStatus(result.status);
    setScanning(false);
}

NetworkDiscoveryModel::ScanResult NetworkDiscoveryModel::scanSync()
{
    ScanResult result;
    const QVector<QString> candidates = localCandidateAddresses();

    QVector<Entry> found;
    for (int i = 0; i < candidates.size(); ++i) {
        const QString address = candidates.at(i);
        if (!hasOpenSmbPort(address, ConnectTimeoutMs)) {
            continue;
        }

        Entry entry;
        entry.address = address;
        entry.host = address;
        entry.port = SmbPort;
        entry.name = hostLabel(address);
        entry.subtitle = QObject::tr("SMB port 445 is reachable. Tap to add the share name.");
        found.append(entry);
    }

    result.entries = found;
    if (candidates.isEmpty()) {
        result.status = QObject::tr("No active IPv4 LAN interface was found. Connect to Wi-Fi and search again, or add the server manually.");
    } else if (found.isEmpty()) {
        result.status = QObject::tr("No SMB servers were detected. You can still add a NAS manually by host name or IP address.");
    } else {
        result.status = QObject::tr("Found %1 possible SMB server(s).").arg(found.size());
    }
    return result;
}

QVector<QString> NetworkDiscoveryModel::localCandidateAddresses()
{
    QVector<QString> candidates;

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (int i = 0; i < interfaces.size(); ++i) {
        const QNetworkInterface iface = interfaces.at(i);
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
                || !flags.testFlag(QNetworkInterface::IsRunning)
                || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const QList<QNetworkAddressEntry> addresses = iface.addressEntries();
        for (int j = 0; j < addresses.size(); ++j) {
            const QHostAddress hostAddress = addresses.at(j).ip();
            if (hostAddress.protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }

            const quint32 own = hostAddress.toIPv4Address();
            if (own == 0) {
                continue;
            }

            // Deliberately probe only the local /24. That keeps the first
            // version predictable and avoids very long scans on large LANs.
            const quint32 base = own & 0xffffff00u;
            int probed = 0;
            for (quint32 last = 1; last <= 254 && probed < MaximumHostsToProbe; ++last) {
                const quint32 candidate = base | last;
                if (candidate == own) {
                    continue;
                }
                const QString address = QHostAddress(candidate).toString();
                if (!candidates.contains(address)) {
                    candidates.append(address);
                    ++probed;
                }
            }
        }
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates;
}

bool NetworkDiscoveryModel::hasOpenSmbPort(const QString &address, int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(address, SmbPort);
    const bool connected = socket.waitForConnected(timeoutMs);
    if (connected) {
        socket.disconnectFromHost();
        socket.waitForDisconnected(50);
    }
    return connected;
}

QString NetworkDiscoveryModel::hostLabel(const QString &address)
{
    return QObject::tr("SMB server %1").arg(address);
}

void NetworkDiscoveryModel::replaceEntries(const QVector<Entry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
    emit countChanged();
}

void NetworkDiscoveryModel::setScanning(bool scanning)
{
    if (m_scanning == scanning) {
        return;
    }
    m_scanning = scanning;
    emit scanningChanged();
}

void NetworkDiscoveryModel::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}
