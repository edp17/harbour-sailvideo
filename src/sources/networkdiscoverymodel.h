/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef NETWORKDISCOVERYMODEL_H
#define NETWORKDISCOVERYMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class QFutureWatcherBase;

class NetworkDiscoveryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        HostRole,
        AddressRole,
        PortRole,
        SubtitleRole
    };
    Q_ENUM(Roles)

    explicit NetworkDiscoveryModel(QObject *parent = nullptr);
    ~NetworkDiscoveryModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool scanning() const;
    int count() const;
    QString status() const;

    Q_INVOKABLE void scan();
    Q_INVOKABLE void clear();

signals:
    void scanningChanged();
    void countChanged();
    void statusChanged();

private slots:
    void handleScanFinished();

private:
    struct Entry {
        QString name;
        QString host;
        QString address;
        int port = 445;
        QString subtitle;
    };

    struct ScanResult {
        QVector<Entry> entries;
        QString status;
    };

    static ScanResult scanSync();
    static QVector<QString> localCandidateAddresses();
    static bool hasOpenSmbPort(const QString &address, int timeoutMs);
    static QString hostLabel(const QString &address);

    void replaceEntries(const QVector<Entry> &entries);
    void setScanning(bool scanning);
    void setStatus(const QString &status);

    QVector<Entry> m_entries;
    bool m_scanning = false;
    QString m_status;
    QFutureWatcherBase *m_watcher = nullptr;
};

#endif // NETWORKDISCOVERYMODEL_H
