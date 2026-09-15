/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "localvideomodel.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QtConcurrent>

#include <algorithm>

namespace {

const int MaxLocalVideos = 5000;

QString cleanedPath(const QString &path)
{
    return QDir::cleanPath(path.trimmed());
}

void addRoot(QStringList *roots, QSet<QString> *seen, const QString &path)
{
    const QString clean = cleanedPath(path);
    if (clean.isEmpty()) {
        return;
    }

    const QFileInfo info(clean);
    if (!info.exists() || !info.isDir() || !info.isReadable()) {
        return;
    }

    const QString canonical = info.canonicalFilePath().isEmpty()
            ? info.absoluteFilePath()
            : info.canonicalFilePath();
    if (seen->contains(canonical)) {
        return;
    }

    seen->insert(canonical);
    roots->append(info.absoluteFilePath());
}

QStringList standardLocations(QStandardPaths::StandardLocation location)
{
    QStringList result;
    const QStringList locations = QStandardPaths::standardLocations(location);
    for (const QString &path : locations) {
        if (!path.trimmed().isEmpty()) {
            result.append(path);
        }
    }
    return result;
}

QString trackerProgram()
{
    QString program = QStandardPaths::findExecutable(QStringLiteral("tracker3"));
    if (!program.isEmpty()) {
        return program;
    }

    program = QStandardPaths::findExecutable(QStringLiteral("tracker"));
    if (!program.isEmpty()) {
        return program;
    }

    return QString();
}

QStringList trackerArguments(const QString &program, const QString &query)
{
    Q_UNUSED(program)

    // Sailfish OS exposes the media index through the Tracker3 Miner.Files
    // D-Bus endpoint. Calling `tracker3 sparql -q ...` without an endpoint can
    // wait indefinitely because it has no database/service to query.
    return QStringList() << QStringLiteral("sparql")
                         << QStringLiteral("-b")
                         << QStringLiteral("org.freedesktop.Tracker3.Miner.Files")
                         << QStringLiteral("-q")
                         << query;
}

QStringList extractFileUrls(const QString &output)
{
    QStringList urls;
    QSet<QString> seen;

    const QRegularExpression expression(QStringLiteral("file://[^\\s\\|<>\\\"']+"));
    QRegularExpressionMatchIterator iterator = expression.globalMatch(output);

    while (iterator.hasNext()) {
        QString url = iterator.next().captured(0).trimmed();
        while (url.endsWith(QLatin1Char(')'))
               || url.endsWith(QLatin1Char(']'))
               || url.endsWith(QLatin1Char(','))) {
            url.chop(1);
        }

        if (url.isEmpty() || seen.contains(url)) {
            continue;
        }

        seen.insert(url);
        urls.append(url);
    }

    return urls;
}

void sortEntries(QVector<LocalVideoModel::Entry> *entries)
{
    std::sort(entries->begin(), entries->end(), [](const LocalVideoModel::Entry &a,
                                                   const LocalVideoModel::Entry &b) {
        if (a.modified != b.modified) {
            return a.modified > b.modified;
        }
        return QString::localeAwareCompare(a.title, b.title) < 0;
    });
}

} // namespace

LocalVideoModel::LocalVideoModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_scanWatcher, &QFutureWatcher<QVector<Entry>>::finished,
            this, &LocalVideoModel::handleScanFinished);
}

LocalVideoModel::~LocalVideoModel()
{
    if (m_scanWatcher.isRunning()) {
        m_scanWatcher.waitForFinished();
    }
}

int LocalVideoModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant LocalVideoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TitleRole:
    case Qt::DisplayRole:
        return entry.title;
    case PathRole:
        return entry.path;
    case UrlRole:
        return entry.url;
    case FolderRole:
        return entry.folder;
    case FolderPathRole:
        return entry.folderPath;
    case FolderNameRole:
        return entry.folderName;
    case SubtitleRole:
        return entry.subtitle;
    case SizeRole:
        return entry.size;
    case SizeTextRole:
        return entry.sizeText;
    case ModifiedRole:
        return entry.modifiedText;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LocalVideoModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(TitleRole, "title");
    roles.insert(PathRole, "path");
    roles.insert(UrlRole, "url");
    roles.insert(FolderRole, "folder");
    roles.insert(FolderPathRole, "folderPath");
    roles.insert(FolderNameRole, "folderName");
    roles.insert(SubtitleRole, "subtitle");
    roles.insert(SizeRole, "size");
    roles.insert(SizeTextRole, "sizeText");
    roles.insert(ModifiedRole, "modified");
    return roles;
}

int LocalVideoModel::count() const
{
    return m_entries.size();
}

bool LocalVideoModel::scanning() const
{
    return m_scanning;
}

QString LocalVideoModel::lastError() const
{
    return m_lastError;
}

