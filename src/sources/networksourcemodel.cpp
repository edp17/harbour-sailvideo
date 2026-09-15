/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "networksourcemodel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

const int MaxNetworkSources = 40;

QString organizationName()
{
    const QString name = QCoreApplication::organizationName().trimmed();
    return name.isEmpty() ? QStringLiteral("org.edp17") : name;
}

QString applicationName()
{
    const QString name = QCoreApplication::applicationName().trimmed();
    return name.isEmpty() ? QStringLiteral("SailVideo") : name;
}

QString defaultStorageDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        base = QDir::home().filePath(QStringLiteral(".local/share"));
    }
    return QDir(base).filePath(organizationName() + QLatin1Char('/') + applicationName());
}

} // namespace

NetworkSourceModel::NetworkSourceModel(const QString &storageDirectory, QObject *parent)
    : QAbstractListModel(parent)
    , m_storageDirectory(storageDirectory.trimmed().isEmpty()
                         ? defaultStorageDirectory()
                         : storageDirectory.trimmed())
{
    load();
}

int NetworkSourceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant NetworkSourceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:
    case Qt::DisplayRole:
        return entry.name;
    case UrlRole:
        return entry.url;
    case SchemeRole:
        return entry.scheme;
    case SubtitleRole:
        return entry.subtitle;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> NetworkSourceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "name");
    roles.insert(UrlRole, "url");
    roles.insert(SchemeRole, "scheme");
    roles.insert(SubtitleRole, "subtitle");
    return roles;
}

int NetworkSourceModel::count() const
{
    return m_entries.size();
}

QString NetworkSourceModel::lastError() const
{
    return m_lastError;
}

QString NetworkSourceModel::storagePath() const
{
    return QDir(m_storageDirectory).filePath(QStringLiteral("network-sources.json"));
}

bool NetworkSourceModel::addOrUpdate(const QString &name, const QString &url)
{
    bool ok = false;
    Entry entry = entryFromUserInput(name, url, &ok);
    if (!ok) {
        return false;
    }

    const int existingIndex = indexOfUrl(entry.url);
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

    while (m_entries.size() > MaxNetworkSources) {
        const int last = m_entries.size() - 1;
        beginRemoveRows(QModelIndex(), last, last);
        m_entries.removeLast();
        endRemoveRows();
        emit countChanged();
    }

    save();
    setLastError(QString());
    return true;
}

bool NetworkSourceModel::setAt(int row, const QString &name, const QString &url)
{
    if (row < 0 || row >= m_entries.size()) {
        setLastError(tr("The selected network source no longer exists."));
        return false;
    }

    bool ok = false;
    const Entry entry = entryFromUserInput(name, url, &ok);
    if (!ok) {
        return false;
    }

    const int duplicateIndex = indexOfUrl(entry.url);
    if (duplicateIndex >= 0 && duplicateIndex != row) {
        setLastError(tr("Another network source already uses this URL."));
        return false;
    }

    m_entries[row] = entry;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed,
                     QVector<int>() << NameRole << UrlRole
                                     << SchemeRole << SubtitleRole);
    save();
    setLastError(QString());
    return true;
}

void NetworkSourceModel::removeAt(int row)
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

void NetworkSourceModel::clear()
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

QString NetworkSourceModel::nameAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return QString();
    }
    return m_entries.at(row).name;
}

QString NetworkSourceModel::urlAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return QString();
    }
    return m_entries.at(row).url;
}

QString NetworkSourceModel::normalizedUrl(const QString &url) const
{
    const QString normalized = normalizedPlaybackUrl(url);
    if (normalized.isEmpty()) {
        setLastError(tr("Enter a valid HTTP or HTTPS media URL."));
        return QString();
    }

    const QUrl parsed(normalized);
    if (!supportedScheme(parsed.scheme())) {
        setLastError(tr("Only HTTP and HTTPS network URLs are supported in this build."));
        return QString();
    }

    if (!parsed.userName().isEmpty() || !parsed.password().isEmpty()) {
        setLastError(tr("Credentials inside URLs are not supported. They would be stored in plain text."));
        return QString();
    }

    setLastError(QString());
    return normalized;
}

QString NetworkSourceModel::suggestedNameForUrl(const QString &url) const
{
    const QString normalized = normalizedPlaybackUrl(url);
    if (normalized.isEmpty()) {
        return QString();
    }
    return displayNameForUrl(normalized);
}

bool NetworkSourceModel::isSupportedPlaybackUrl(const QString &url) const
{
    const QString normalized = normalizedPlaybackUrl(url);
    if (normalized.isEmpty()) {
        return false;
    }

    const QUrl parsed(normalized);
    return supportedScheme(parsed.scheme())
            && parsed.userName().isEmpty()
            && parsed.password().isEmpty();
}

