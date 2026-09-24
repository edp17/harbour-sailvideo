/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "playbackhistorymodel.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QCoreApplication>

namespace {

const int MaxEntries = 25;
const int MaxQueueItems = 500;
const qint64 MinimumResumePositionMs = 5000;
const qint64 CompletedThresholdMs = 30000;

QString sailjailOrganizationName()
{
    const QString name = QCoreApplication::organizationName().trimmed();
    return name.isEmpty() ? QStringLiteral("org.edp17") : name;
}

QString sailjailApplicationName()
{
    const QString name = QCoreApplication::applicationName().trimmed();
    return name.isEmpty() ? QStringLiteral("SailVideo") : name;
}

QString defaultPlaybackHistoryStorageDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        base = QDir::home().filePath(QStringLiteral(".local/share"));
    }

    return QDir(base).filePath(sailjailOrganizationName()
                               + QLatin1Char('/')
                               + sailjailApplicationName());
}

QString playbackHistoryStoragePathForDirectory(const QString &directory)
{
    return QDir(directory).filePath(QStringLiteral("playback-history.json"));
}

QStringList legacyPlaybackHistoryStoragePaths(const QString &currentStoragePath)
{
    QStringList paths;

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        paths << QDir(appData).filePath(QStringLiteral("playback-history.json"));
    }

    paths << QDir(QDir::home().filePath(QStringLiteral(".local/share/org.edp17/SailVideo")))
             .filePath(QStringLiteral("playback-history.json"));
    paths << QDir(QDir::home().filePath(QStringLiteral(".local/share/SailVideo")))
             .filePath(QStringLiteral("playback-history.json"));

    paths.removeAll(currentStoragePath);
    paths.removeDuplicates();
    return paths;
}

QDateTime dateTimeFromJson(const QJsonValue &value)
{
    const QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
    return parsed.isValid() ? parsed : QDateTime::currentDateTime();
}

} // namespace

PlaybackHistoryModel::PlaybackHistoryModel(const QString &storageDirectory,
                                           QObject *parent)
    : QAbstractListModel(parent)
    , m_storageDirectory(storageDirectory.trimmed().isEmpty()
                         ? defaultPlaybackHistoryStorageDirectory()
                         : storageDirectory.trimmed())
{
    load();
}

int PlaybackHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant PlaybackHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case UrlRole:
        return entry.url;
    case TitleRole:
    case Qt::DisplayRole:
        return entry.title;
    case DurationRole:
        return entry.duration;
    case PositionRole:
        return entry.position;
    case SourceSizeRole:
        return entry.sourceSize;
    case LastPlayedRole:
        return entry.lastPlayed;
    case LastPlayedTextRole:
        return entry.lastPlayed.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PlaybackHistoryModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(UrlRole, "url");
    roles.insert(TitleRole, "title");
    roles.insert(DurationRole, "duration");
    roles.insert(PositionRole, "position");
    roles.insert(SourceSizeRole, "sourceSize");
    roles.insert(LastPlayedRole, "lastPlayed");
    roles.insert(LastPlayedTextRole, "lastPlayedText");
    return roles;
}

int PlaybackHistoryModel::count() const
{
    return m_entries.size();
}

QString PlaybackHistoryModel::latestUrl() const
{
    return m_entries.isEmpty() ? QString() : m_entries.first().url;
}

QString PlaybackHistoryModel::latestTitle() const
{
    return m_entries.isEmpty() ? QString() : m_entries.first().title;
}

qint64 PlaybackHistoryModel::latestPosition() const
{
    return m_entries.isEmpty() ? 0 : m_entries.first().position;
}

qint64 PlaybackHistoryModel::latestSourceSize() const
{
    return m_entries.isEmpty() ? 0 : m_entries.first().sourceSize;
}

QString PlaybackHistoryModel::storagePath() const
{
    return playbackHistoryStoragePathForDirectory(m_storageDirectory);
}

QString PlaybackHistoryModel::storageDirectory() const
{
    return m_storageDirectory;
}

