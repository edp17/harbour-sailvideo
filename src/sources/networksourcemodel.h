/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef NETWORKSOURCEMODEL_H
#define NETWORKSOURCEMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class NetworkSourceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        UrlRole,
        SchemeRole,
        SubtitleRole
    };
    Q_ENUM(Roles)

    explicit NetworkSourceModel(const QString &storageDirectory = QString(), QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QString lastError() const;
    QString storagePath() const;

    Q_INVOKABLE bool addOrUpdate(const QString &name, const QString &url);
    Q_INVOKABLE bool setAt(int row, const QString &name, const QString &url);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void clear();

    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString urlAt(int row) const;
    Q_INVOKABLE QString normalizedUrl(const QString &url) const;
    Q_INVOKABLE QString suggestedNameForUrl(const QString &url) const;
    Q_INVOKABLE bool isSupportedPlaybackUrl(const QString &url) const;

signals:
    void countChanged();
    void lastErrorChanged();

private:
    struct Entry {
        QString name;
        QString url;
        QString scheme;
        QString subtitle;
    };

    void load();
    void loadLegacySettings(QVector<Entry> *entries) const;
    void save() const;
    int indexOfUrl(const QString &url) const;
    void setLastError(const QString &message) const;
    Entry entryFromUserInput(const QString &name,
                             const QString &url,
                             bool *ok) const;

    static QString cleanedText(const QString &value);
    static QString normalizedPlaybackUrl(const QString &value);
    static QString displayNameForUrl(const QString &url,
                                     const QString &fallbackName = QString());
    static QString subtitleForUrl(const QString &url);
    static bool supportedScheme(const QString &scheme);

    QString m_storageDirectory;
    QVector<Entry> m_entries;
    mutable QString m_lastError;
};

#endif // NETWORKSOURCEMODEL_H
