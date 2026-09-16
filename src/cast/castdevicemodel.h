/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef CASTDEVICEMODEL_H
#define CASTDEVICEMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QHostAddress>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

class CastDeviceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool discovering READ discovering NOTIFY discoveringChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        HostRole,
        PortRole,
        ModelRole,
        UuidRole
    };
    Q_ENUM(Roles)

    explicit CastDeviceModel(const QString &storageDirectory = QString(), QObject *parent = nullptr);
    ~CastDeviceModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    bool discovering() const;
    QString lastError() const;
    QString storagePath() const;

    Q_INVOKABLE void startDiscovery();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void stopDiscovery();

signals:
    void countChanged();
    void discoveringChanged();
    void lastErrorChanged();

private slots:
    void readPendingDatagrams();
    void sendQuery();
    void expireOldDevices();

private:
    struct Service {
        QString instance;
        QString target;
        QString friendlyName;
        QString model;
        QString uuid;
        QHostAddress address;
        quint16 port = 8009;
        qint64 lastSeenMs = 0;
    };

    struct Device {
        QString name;
        QString host;
        int port = 8009;
        QString model;
        QString uuid;
        QString key;
    };

    bool bindSocket();
    void parsePacket(const QByteArray &packet, const QHostAddress &senderAddress);
    bool readName(const QByteArray &packet, int *offset, QString *name, int depth = 0) const;
    void rebuildDevices();
    QString displayNameForService(const Service &service) const;
    void setDiscovering(bool discovering);
    void setLastError(const QString &message);
    void loadCachedDevices();
    bool saveCachedDevices() const;
    int deviceIndexForKey(const QString &key) const;

    QUdpSocket m_socket;
    QTimer m_queryTimer;
    QTimer m_expiryTimer;
    QHash<QString, Service> m_services;
    QHash<QString, QHostAddress> m_hostAddresses;
    QVector<Device> m_devices;
    bool m_discovering = false;
    QString m_lastError;
    QString m_storageDirectory;
};

#endif // CASTDEVICEMODEL_H