QString PlaybackHistoryModel::latestUrlValue() const
{
    return latestUrl();
}

QString PlaybackHistoryModel::latestTitleValue() const
{
    return latestTitle();
}

qint64 PlaybackHistoryModel::latestPositionValue() const
{
    return latestPosition();
}

qint64 PlaybackHistoryModel::latestSourceSizeValue() const
{
    return latestSourceSize();
}

void PlaybackHistoryModel::addOrUpdate(const QString &url,
                                       const QString &title,
                                       qint64 duration,
                                       qint64 position)
{
    const QString cleanUrl = cleanedText(url);
    if (cleanUrl.isEmpty()) {
        return;
    }

    Entry entry;
    const int existingIndex = indexOfUrl(cleanUrl);
    if (existingIndex >= 0) {
        entry = m_entries.at(existingIndex);
        beginRemoveRows(QModelIndex(), existingIndex, existingIndex);
        m_entries.remove(existingIndex);
        endRemoveRows();
        emit countChanged();
    }

    entry.url = cleanUrl;
    entry.title = fallbackTitle(cleanUrl, title);
    entry.duration = qMax<qint64>(0, duration);
    entry.position = normalizedPosition(entry.duration, position);
    entry.lastPlayed = QDateTime::currentDateTime();

    beginInsertRows(QModelIndex(), 0, 0);
    m_entries.prepend(entry);
    endInsertRows();
    emit countChanged();

    while (m_entries.size() > MaxEntries) {
        const int last = m_entries.size() - 1;
        beginRemoveRows(QModelIndex(), last, last);
        m_entries.removeLast();
        endRemoveRows();
        emit countChanged();
    }

    save();
    emit latestChanged();
}

void PlaybackHistoryModel::updatePosition(const QString &url,
                                          qint64 duration,
                                          qint64 position)
{
    const int existingIndex = indexOfUrl(cleanedText(url));
    if (existingIndex < 0) {
        return;
    }

    Entry &entry = m_entries[existingIndex];
    entry.duration = qMax<qint64>(0, duration);
    entry.position = normalizedPosition(entry.duration, position);
    entry.lastPlayed = QDateTime::currentDateTime();

    const QModelIndex changed = index(existingIndex, 0);
    emit dataChanged(changed, changed,
                     QVector<int>() << DurationRole << PositionRole
                                     << LastPlayedRole << LastPlayedTextRole);
    save();
    if (existingIndex == 0) {
        emit latestChanged();
    }
}

qint64 PlaybackHistoryModel::resumePosition(const QString &url) const
{
    const int existingIndex = indexOfUrl(cleanedText(url));
    if (existingIndex < 0) {
        return 0;
    }

    const Entry entry = m_entries.at(existingIndex);
    if (entry.position < MinimumResumePositionMs) {
        return 0;
    }

    return entry.position;
}

qint64 PlaybackHistoryModel::sourceSizeForUrl(const QString &url) const
{
    const int existingIndex = indexOfUrl(cleanedText(url));
    if (existingIndex < 0) {
        return 0;
    }

    return qMax<qint64>(0, m_entries.at(existingIndex).sourceSize);
}

void PlaybackHistoryModel::updateSourceSize(const QString &url, qint64 sourceSize)
{
    const int existingIndex = indexOfUrl(cleanedText(url));
    if (existingIndex < 0 || sourceSize <= 0) {
        return;
    }

    Entry &entry = m_entries[existingIndex];
    if (entry.sourceSize == sourceSize) {
        return;
    }

    entry.sourceSize = sourceSize;

    const QModelIndex changed = index(existingIndex, 0);
    emit dataChanged(changed, changed, QVector<int>() << SourceSizeRole);
    save();

    if (existingIndex == 0) {
        emit latestChanged();
    }
}

