/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "nassourcemodel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

namespace {

const int MaxNasSources = 40;
const int DefaultSmbPort = 445;

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

QString appSpecificDirectory(QStandardPaths::StandardLocation location,
                             const QString &fallbackSubdir)
{
    QString base = QStandardPaths::writableLocation(location);
    if (base.isEmpty()) {
        base = QDir::home().filePath(fallbackSubdir);
    }

    return QDir(base).filePath(sailjailOrganizationName()
                               + QLatin1Char('/')
                               + sailjailApplicationName());
}

QString nasSourcesStorageDirectory()
{
    return appSpecificDirectory(QStandardPaths::GenericDataLocation,
                                QStringLiteral(".local/share"));
}

QString nasSourcesStoragePath()
{
    return QDir(nasSourcesStorageDirectory()).filePath(QStringLiteral("nas-sources.json"));
}

QStringList legacyNasSourcesStoragePaths()
{
    QStringList paths;

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        paths << QDir(appData).filePath(QStringLiteral("nas-sources.json"));
    }

    paths << QDir(QDir::home().filePath(QStringLiteral(".local/share/SailVideo")))
             .filePath(QStringLiteral("nas-sources.json"));

    paths.removeAll(nasSourcesStoragePath());
    paths.removeDuplicates();
    return paths;
}

} // namespace

NasSourceModel::NasSourceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    load();
}

int NasSourceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant NasSourceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
    case Qt::DisplayRole:
        return entry.name;
    case HostRole:
        return entry.host;
    case PortRole:
        return entry.port;
    case ShareRole:
        return entry.share;
    case PathRole:
        return entry.path;
    case DomainRole:
        return entry.domain;
    case UsernameRole:
        return entry.username;
    case GuestRole:
        return entry.guest;
    case SubtitleRole:
        return entry.subtitle;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> NasSourceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "name");
    roles.insert(HostRole, "host");
    roles.insert(PortRole, "port");
    roles.insert(ShareRole, "share");
    roles.insert(PathRole, "path");
    roles.insert(DomainRole, "domain");
    roles.insert(UsernameRole, "username");
    roles.insert(GuestRole, "guest");
    roles.insert(SubtitleRole, "subtitle");
    return roles;
}

int NasSourceModel::count() const
{
    return m_entries.size();
}

QString NasSourceModel::lastError() const
{
    return m_lastError;
}

QString NasSourceModel::storagePath() const
{
    return nasSourcesStoragePath();
}

QString NasSourceModel::storageDirectory() const
{
    return nasSourcesStorageDirectory();
}

bool NasSourceModel::addOrUpdate(const QString &name,
                                 const QString &host,
                                 int port,
                                 const QString &share,
                                 const QString &path,
                                 const QString &domain,
                                 const QString &username,
                                 bool guest)
{
    bool ok = false;
    Entry entry = entryFromUserInput(name, host, port, share, path, domain, username, guest, &ok);
    if (!ok) {
        return false;
    }

    const int existingIndex = indexOfServerSharePath(entry.host, entry.port, entry.share, entry.path);
    if (existingIndex >= 0) {
        beginRemoveRows(QModelIndex(), existingIndex, existingIndex);
        m_entries.remove(existingIndex);
        endRemoveRows();
        emit countChanged();
    }

    beginInsertRows(QModelIndex(), 0, 0);
    m_entries.prepend(entry);
    endInsertRows();
    emit countChanged();

    while (m_entries.size() > MaxNasSources) {
        const int last = m_entries.size() - 1;
        beginRemoveRows(QModelIndex(), last, last);
        m_entries.removeLast();
        endRemoveRows();
        emit countChanged();
    }

    if (!save()) {
        return false;
    }
    setLastError(QString());
    return true;
}

bool NasSourceModel::setAt(int row,
                           const QString &name,
                           const QString &host,
                           int port,
                           const QString &share,
                           const QString &path,
                           const QString &domain,
                           const QString &username,
                           bool guest)
{
    if (row < 0 || row >= m_entries.size()) {
        setLastError(tr("The selected NAS source no longer exists."));
        return false;
    }

    bool ok = false;
    Entry entry = entryFromUserInput(name, host, port, share, path, domain, username, guest, &ok);
    if (!ok) {
        return false;
    }

    const int duplicateIndex = indexOfServerSharePath(entry.host, entry.port, entry.share, entry.path);
    if (duplicateIndex >= 0 && duplicateIndex != row) {
        setLastError(tr("Another NAS source already uses this server, share and folder."));
        return false;
    }

    m_entries[row] = entry;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed,
                     QVector<int>() << NameRole << HostRole << PortRole
                                     << ShareRole << PathRole << DomainRole
                                     << UsernameRole << GuestRole << SubtitleRole);
    if (!save()) {
        return false;
    }
    setLastError(QString());
    return true;
}