void LocalVideoModel::refresh()
{
    if (m_scanWatcher.isRunning()) {
        return;
    }

    setLastError(QString());
    setScanning(true);
    m_scanWatcher.setFuture(QtConcurrent::run(&LocalVideoModel::scanEntries));
}

QString LocalVideoModel::titleAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return QString();
    }
    return m_entries.at(row).title;
}

QString LocalVideoModel::urlAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return QString();
    }
    return m_entries.at(row).url;
}

void LocalVideoModel::handleScanFinished()
{
    const QVector<Entry> entries = m_scanWatcher.result();

    beginResetModel();
    m_entries = entries;
    endResetModel();

    emit countChanged();
    setScanning(false);

    if (m_entries.isEmpty()) {
        setLastError(tr("No local videos were found in the media index or in Videos, Downloads, Documents and removable media."));
    }
}

QVector<LocalVideoModel::Entry> LocalVideoModel::scanEntries()
{
    QVector<Entry> entries = scanTrackerEntries();
    if (!entries.isEmpty()) {
        qInfo() << "SailVideo: local video page loaded" << entries.size()
                << "entries from Tracker media index";
        return entries;
    }

    entries = scanFilesystemEntries();
    qInfo() << "SailVideo: local video page loaded" << entries.size()
            << "entries from filesystem fallback";
    return entries;
}

QVector<LocalVideoModel::Entry> LocalVideoModel::scanTrackerEntries()
{
    QVector<Entry> entries;
    QSet<QString> seen;

    const QString program = trackerProgram();
    if (program.isEmpty()) {
        qWarning() << "SailVideo: tracker command not found; local video page will use filesystem fallback";
        return entries;
    }

    const QStringList queries = QStringList()
            // Current Tracker3 graph name used by tracker-miners.
            << QStringLiteral(
                   "SELECT (nie:isStoredAs(?x) AS ?url) "
                   "WHERE { "
                   "  GRAPH tracker:Video { "
                   "    ?x a nmm:Video . "
                   "    ?x nie:isStoredAs ?file . "
                   "    ?file nie:dataSource/tracker:available true . "
                   "  } "
                   "} "
                   "ORDER BY DESC(nfo:fileLastModified(?file)) "
                   "LIMIT 5000")
            // Some Tracker3 builds use the plural graph name.
            << QStringLiteral(
                   "SELECT (nie:isStoredAs(?x) AS ?url) "
                   "WHERE { "
                   "  GRAPH tracker:Videos { "
                   "    ?x a nmm:Video . "
                   "    ?x nie:isStoredAs ?file . "
                   "    ?file nie:dataSource/tracker:available true . "
                   "  } "
                   "} "
                   "ORDER BY DESC(nfo:fileLastModified(?file)) "
                   "LIMIT 5000");

    for (const QString &query : queries) {
        QProcess process;
        const QStringList arguments = trackerArguments(program, query);
        qInfo() << "SailVideo: querying Tracker video index via"
                << program << arguments.mid(0, 3);

        process.start(program, arguments);
        if (!process.waitForStarted(2000)) {
            qWarning() << "SailVideo: could not start Tracker query:" << process.errorString();
            continue;
        }

        if (!process.waitForFinished(8000)) {
            qWarning() << "SailVideo: Tracker video query timed out";
            process.kill();
            process.waitForFinished(500);
            continue;
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            const QString errorText = QString::fromUtf8(process.readAllStandardError()).trimmed();
            qWarning() << "SailVideo: Tracker local video query failed:"
                       << (errorText.isEmpty() ? process.errorString() : errorText);
            continue;
        }

        const QString output = QString::fromUtf8(process.readAllStandardOutput());
        const QStringList urls = extractFileUrls(output);
        qInfo() << "SailVideo: Tracker query returned" << urls.size() << "video URLs";
        for (const QString &urlString : urls) {
            if (entries.size() >= MaxLocalVideos) {
                break;
            }

            const QUrl url(urlString);
            const QString path = url.toLocalFile();
            appendEntryForPath(&entries, &seen, path);
        }

        if (entries.size() >= MaxLocalVideos) {
            break;
        }
    }

    sortEntries(&entries);
    return entries;
}

QVector<LocalVideoModel::Entry> LocalVideoModel::scanFilesystemEntries()
{
    QVector<Entry> entries;
    QSet<QString> seen;

    const QStringList roots = scanRoots();
    for (const QString &root : roots) {
        QDirIterator iterator(root,
                              QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext() && entries.size() < MaxLocalVideos) {
            appendEntryForPath(&entries, &seen, iterator.next());
        }
    }

    sortEntries(&entries);
    return entries;
}