void PlaybackHistoryModel::updatePlaybackQueue(const QString &url,
                                               const QVariantList &items)
{
    const int existingIndex = indexOfUrl(cleanedText(url));
    if (existingIndex < 0) {
        return;
    }

    QVector<QueueItem> queue;
    const int itemCount = qMin(items.size(), MaxQueueItems);
    for (int i = 0; i < itemCount; ++i) {
        const QVariantMap map = items.at(i).toMap();
        const QString queueUrl =
                cleanedText(map.value(QStringLiteral("url")).toString());
        if (queueUrl.isEmpty()) {
            continue;
        }

        bool duplicate = false;
        for (int j = 0; j < queue.size(); ++j) {
            if (queue.at(j).url == queueUrl) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        QueueItem item;
        item.url = queueUrl;
        item.title = fallbackTitle(
                    queueUrl,
                    map.value(QStringLiteral("title")).toString());
        item.sourceSize = qMax<qint64>(
                    0,
                    map.value(QStringLiteral("sourceSize")).toLongLong());
        queue.append(item);
    }

    Entry &entry = m_entries[existingIndex];
    bool unchanged = entry.playbackQueue.size() == queue.size();
    if (unchanged) {
        for (int i = 0; i < queue.size(); ++i) {
            const QueueItem &oldItem = entry.playbackQueue.at(i);
            const QueueItem &newItem = queue.at(i);
            if (oldItem.url != newItem.url
                    || oldItem.title != newItem.title
                    || oldItem.sourceSize != newItem.sourceSize) {
                unchanged = false;
                break;
            }
        }
    }

    if (unchanged) {
        return;
    }

    entry.playbackQueue = queue;
    save();
}

QVariantList PlaybackHistoryModel::playbackQueueForUrl(const QString &url) const
{
    QVariantList result;
    const int existingIndex = indexOfUrl(cleanedText(url));
    if (existingIndex < 0) {
        return result;
    }

    const QVector<QueueItem> &queue =
            m_entries.at(existingIndex).playbackQueue;
    for (int i = 0; i < queue.size(); ++i) {
        const QueueItem &item = queue.at(i);
        QVariantMap map;
        map.insert(QStringLiteral("url"), item.url);
        map.insert(QStringLiteral("title"), item.title);
        map.insert(QStringLiteral("sourceSize"), item.sourceSize);
        result.append(map);
    }
    return result;
}

QString PlaybackHistoryModel::titleForUrl(const QString &url, const QString &title) const
{
    return fallbackTitle(cleanedText(url), title);
}

void PlaybackHistoryModel::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }

    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
    save();
    emit latestChanged();
}