void NasSourceModel::removeAt(int row)
{
    if (row < 0 || row >= m_entries.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_entries.remove(row);
    endRemoveRows();
    emit countChanged();
    save();
}

void NasSourceModel::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }

    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
    save();
}

QString NasSourceModel::nameAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).name : QString();
}

QString NasSourceModel::hostAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).host : QString();
}

int NasSourceModel::portAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).port : DefaultSmbPort;
}

QString NasSourceModel::shareAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).share : QString();
}

QString NasSourceModel::pathAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).path : QString();
}

QString NasSourceModel::domainAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).domain : QString();
}

QString NasSourceModel::usernameAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).username : QString();
}

bool NasSourceModel::guestAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).guest : true;
}

int NasSourceModel::indexForLocation(const QString &host, int port, const QString &share) const
{
    return indexOfServerShare(normalizedHost(host),
                              port > 0 ? port : DefaultSmbPort,
                              normalizedShare(share));
}

int NasSourceModel::indexForLocationAndPath(const QString &host,
                                            int port,
                                            const QString &share,
                                            const QString &path) const
{
    return indexOfServerSharePath(normalizedHost(host),
                                  port > 0 ? port : DefaultSmbPort,
                                  normalizedShare(share),
                                  normalizedPath(path));
}

void NasSourceModel::load()
{
    QVector<Entry> loadedEntries;
    const QString primaryPath = nasSourcesStoragePath();

    auto appendJsonEntriesFromFile = [this](const QString &storagePath,
                                            QVector<Entry> *target,
                                            QString *errorString) -> bool {
        QFile file(storagePath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorString) {
                *errorString = file.errorString();
            }
            return false;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorString) {
                *errorString = parseError.errorString();
            }
            return false;
        }

        const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();
        for (int i = 0; i < entries.size() && target->size() < MaxNasSources; ++i) {
            const QJsonObject object = entries.at(i).toObject();

            Entry entry;
            entry.host = normalizedHost(object.value(QStringLiteral("host")).toString());
            entry.port = object.value(QStringLiteral("port")).toInt(DefaultSmbPort);
            if (entry.port <= 0) {
                entry.port = DefaultSmbPort;
            }
            entry.share = normalizedShare(object.value(QStringLiteral("share")).toString());
            entry.path = normalizedPath(object.value(QStringLiteral("path")).toString());
            entry.domain = cleanedText(object.value(QStringLiteral("domain")).toString());
            entry.username = cleanedText(object.value(QStringLiteral("username")).toString());
            entry.guest = object.value(QStringLiteral("guest")).toBool(true);
            entry.name = displayName(entry, object.value(QStringLiteral("name")).toString());
            entry.subtitle = subtitle(entry);

            if (!entry.host.isEmpty() && !entry.share.isEmpty()
                    && indexOfServerSharePath(entry.host, entry.port, entry.share, entry.path) < 0) {
                target->append(entry);
            }
        }

        return true;
    };

    bool loadedFromLegacyPath = false;

    if (QFile::exists(primaryPath)) {
        QString errorString;
        if (!appendJsonEntriesFromFile(primaryPath, &loadedEntries, &errorString)) {
            setLastError(tr("Could not read saved NAS sources from %1: %2")
                         .arg(primaryPath, errorString));
            qWarning() << "SailVideo: failed to read primary NAS source store"
                       << primaryPath << errorString;
            return;
        }

        qDebug() << "SailVideo: loaded NAS sources from" << primaryPath
                 << "count" << loadedEntries.size();
    } else {
        const QStringList legacyPaths = legacyNasSourcesStoragePaths();
        for (const QString &legacyPath : legacyPaths) {
            if (!QFile::exists(legacyPath)) {
                continue;
            }

            QString errorString;
            QVector<Entry> legacyEntries;
            if (appendJsonEntriesFromFile(legacyPath, &legacyEntries, &errorString)) {
                loadedEntries = legacyEntries;
                loadedFromLegacyPath = true;
                qWarning() << "SailVideo: migrating NAS sources from legacy path"
                           << legacyPath << "to" << primaryPath;
                break;
            }

            qWarning() << "SailVideo: failed to read legacy NAS source store"
                       << legacyPath << errorString;
        }

        if (loadedEntries.isEmpty()) {
            QSettings settings;
            settings.beginGroup(QStringLiteral("nasSources"));
            const int count = settings.value(QStringLiteral("count"), 0).toInt();

            for (int i = 0; i < count && i < MaxNasSources; ++i) {
                settings.beginGroup(QString::number(i));

                Entry entry;
                entry.host = normalizedHost(settings.value(QStringLiteral("host")).toString());
                entry.port = settings.value(QStringLiteral("port"), DefaultSmbPort).toInt();
                if (entry.port <= 0) {
                    entry.port = DefaultSmbPort;
                }
                entry.share = normalizedShare(settings.value(QStringLiteral("share")).toString());
                entry.path = normalizedPath(settings.value(QStringLiteral("path")).toString());
                entry.domain = cleanedText(settings.value(QStringLiteral("domain")).toString());
                entry.username = cleanedText(settings.value(QStringLiteral("username")).toString());
                entry.guest = settings.value(QStringLiteral("guest"), true).toBool();
                entry.name = displayName(entry, settings.value(QStringLiteral("name")).toString());
                entry.subtitle = subtitle(entry);

                settings.endGroup();

                if (!entry.host.isEmpty() && !entry.share.isEmpty()) {
                    loadedEntries.append(entry);
                }
            }

            settings.endGroup();

            if (!loadedEntries.isEmpty()) {
                loadedFromLegacyPath = true;
                qWarning() << "SailVideo: migrating NAS sources from legacy QSettings store to"
                           << primaryPath;
            }
        }
    }

    beginResetModel();
    m_entries = loadedEntries;
    endResetModel();
    emit countChanged();

    if (loadedFromLegacyPath) {
        save();
    }
}