QStringList LocalVideoModel::scanRoots()
{
    QStringList roots;
    QSet<QString> seen;

    for (const QString &path : standardLocations(QStandardPaths::MoviesLocation)) {
        addRoot(&roots, &seen, path);
    }
    for (const QString &path : standardLocations(QStandardPaths::DownloadLocation)) {
        addRoot(&roots, &seen, path);
    }
    const QDir home(QDir::homePath());
    addRoot(&roots, &seen, home.filePath(QStringLiteral("Videos")));
    addRoot(&roots, &seen, home.filePath(QStringLiteral("Downloads")));

    const QString userName = QFileInfo(QDir::homePath()).fileName();
    addRoot(&roots, &seen, QStringLiteral("/run/media/") + userName);
    addRoot(&roots, &seen, QStringLiteral("/media/") + userName);
    addRoot(&roots, &seen, QStringLiteral("/media/sdcard"));

    return roots;
}

bool LocalVideoModel::isVideoFile(const QString &path)
{
    static const QStringList extensions = QStringList()
            << QStringLiteral("3g2")
            << QStringLiteral("3gp")
            << QStringLiteral("asf")
            << QStringLiteral("avi")
            << QStringLiteral("divx")
            << QStringLiteral("flv")
            << QStringLiteral("m2ts")
            << QStringLiteral("m4v")
            << QStringLiteral("mkv")
            << QStringLiteral("mov")
            << QStringLiteral("mp4")
            << QStringLiteral("mpeg")
            << QStringLiteral("mpg")
            << QStringLiteral("mts")
            << QStringLiteral("ogm")
            << QStringLiteral("ogv")
            << QStringLiteral("ts")
            << QStringLiteral("vob")
            << QStringLiteral("webm")
            << QStringLiteral("wmv");

    return extensions.contains(QFileInfo(path).suffix().toLower());
}

QString LocalVideoModel::prettyFolder(const QString &path)
{
    const QString clean = cleanedPath(path);
    const QString home = cleanedPath(QDir::homePath());

    if (!home.isEmpty() && clean == home) {
        return QStringLiteral("~");
    }

    if (!home.isEmpty() && clean.startsWith(home + QLatin1Char('/'))) {
        return QStringLiteral("~") + clean.mid(home.size());
    }

    return clean;
}

QString LocalVideoModel::prettySize(qint64 bytes)
{
    if (bytes >= 1024LL * 1024LL * 1024LL) {
        return QStringLiteral("%1 GB").arg(bytes / double(1024LL * 1024LL * 1024LL), 0, 'f', 1);
    }
    if (bytes >= 1024LL * 1024LL) {
        return QStringLiteral("%1 MB").arg(bytes / double(1024LL * 1024LL), 0, 'f', 1);
    }
    if (bytes >= 1024LL) {
        return QStringLiteral("%1 KB").arg(qRound64(bytes / double(1024LL)));
    }
    return QStringLiteral("%1 B").arg(bytes);
}

QString LocalVideoModel::subtitleForFile(const QFileInfo &info)
{
    const QString folder = prettyFolder(info.absolutePath());
    const QString size = prettySize(info.size());
    const QString modified = QLocale().toString(info.lastModified(), QLocale::ShortFormat);
    return QStringLiteral("%1 · %2 · %3").arg(folder, size, modified);
}

LocalVideoModel::Entry LocalVideoModel::entryForFile(const QFileInfo &info)
{
    Entry entry;
    entry.path = info.absoluteFilePath();
    entry.url = QUrl::fromLocalFile(entry.path).toString();
    entry.title = info.completeBaseName().trimmed().isEmpty()
            ? info.fileName()
            : info.completeBaseName();
    entry.folderPath = info.absolutePath();
    entry.folder = prettyFolder(entry.folderPath);
    entry.folderName = QDir(entry.folderPath).dirName();
    if (entry.folderName.trimmed().isEmpty()) {
        entry.folderName = entry.folder;
    }
    entry.size = info.size();
    entry.sizeText = prettySize(entry.size);
    entry.modified = info.lastModified();
    entry.modifiedText = QLocale().toString(entry.modified, QLocale::ShortFormat);
    entry.subtitle = subtitleForFile(info);
    return entry;
}

void LocalVideoModel::appendEntryForPath(QVector<Entry> *entries,
                                         QSet<QString> *seen,
                                         const QString &path)
{
    if (!entries || !seen || path.trimmed().isEmpty()) {
        return;
    }

    if (!isVideoFile(path)) {
        return;
    }

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !info.isReadable()) {
        return;
    }

    const QString canonical = info.canonicalFilePath().isEmpty()
            ? info.absoluteFilePath()
            : info.canonicalFilePath();
    if (seen->contains(canonical)) {
        return;
    }

    seen->insert(canonical);
    entries->append(entryForFile(info));
}

void LocalVideoModel::setScanning(bool scanning)
{
    if (m_scanning == scanning) {
        return;
    }
    m_scanning = scanning;
    emit scanningChanged();
}

void LocalVideoModel::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}