void PlaybackHistoryModel::load()
{
    QVector<Entry> loadedEntries;
    const QString storageFilePath = this->storagePath();

    QString jsonPathToLoad = storageFilePath;
    if (!QFile::exists(jsonPathToLoad)) {
        const QStringList legacyPaths = legacyPlaybackHistoryStoragePaths(storageFilePath);
        for (int i = 0; i < legacyPaths.size(); ++i) {
            if (QFile::exists(legacyPaths.at(i))) {
                jsonPathToLoad = legacyPaths.at(i);
                break;
            }
        }
    }

    if (QFile::exists(jsonPathToLoad)) {
        QFile file(jsonPathToLoad);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "SailVideo: failed to open playback history store"
                       << jsonPathToLoad << file.errorString();
            replaceEntries(loadedEntries);
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning() << "SailVideo: failed to parse playback history store"
                       << jsonPathToLoad << parseError.errorString();
            replaceEntries(loadedEntries);
            return;
        }

        const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();
        for (int i = 0; i < entries.size() && loadedEntries.size() < MaxEntries; ++i) {
            const QJsonObject object = entries.at(i).toObject();

            Entry entry;
            entry.url = cleanedText(object.value(QStringLiteral("url")).toString());
            entry.title = fallbackTitle(entry.url,
                                        object.value(QStringLiteral("title")).toString());
            entry.duration = qMax<qint64>(0,
                                          static_cast<qint64>(object.value(QStringLiteral("duration")).toDouble(0)));
            entry.position = normalizedPosition(
                        entry.duration,
                        static_cast<qint64>(object.value(QStringLiteral("position")).toDouble(0)));
            entry.sourceSize = qMax<qint64>(0,
                                           static_cast<qint64>(object.value(QStringLiteral("sourceSize")).toDouble(0)));
            entry.lastPlayed = dateTimeFromJson(object.value(QStringLiteral("lastPlayed")));

            const QJsonArray playbackQueue =
                    object.value(QStringLiteral("playbackQueue")).toArray();
            for (int queueIndex = 0;
                 queueIndex < playbackQueue.size()
                 && entry.playbackQueue.size() < MaxQueueItems;
                 ++queueIndex) {
                const QJsonObject queueObject =
                        playbackQueue.at(queueIndex).toObject();

                QueueItem queueItem;
                queueItem.url = cleanedText(
                            queueObject.value(QStringLiteral("url")).toString());
                if (queueItem.url.isEmpty()) {
                    continue;
                }

                queueItem.title = fallbackTitle(
                            queueItem.url,
                            queueObject.value(QStringLiteral("title")).toString());
                queueItem.sourceSize = qMax<qint64>(
                            0,
                            static_cast<qint64>(
                                queueObject.value(QStringLiteral("sourceSize"))
                                .toDouble(0)));

                bool duplicateQueueItem = false;
                for (int j = 0; j < entry.playbackQueue.size(); ++j) {
                    if (entry.playbackQueue.at(j).url == queueItem.url) {
                        duplicateQueueItem = true;
                        break;
                    }
                }

                if (!duplicateQueueItem) {
                    entry.playbackQueue.append(queueItem);
                }
            }

            if (!entry.url.isEmpty()) {
                bool duplicate = false;
                for (int j = 0; j < loadedEntries.size(); ++j) {
                    if (loadedEntries.at(j).url == entry.url) {
                        duplicate = true;
                        break;
                    }
                }

                if (!duplicate) {
                    loadedEntries.append(entry);
                }
            }
        }

        replaceEntries(loadedEntries);
        qWarning() << "SailVideo: playback history loaded from" << jsonPathToLoad
                   << "entries:" << loadedEntries.size()
                   << "latest:" << latestUrl();
        if (jsonPathToLoad != storageFilePath && !loadedEntries.isEmpty()) {
            save();
        }
        return;
    }

    // One-time migration from the earlier QSettings-based history store.
    // QSettings proved unreliable under Sailjail for this app, so Phase 6 r1
    // moves playback history into the explicit whitelisted app-data path.
    QSettings settings;
    settings.beginGroup(QStringLiteral("history"));
    const int count = settings.value(QStringLiteral("count"), 0).toInt();

    for (int i = 0; i < count && loadedEntries.size() < MaxEntries; ++i) {
        settings.beginGroup(QString::number(i));

        Entry entry;
        entry.url = cleanedText(settings.value(QStringLiteral("url")).toString());
        entry.title = fallbackTitle(entry.url,
                                    settings.value(QStringLiteral("title")).toString());
        entry.duration = qMax<qint64>(0,
                                      settings.value(QStringLiteral("duration"), 0).toLongLong());
        entry.position = normalizedPosition(
                    entry.duration,
                    settings.value(QStringLiteral("position"), 0).toLongLong());
        entry.sourceSize = qMax<qint64>(0,
                                       settings.value(QStringLiteral("sourceSize"), 0).toLongLong());
        entry.lastPlayed = settings.value(QStringLiteral("lastPlayed")).toDateTime();
        if (!entry.lastPlayed.isValid()) {
            entry.lastPlayed = QDateTime::currentDateTime();
        }

        settings.endGroup();

        if (!entry.url.isEmpty()) {
            bool duplicate = false;
            for (int j = 0; j < loadedEntries.size(); ++j) {
                if (loadedEntries.at(j).url == entry.url) {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate) {
                loadedEntries.append(entry);
            }
        }
    }

    settings.endGroup();
    replaceEntries(loadedEntries);

    qWarning() << "SailVideo: playback history loaded from QSettings migration"
               << "entries:" << loadedEntries.size()
               << "latest:" << latestUrl()
               << "target:" << storageFilePath;

    if (!loadedEntries.isEmpty()) {
        save();
    }
}