void NetworkSourceModel::load()
{
    QVector<Entry> loadedEntries;

    QFile file(storagePath());
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        file.close();
        const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();

        for (int i = 0; i < entries.size() && loadedEntries.size() < MaxNetworkSources; ++i) {
            const QJsonObject object = entries.at(i).toObject();
            const QString url = normalizedPlaybackUrl(object.value(QStringLiteral("url")).toString());
            const QString name = object.value(QStringLiteral("name")).toString();
            const QUrl parsed(url);
            if (url.isEmpty() || !supportedScheme(parsed.scheme())) {
                continue;
            }

            Entry entry;
            entry.url = url;
            entry.name = displayNameForUrl(url, name);
            entry.scheme = parsed.scheme().toUpper();
            entry.subtitle = subtitleForUrl(url);
            loadedEntries.append(entry);
        }
    }

    if (loadedEntries.isEmpty()) {
        loadLegacySettings(&loadedEntries);
        if (!loadedEntries.isEmpty()) {
            m_entries = loadedEntries;
            save();
        }
    }

    if (!loadedEntries.isEmpty()) {
        beginResetModel();
        m_entries = loadedEntries;
        endResetModel();
        emit countChanged();
    }
}

void NetworkSourceModel::loadLegacySettings(QVector<Entry> *entries) const
{
    if (!entries) {
        return;
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("networkSources"));
    const int count = settings.value(QStringLiteral("count"), 0).toInt();

    for (int i = 0; i < count && entries->size() < MaxNetworkSources; ++i) {
        settings.beginGroup(QString::number(i));
        const QString url = normalizedPlaybackUrl(settings.value(QStringLiteral("url")).toString());
        const QString name = settings.value(QStringLiteral("name")).toString();
        settings.endGroup();

        const QUrl parsed(url);
        if (url.isEmpty() || !supportedScheme(parsed.scheme())) {
            continue;
        }

        Entry entry;
        entry.url = url;
        entry.name = displayNameForUrl(url, name);
        entry.scheme = parsed.scheme().toUpper();
        entry.subtitle = subtitleForUrl(url);
        entries->append(entry);
    }

    settings.endGroup();
}

void NetworkSourceModel::save() const
{
    QDir directory(m_storageDirectory);
    if (!directory.exists()) {
        directory.mkpath(QStringLiteral("."));
    }

    QJsonArray entries;
    for (const Entry &entry : m_entries) {
        QJsonObject object;
        object.insert(QStringLiteral("name"), entry.name);
        object.insert(QStringLiteral("url"), entry.url);
        entries.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("entries"), entries);

    QFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(tr("Could not save URL sources."));
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
}

int NetworkSourceModel::indexOfUrl(const QString &url) const
{
    const QString cleanUrl = normalizedPlaybackUrl(url);
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).url == cleanUrl) {
            return i;
        }
    }
    return -1;
}

void NetworkSourceModel::setLastError(const QString &message) const
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit const_cast<NetworkSourceModel *>(this)->lastErrorChanged();
}

NetworkSourceModel::Entry NetworkSourceModel::entryFromUserInput(const QString &name,
                                                                 const QString &url,
                                                                 bool *ok) const
{
    if (ok) {
        *ok = false;
    }

    Entry entry;
    entry.url = normalizedUrl(url);
    if (entry.url.isEmpty()) {
        return entry;
    }

    const QUrl parsed(entry.url);
    entry.name = displayNameForUrl(entry.url, name);
    entry.scheme = parsed.scheme().toUpper();
    entry.subtitle = subtitleForUrl(entry.url);

    if (entry.name.isEmpty()) {
        setLastError(tr("Enter a name or a URL with a usable file name."));
        return entry;
    }

    if (ok) {
        *ok = true;
    }
    return entry;
}

QString NetworkSourceModel::cleanedText(const QString &value)
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

QString NetworkSourceModel::normalizedPlaybackUrl(const QString &value)
{
    QString text = cleanedText(value);
    if (text.isEmpty()) {
        return QString();
    }

    QUrl url = QUrl::fromUserInput(text);
    if (!url.isValid() || url.scheme().isEmpty()) {
        return QString();
    }

    const QString scheme = url.scheme().toLower();
    url.setScheme(scheme);
    return url.toString(QUrl::FullyEncoded);
}

QString NetworkSourceModel::displayNameForUrl(const QString &url,
                                              const QString &fallbackName)
{
    const QString cleanName = cleanedText(fallbackName);
    if (!cleanName.isEmpty()) {
        return cleanName;
    }

    const QUrl parsed(url);
    QString pathName;
    if (!parsed.path().isEmpty() && parsed.path() != QLatin1String("/")) {
        pathName = QFileInfo(parsed.path()).fileName();
        pathName = QUrl::fromPercentEncoding(pathName.toUtf8());
    }

    if (!pathName.isEmpty()) {
        return pathName;
    }

    if (!parsed.host().isEmpty()) {
        return parsed.host();
    }

    return cleanedText(url);
}

QString NetworkSourceModel::subtitleForUrl(const QString &url)
{
    const QUrl parsed(url);
    QString text = parsed.host();
    if (!parsed.path().isEmpty() && parsed.path() != QLatin1String("/")) {
        text += parsed.path();
    }
    if (text.isEmpty()) {
        text = url;
    }
    return QUrl::fromPercentEncoding(text.toUtf8());
}

bool NetworkSourceModel::supportedScheme(const QString &scheme)
{
    const QString cleanScheme = scheme.toLower();
    return cleanScheme == QLatin1String("http")
            || cleanScheme == QLatin1String("https");
}
