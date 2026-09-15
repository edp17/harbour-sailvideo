/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef NASSOURCEMODEL_H
#define NASSOURCEMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class NasSourceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
    Q_PROPERTY(QString storageDirectory READ storageDirectory CONSTANT)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        HostRole,
        PortRole,
        ShareRole,
        PathRole,
        DomainRole,
        UsernameRole,
        GuestRole,
        SubtitleRole
    };
    Q_ENUM(Roles)

    explicit NasSourceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QString lastError() const;
    QString storagePath() const;
    QString storageDirectory() const;

    Q_INVOKABLE bool addOrUpdate(const QString &name,
                                 const QString &host,
                                 int port,
                                 const QString &share,
                                 const QString &path,
                                 const QString &domain,
                                 const QString &username,
                                 bool guest);
    Q_INVOKABLE bool setAt(int row,
                           const QString &name,
                           const QString &host,
                           int port,
                           const QString &share,
                           const QString &path,
                           const QString &domain,
                           const QString &username,
                           bool guest);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void clear();

    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString hostAt(int row) const;
    Q_INVOKABLE int portAt(int row) const;
    Q_INVOKABLE QString shareAt(int row) const;
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE QString domainAt(int row) const;
    Q_INVOKABLE QString usernameAt(int row) const;
    Q_INVOKABLE bool guestAt(int row) const;
    Q_INVOKABLE int indexForLocation(const QString &host, int port, const QString &share) const;
    Q_INVOKABLE int indexForLocationAndPath(const QString &host,
                                            int port,
                                            const QString &share,
                                            const QString &path) const;

signals:
    void countChanged();
    void lastErrorChanged();

private:
    struct Entry {
        QString name;
        QString host;
        int port = 445;
        QString share;
        QString path;
        QString domain;
        QString username;
        bool guest = true;
        QString subtitle;
    };

    void load();
    bool save() const;
    int indexOfServerShare(const QString &host, int port, const QString &share) const;
    int indexOfServerSharePath(const QString &host,
                               int port,
                               const QString &share,
                               const QString &path) const;
    void setLastError(const QString &message) const;
    Entry entryFromUserInput(const QString &name,
                             const QString &host,
                             int port,
                             const QString &share,
                             const QString &path,
                             const QString &domain,
                             const QString &username,
                             bool guest,
                             bool *ok) const;

    static QString cleanedText(const QString &value);
    static QString normalizedHost(const QString &host);
    static QString normalizedShare(const QString &share);
    static QString normalizedPath(const QString &path);
    static QString pathPartFromShareInput(const QString &share);
    static QString displayName(const Entry &entry, const QString &fallbackName = QString());
    static QString subtitle(const Entry &entry);

    QVector<Entry> m_entries;
    mutable QString m_lastError;
};

#endif // NASSOURCEMODEL_H