bool NasSourceModel::save() const
{
    const QString directoryPath = nasSourcesStorageDirectory();
    QDir directory(directoryPath);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        setLastError(tr("Could not create SailVideo data directory for NAS sources: %1")
                     .arg(directoryPath));
        qWarning() << "SailVideo: failed to create NAS source directory"
                   << directoryPath;
        return false;
    }

    QJsonArray entries;
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &entry = m_entries.at(i);

        QJsonObject object;
        object.insert(QStringLiteral("name"), entry.name);
        object.insert(QStringLiteral("host"), entry.host);
        object.insert(QStringLiteral("port"), entry.port);
        object.insert(QStringLiteral("share"), entry.share);
        object.insert(QStringLiteral("path"), entry.path);
        object.insert(QStringLiteral("domain"), entry.domain);
        object.insert(QStringLiteral("username"), entry.username);
        object.insert(QStringLiteral("guest"), entry.guest);

        entries.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("entries"), entries);

    const QString storagePath = nasSourcesStoragePath();
    QSaveFile file(storagePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastError(tr("Could not save NAS sources to %1: %2")
                     .arg(storagePath, file.errorString()));
        qWarning() << "SailVideo: failed to open NAS source store for writing"
                   << storagePath << file.errorString();
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setLastError(tr("Could not save NAS sources to %1: %2")
                     .arg(storagePath, file.errorString()));
        qWarning() << "SailVideo: failed to commit NAS source store"
                   << storagePath << file.errorString();
        return false;
    }

    qDebug() << "SailVideo: saved NAS sources to" << storagePath
             << "count" << m_entries.size();

    return true;
}

