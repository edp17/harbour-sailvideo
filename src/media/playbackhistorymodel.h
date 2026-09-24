/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef PLAYBACKHISTORYMODEL_H
#define PLAYBACKHISTORYMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVariant>
#include <QVector>

class PlaybackHistoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString latestUrl READ latestUrl NOTIFY latestChanged)
    Q_PROPERTY(QString latestTitle READ latestTitle NOTIFY latestChanged)
    Q_PROPERTY(qint64 latestPosition READ latestPosition NOTIFY latestChanged)
    Q_PROPERTY(qint64 latestSourceSize READ latestSourceSize NOTIFY latestChanged)
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
    Q_PROPERTY(QString storageDirectory READ storageDirectory CONSTANT)

public:
    enum Roles {
        UrlRole = Qt::UserRole + 1,
        TitleRole,
        DurationRole,
        PositionRole,
        SourceSizeRole,
        LastPlayedRole,
        LastPlayedTextRole
    };
    Q_ENUM(Roles)

    explicit PlaybackHistoryModel(const QString &storageDirectory = QString(),
                                  QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QString latestUrl() const;
    QString latestTitle() const;
    qint64 latestPosition() const;
    qint64 latestSourceSize() const;
    QString storagePath() const;
    QString storageDirectory() const;

    Q_INVOKABLE void addOrUpdate(const QString &url,
                                 const QString &title,
                                 qint64 duration,
                                 qint64 position);
    Q_INVOKABLE void updatePosition(const QString &url,
                                    qint64 duration,
                                    qint64 position);
    Q_INVOKABLE qint64 resumePosition(const QString &url) const;
    Q_INVOKABLE qint64 sourceSizeForUrl(const QString &url) const;
    Q_INVOKABLE void updateSourceSize(const QString &url, qint64 sourceSize);
    Q_INVOKABLE void updatePlaybackQueue(const QString &url,
                                         const QVariantList &items);
    Q_INVOKABLE QVariantList playbackQueueForUrl(const QString &url) const;
    Q_INVOKABLE QString latestUrlValue() const;
    Q_INVOKABLE QString latestTitleValue() const;
    Q_INVOKABLE qint64 latestPositionValue() const;
    Q_INVOKABLE qint64 latestSourceSizeValue() const;
    Q_INVOKABLE QString titleForUrl(const QString &url,
                                    const QString &title = QString()) const;
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void latestChanged();

private:
    struct QueueItem {
        QString url;
        QString title;
        qint64 sourceSize = 0;
    };

    struct Entry {
        QString url;
        QString title;
        qint64 duration = 0;
        qint64 position = 0;
        qint64 sourceSize = 0;
        QVector<QueueItem> playbackQueue;
        QDateTime lastPlayed;
    };

    void load();
    bool save() const;
    int indexOfUrl(const QString &url) const;
    void replaceEntries(const QVector<Entry> &entries);

    static QString cleanedText(const QString &value);
    static QString fallbackTitle(const QString &url, const QString &title);
    static qint64 normalizedPosition(qint64 duration, qint64 position);

    QString m_storageDirectory;
    QVector<Entry> m_entries;
};

#endif // PLAYBACKHISTORYMODEL_H