bool PlaybackHistoryModel::save() const
{
    const QString directoryPath = storageDirectory();
    QDir directory(directoryPath);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        qWarning() << "SailVideo: failed to create playback history directory"
                   << directoryPath;
        return false;
    }

    QJsonArray entries;
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &entry = m_entries.at(i);

        QJsonObject object;
        object.insert(QStringLiteral("url"), entry.url);
        object.insert(QStringLiteral("title"), entry.title);
        object.insert(QStringLiteral("duration"), static_cast<double>(entry.duration));
        object.insert(QStringLiteral("position"), static_cast<double>(entry.position));
        object.insert(QStringLiteral("sourceSize"), static_cast<double>(entry.sourceSize));
        object.insert(QStringLiteral("lastPlayed"), entry.lastPlayed.toString(Qt::ISODate));

        if (!entry.playbackQueue.isEmpty()) {
            QJsonArray playbackQueue;
            for (int queueIndex = 0;
                 queueIndex < entry.playbackQueue.size();
                 ++queueIndex) {
                const QueueItem &queueItem = entry.playbackQueue.at(queueIndex);
                QJsonObject queueObject;
                queueObject.insert(QStringLiteral("url"), queueItem.url);
                queueObject.insert(QStringLiteral("title"), queueItem.title);
                queueObject.insert(QStringLiteral("sourceSize"),
                                   static_cast<double>(queueItem.sourceSize));
                playbackQueue.append(queueObject);
            }
            object.insert(QStringLiteral("playbackQueue"), playbackQueue);
        }

        entries.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("entries"), entries);

    const QString storageFilePath = this->storagePath();
    QSaveFile file(storageFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "SailVideo: failed to open playback history store for writing"
                   << storageFilePath << file.errorString();
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning() << "SailVideo: failed to commit playback history store"
                   << storageFilePath << file.errorString();
        return false;
    }

    qWarning() << "SailVideo: playback history saved to" << storageFilePath
               << "entries:" << m_entries.size()
               << "latest:" << latestUrl();
    return true;
}

int PlaybackHistoryModel::indexOfUrl(const QString &url) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).url == url) {
            return i;
        }
    }
    return -1;
}

void PlaybackHistoryModel::replaceEntries(const QVector<Entry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
    emit countChanged();
    emit latestChanged();
}

QString PlaybackHistoryModel::cleanedText(const QString &value)
{
    QString text = value.trimmed();

    for (int i = 0; i < 3; ++i) {
        if (text.size() >= 2
                && ((text.startsWith(QLatin1Char('\'')) && text.endsWith(QLatin1Char('\'')))
                    || (text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"'))))) {
            text = text.mid(1, text.size() - 2).trimmed();
        } else {
            break;
        }
    }

    return text;
}

QString PlaybackHistoryModel::fallbackTitle(const QString &url, const QString &title)
{
    const QString cleanTitle = cleanedText(title);
    if (!cleanTitle.isEmpty()) {
        return cleanTitle;
    }

    const QString cleanUrl = cleanedText(url);
    const QUrl parsedUrl(cleanUrl);
    QString fileName;
    if (parsedUrl.isLocalFile()) {
        fileName = QFileInfo(parsedUrl.toLocalFile()).fileName();
    } else if (!parsedUrl.path().isEmpty()) {
        fileName = QFileInfo(parsedUrl.path()).fileName();
    } else {
        fileName = QFileInfo(cleanUrl).fileName();
    }

    fileName = cleanedText(QUrl::fromPercentEncoding(fileName.toUtf8()));
    if (!fileName.isEmpty()) {
        return fileName;
    }

    return cleanUrl;
}

qint64 PlaybackHistoryModel::normalizedPosition(qint64 duration, qint64 position)
{
    if (position < MinimumResumePositionMs) {
        return 0;
    }

    if (duration > 0 && duration - position < CompletedThresholdMs) {
        return 0;
    }

    return qMin(position, duration > 0 ? duration : position);
}