int NasSourceModel::indexOfServerShare(const QString &host, int port, const QString &share) const
{
    const QString cleanHost = normalizedHost(host);
    const int cleanPort = port > 0 ? port : DefaultSmbPort;
    const QString cleanShare = normalizedShare(share);
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &entry = m_entries.at(i);
        if (entry.host.compare(cleanHost, Qt::CaseInsensitive) == 0
                && entry.port == cleanPort
                && entry.share.compare(cleanShare, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

int NasSourceModel::indexOfServerSharePath(const QString &host,
                                           int port,
                                           const QString &share,
                                           const QString &path) const
{
    const QString cleanHost = normalizedHost(host);
    const int cleanPort = port > 0 ? port : DefaultSmbPort;
    const QString cleanShare = normalizedShare(share);
    const QString cleanPath = normalizedPath(path);
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &entry = m_entries.at(i);
        if (entry.host.compare(cleanHost, Qt::CaseInsensitive) == 0
                && entry.port == cleanPort
                && entry.share.compare(cleanShare, Qt::CaseInsensitive) == 0
                && entry.path.compare(cleanPath, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

void NasSourceModel::setLastError(const QString &message) const
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit const_cast<NasSourceModel *>(this)->lastErrorChanged();
}

NasSourceModel::Entry NasSourceModel::entryFromUserInput(const QString &name,
                                                         const QString &host,
                                                         int port,
                                                         const QString &share,
                                                         const QString &path,
                                                         const QString &domain,
                                                         const QString &username,
                                                         bool guest,
                                                         bool *ok) const
{
    if (ok) {
        *ok = false;
    }

    const QString impliedPath = pathPartFromShareInput(share);
    QString mergedPath;
    if (!impliedPath.isEmpty() && !cleanedText(path).isEmpty()) {
        mergedPath = impliedPath + QLatin1Char('/') + cleanedText(path);
    } else if (!impliedPath.isEmpty()) {
        mergedPath = impliedPath;
    } else {
        mergedPath = path;
    }

    Entry entry;
    entry.host = normalizedHost(host);
    entry.port = port > 0 ? port : DefaultSmbPort;
    entry.share = normalizedShare(share);
    entry.path = normalizedPath(mergedPath);
    entry.domain = cleanedText(domain);
    entry.username = cleanedText(username);
    entry.guest = guest;
    entry.name = displayName(entry, name);
    entry.subtitle = subtitle(entry);

    if (entry.host.isEmpty()) {
        setLastError(tr("Enter a NAS host name or IP address."));
        return entry;
    }
    if (entry.share.isEmpty()) {
        setLastError(tr("Enter a share name."));
        return entry;
    }
    if (!entry.guest && entry.username.isEmpty()) {
        setLastError(tr("Enter a username or enable guest access."));
        return entry;
    }

    if (ok) {
        *ok = true;
    }
    return entry;
}

QString NasSourceModel::cleanedText(const QString &value)
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

QString NasSourceModel::normalizedHost(const QString &host)
{
    QString text = cleanedText(host);
    text.remove(QStringLiteral("smb://"), Qt::CaseInsensitive);
    text.remove(QStringLiteral("smb2://"), Qt::CaseInsensitive);
    while (text.startsWith(QLatin1Char('/'))) {
        text.remove(0, 1);
    }
    const int slash = text.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        text = text.left(slash);
    }
    const int at = text.lastIndexOf(QLatin1Char('@'));
    if (at >= 0) {
        text = text.mid(at + 1);
    }
    const int colon = text.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        text = text.left(colon);
    }
    return text.trimmed();
}

QString NasSourceModel::normalizedShare(const QString &share)
{
    QString text = cleanedText(share);
    text.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (text.startsWith(QLatin1Char('/'))) {
        text.remove(0, 1);
    }
    const int slash = text.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        text = text.left(slash);
    }
    return text.trimmed();
}

QString NasSourceModel::normalizedPath(const QString &path)
{
    QString text = cleanedText(path);
    text.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (text.startsWith(QLatin1Char('/'))) {
        text.remove(0, 1);
    }
    while (text.endsWith(QLatin1Char('/'))) {
        text.chop(1);
    }

    QStringList parts;
    const QStringList rawParts = text.split(QLatin1Char('/'), QString::SkipEmptyParts);
    for (int i = 0; i < rawParts.size(); ++i) {
        const QString part = rawParts.at(i).trimmed();
        if (part.isEmpty() || part == QLatin1String(".")) {
            continue;
        }
        if (part == QLatin1String("..")) {
            if (!parts.isEmpty()) {
                parts.removeLast();
            }
            continue;
        }
        parts.append(part);
    }
    return parts.join(QLatin1Char('/'));
}

QString NasSourceModel::pathPartFromShareInput(const QString &share)
{
    QString text = cleanedText(share);
    text.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (text.startsWith(QLatin1Char('/'))) {
        text.remove(0, 1);
    }

    const int slash = text.indexOf(QLatin1Char('/'));
    if (slash < 0) {
        return QString();
    }

    return normalizedPath(text.mid(slash + 1));
}

QString NasSourceModel::displayName(const Entry &entry, const QString &fallbackName)
{
    const QString cleanName = cleanedText(fallbackName);
    if (!cleanName.isEmpty()) {
        return cleanName;
    }
    if (!entry.host.isEmpty() && !entry.share.isEmpty()) {
        QString text = QStringLiteral("%1/%2").arg(entry.host, entry.share);
        if (!entry.path.isEmpty()) {
            text += QStringLiteral("/") + entry.path;
        }
        return text;
    }
    return entry.host;
}

QString NasSourceModel::subtitle(const Entry &entry)
{
    QString text = QStringLiteral("SMB · //%1/%2").arg(entry.host, entry.share);
    if (!entry.path.isEmpty()) {
        text += QStringLiteral("/") + entry.path;
    }
    if (entry.port > 0 && entry.port != DefaultSmbPort) {
        text += QStringLiteral(" · port %1").arg(entry.port);
    }
    text += entry.guest
            ? QStringLiteral(" · guest")
            : QStringLiteral(" · %1").arg(entry.username);
    return text;
}
