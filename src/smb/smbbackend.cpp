/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "smbbackend.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLibrary>
#include <QRegularExpression>
#include <QStringList>
#include <QThread>
#include <QtConcurrent>
#include <QUrl>

#include <algorithm>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>

extern "C" {

struct smb2_context;
struct smb2dir;
struct smb2fh;

struct smb2_stat_64 {
    uint32_t smb2_type;
    uint32_t smb2_nlink;
    uint64_t smb2_ino;
    uint64_t smb2_size;
    uint64_t smb2_atime;
    uint64_t smb2_atime_nsec;
    uint64_t smb2_mtime;
    uint64_t smb2_mtime_nsec;
    uint64_t smb2_ctime;
    uint64_t smb2_ctime_nsec;
    uint64_t smb2_btime;
    uint64_t smb2_btime_nsec;
    uint32_t smb2_attributes;
    uint32_t smb2_reparse_tag;
};

struct smb2dirent {
    const char *name;
    struct smb2_stat_64 st;
};

} // extern "C"

namespace {

const int DefaultSmbPort = 445;
const uint32_t Smb2TypeDirectory = 0x00000001;
const qint64 DefaultReadChunk = 64 * 1024;
const qint64 MinimumReadChunk = 16 * 1024;
const qint64 MaximumReadChunk = 1024 * 1024;

QStringList privateLibraryCandidates()
{
    QStringList names;

#ifdef SAILVIDEO_PRIVATE_LIB_DIR
    const QString configuredPrivateDir = QString::fromUtf8(SAILVIDEO_PRIVATE_LIB_DIR);
    if (!configuredPrivateDir.isEmpty()) {
        names << QDir(configuredPrivateDir).absoluteFilePath(QStringLiteral("libsmb2.so"));
        names << QDir(configuredPrivateDir).absoluteFilePath(QStringLiteral("libsmb2.so.1"));
        names << QDir(configuredPrivateDir).absoluteFilePath(QStringLiteral("libsmb2.so.0"));
    }
#endif

    const QDir appDir(QCoreApplication::applicationDirPath());
    names << appDir.absoluteFilePath(QStringLiteral("../lib/harbour-sailvideo/libsmb2.so"));
    names << appDir.absoluteFilePath(QStringLiteral("../lib/harbour-sailvideo/libsmb2.so.1"));
    names << appDir.absoluteFilePath(QStringLiteral("../lib/harbour-sailvideo/libsmb2.so.0"));
    names << appDir.absoluteFilePath(QStringLiteral("../lib64/harbour-sailvideo/libsmb2.so"));
    names << appDir.absoluteFilePath(QStringLiteral("../lib64/harbour-sailvideo/libsmb2.so.1"));
    names << appDir.absoluteFilePath(QStringLiteral("../lib64/harbour-sailvideo/libsmb2.so.0"));

    // Fallback to system library resolution. This keeps development packages and
    // distributions with a native libsmb2 package working without a bundled copy.
    names << QStringLiteral("smb2");
    names << QStringLiteral("libsmb2.so");
    names << QStringLiteral("libsmb2.so.1");
    names << QStringLiteral("libsmb2.so.0");

    names.removeDuplicates();
    return names;
}

class LibSmb2
{
public:
    typedef smb2_context *(*InitContextFunc)();
    typedef void (*DestroyContextFunc)(smb2_context *);
    typedef void (*SetUserFunc)(smb2_context *, const char *);
    typedef void (*SetPasswordFunc)(smb2_context *, const char *);
    typedef void (*SetDomainFunc)(smb2_context *, const char *);
    typedef void (*SetAuthenticationFunc)(smb2_context *, int);
    typedef void (*SetSignFunc)(smb2_context *, int);
    typedef int (*ConnectShareFunc)(smb2_context *, const char *, const char *, const char *);
    typedef int (*DisconnectShareFunc)(smb2_context *);
    typedef const char *(*GetErrorFunc)(smb2_context *);
    typedef smb2dir *(*OpenDirFunc)(smb2_context *, const char *);
    typedef void (*CloseDirFunc)(smb2_context *, smb2dir *);
    typedef smb2dirent *(*ReadDirFunc)(smb2_context *, smb2dir *);
    typedef smb2fh *(*OpenFunc)(smb2_context *, const char *, int);
    typedef int (*CloseFunc)(smb2_context *, smb2fh *);
    typedef int (*PReadFunc)(smb2_context *, smb2fh *, uint8_t *, uint32_t, uint64_t);
    typedef int (*StatFunc)(smb2_context *, const char *, smb2_stat_64 *);
    typedef uint32_t (*MaxReadSizeFunc)(smb2_context *);

    bool load(QString *errorString)
    {
        if (m_loaded) {
            return true;
        }

        const QStringList names = privateLibraryCandidates();

        QStringList attempted;
        for (int i = 0; i < names.size(); ++i) {
            const QString name = names.at(i);
            m_library.setFileName(name);
            attempted.append(name);
            if (!m_library.load()) {
                continue;
            }

            if (resolveAll()) {
                m_loaded = true;
                m_loadedFrom = name;
                return true;
            }

            const QString resolveError = m_library.errorString();
            m_library.unload();
            if (errorString) {
                *errorString = QStringLiteral("libsmb2 was found but required symbols are missing: %1")
                        .arg(resolveError);
            }
            return false;
        }

        if (errorString) {
            QString hint;
#if defined(SAILVIDEO_BUILT_WITH_BUNDLED_LIBSMB2) && SAILVIDEO_BUILT_WITH_BUNDLED_LIBSMB2
            hint = QStringLiteral(" This build expected an app-private bundled libsmb2. Check that the RPM contains /usr/lib*/harbour-sailvideo/libsmb2.so* and rebuild from a clean CMake cache if it does not.");
#else
            hint = QStringLiteral(" This build was made without bundled libsmb2. Run tools/import-libsmb2.sh before configuring SailVideo, then rebuild from a clean CMake cache.");
#endif
            *errorString = QStringLiteral("libsmb2 is not available. Tried: %1.%2")
                    .arg(attempted.join(QStringLiteral(", ")), hint);
        }
        return false;
    }

    QString error(smb2_context *ctx) const
    {
        if (!getError || !ctx) {
            return QStringLiteral("Unknown SMB error.");
        }
        const char *text = getError(ctx);
        return text && *text
                ? QString::fromUtf8(text)
                : QStringLiteral("Unknown SMB error.");
    }

    InitContextFunc initContext = nullptr;
    DestroyContextFunc destroyContext = nullptr;
    SetUserFunc setUser = nullptr;
    SetPasswordFunc setPassword = nullptr;
    SetDomainFunc setDomain = nullptr;
    SetAuthenticationFunc setAuthentication = nullptr;
    SetSignFunc setSign = nullptr;
    ConnectShareFunc connectShare = nullptr;
    DisconnectShareFunc disconnectShare = nullptr;
    GetErrorFunc getError = nullptr;
    OpenDirFunc openDir = nullptr;
    CloseDirFunc closeDir = nullptr;
    ReadDirFunc readDir = nullptr;
    OpenFunc open = nullptr;
    CloseFunc close = nullptr;
    PReadFunc pread = nullptr;
    StatFunc stat = nullptr;
    MaxReadSizeFunc maxReadSize = nullptr;

    QString loadedFrom() const
    {
        return m_loadedFrom;
    }

private:
    template<typename T>
    bool resolve(T *target, const char *name)
    {
        *target = reinterpret_cast<T>(m_library.resolve(name));
        return *target != nullptr;
    }

    bool resolveAll()
    {
        // setAuthentication, setSign, smb2_stat and maxReadSize are useful
        // but optional from the loader's point of view so older system libs can
        // still load for directory browsing. File-size lookup reports a clear
        // error if smb2_stat is unavailable.
        setAuthentication = reinterpret_cast<SetAuthenticationFunc>(m_library.resolve("smb2_set_authentication"));
        setSign = reinterpret_cast<SetSignFunc>(m_library.resolve("smb2_set_sign"));
        stat = reinterpret_cast<StatFunc>(m_library.resolve("smb2_stat"));
        maxReadSize = reinterpret_cast<MaxReadSizeFunc>(m_library.resolve("smb2_get_max_read_size"));

        return resolve(&initContext, "smb2_init_context")
                && resolve(&destroyContext, "smb2_destroy_context")
                && resolve(&setUser, "smb2_set_user")
                && resolve(&setPassword, "smb2_set_password")
                && resolve(&setDomain, "smb2_set_domain")
                && resolve(&connectShare, "smb2_connect_share")
                && resolve(&disconnectShare, "smb2_disconnect_share")
                && resolve(&getError, "smb2_get_error")
                && resolve(&openDir, "smb2_opendir")
                && resolve(&closeDir, "smb2_closedir")
                && resolve(&readDir, "smb2_readdir")
                && resolve(&open, "smb2_open")
                && resolve(&close, "smb2_close")
                && resolve(&pread, "smb2_pread");
    }

    QLibrary m_library;
    bool m_loaded = false;
    QString m_loadedFrom;
};

QString credentialUser(const QString &username, bool guest)
{
    const QString cleanUser = username.trimmed();
    if (guest) {
        return QStringLiteral("guest");
    }
    return cleanUser.isEmpty() ? QStringLiteral("guest") : cleanUser;
}

bool configureAndConnect(LibSmb2 *api,
                         smb2_context **ctx,
                         const QString &host,
                         int port,
                         const QString &share,
                         const QString &domain,
                         const QString &username,
                         const QString &password,
                         bool guest,
                         QString *errorString)
{
    if (!api || !ctx) {
        if (errorString) {
            *errorString = QStringLiteral("Internal SMB setup error.");
        }
        return false;
    }

    QString loadError;
    if (!api->load(&loadError)) {
        if (errorString) {
            *errorString = loadError;
        }
        return false;
    }

    smb2_context *context = api->initContext();
    if (!context) {
        if (errorString) {
            *errorString = QStringLiteral("Could not create an SMB context.");
        }
        return false;
    }

    const QByteArray domainBytes = domain.trimmed().toUtf8();
    const QString user = credentialUser(username, guest);
    const QByteArray userBytes = user.toUtf8();
    const QByteArray passwordBytes = guest ? QByteArray() : password.toUtf8();
    const QString server = SmbBackend::serverForLibsmb2(host, port);
    const QByteArray serverBytes = server.toUtf8();
    const QByteArray shareBytes = SmbBackend::normalizedShare(share).toUtf8();

    if (!domainBytes.isEmpty()) {
        api->setDomain(context, domainBytes.constData());
    }
    api->setUser(context, userBytes.constData());
    api->setPassword(context, passwordBytes.constData());

    // Force NTLMSSP when the symbol exists. It avoids Kerberos dependency surprises
    // on small mobile systems and matches username/password NAS usage.
    if (api->setAuthentication) {
        api->setAuthentication(context, 1); // SMB2_SEC_NTLMSSP
    }

    // Request signing support but do not require it; this follows libsmb2's default
    // of working with common home NAS setups.
    if (api->setSign) {
        api->setSign(context, 0);
    }

    const int rc = api->connectShare(context,
                                     serverBytes.constData(),
                                     shareBytes.constData(),
                                     userBytes.constData());
    if (rc != 0) {
        if (errorString) {
            *errorString = QStringLiteral("Could not connect to //%1/%2: %3")
                    .arg(server)
                    .arg(QString::fromUtf8(shareBytes))
                    .arg(api->error(context));
        }
        api->destroyContext(context);
        return false;
    }

    *ctx = context;
    return true;
}

QVariantMap makeEntry(const QString &name,
                      const QString &path,
                      bool directory,
                      qint64 size)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("name"), name);
    entry.insert(QStringLiteral("path"), path);
    entry.insert(QStringLiteral("isDirectory"), directory);
    entry.insert(QStringLiteral("size"), size);
    const bool image = SmbBackend::likelyImageExtension(name);
    entry.insert(QStringLiteral("isVideo"), SmbBackend::likelyVideoExtension(name));
    entry.insert(QStringLiteral("isImage"), image);
    entry.insert(QStringLiteral("subtitle"), directory
                 ? QObject::tr("Folder")
                 : SmbBackend::humanSize(size));
    return entry;
}

} // namespace

struct SmbFileReader::Private
{
    LibSmb2 api;
    smb2_context *context = nullptr;
    smb2fh *handle = nullptr;
    qint64 maxRead = DefaultReadChunk;
    QString errorString;
};

SmbFileReader::SmbFileReader()
    : d(new Private)
{
}

SmbFileReader::~SmbFileReader()
{
    if (d->handle && d->context && d->api.close) {
        d->api.close(d->context, d->handle);
        d->handle = nullptr;
    }
    if (d->context && d->api.disconnectShare) {
        d->api.disconnectShare(d->context);
    }
    if (d->context && d->api.destroyContext) {
        d->api.destroyContext(d->context);
        d->context = nullptr;
    }
}

bool SmbFileReader::isOpen() const
{
    return d->context && d->handle;
}

qint64 SmbFileReader::maxReadSize() const
{
    return d->maxRead;
}

QByteArray SmbFileReader::read(qint64 offset, qint64 count, QString *errorString)
{
    if (!isOpen()) {
        const QString message = QStringLiteral("The SMB file is not open.");
        if (errorString) {
            *errorString = message;
        }
        return QByteArray();
    }

    const qint64 boundedCount = qBound<qint64>(1,
                                               qMin(count, d->maxRead),
                                               MaximumReadChunk);
    QByteArray buffer;
    buffer.resize(static_cast<int>(boundedCount));
    const int rc = d->api.pread(d->context,
                                d->handle,
                                reinterpret_cast<uint8_t *>(buffer.data()),
                                static_cast<uint32_t>(buffer.size()),
                                static_cast<uint64_t>(qMax<qint64>(0, offset)));
    if (rc < 0) {
        d->errorString = d->api.error(d->context);
        if (errorString) {
            *errorString = d->errorString;
        }
        return QByteArray();
    }

    buffer.resize(rc);
    if (errorString) {
        errorString->clear();
    }
    return buffer;
}

QString SmbFileReader::errorString() const
{
    return d->errorString;
}

SmbBackend::SmbBackend(QObject *parent)
    : QObject(parent)
{
    updateBackendStatus();
}

SmbBackend::~SmbBackend()
{
    if (m_listWatcher) {
        m_listWatcher->cancel();
        m_listWatcher->waitForFinished();
        delete m_listWatcher;
        m_listWatcher = nullptr;
    }
}

bool SmbBackend::busy() const
{
    return m_busy;
}

QString SmbBackend::lastError() const
{
    return m_lastError;
}

QString SmbBackend::backendStatus() const
{
    updateBackendStatus();
    return m_backendStatus;
}

bool SmbBackend::backendAvailable() const
{
    QString error;
    LibSmb2 api;
    const bool ok = api.load(&error);
    m_backendStatus = ok
            ? tr("SMB backend available: %1").arg(api.loadedFrom())
            : tr("SMB backend unavailable: %1").arg(error);
    return ok;
}

QString SmbBackend::smbUrlForFile(const QString &host,
                                  int port,
                                  const QString &share,
                                  const QString &path) const
{
    const QString cleanHost = normalizedHost(host);
    const QString cleanShare = normalizedShare(share);
    const QString cleanPath = normalizedPath(path);
    if (cleanHost.isEmpty() || cleanShare.isEmpty()) {
        return QString();
    }

    QUrl url;
    url.setScheme(QStringLiteral("smb"));
    url.setHost(cleanHost);
    if (port > 0 && port != DefaultSmbPort) {
        url.setPort(port);
    }
    url.setPath(QStringLiteral("/") + cleanShare
                + (cleanPath.isEmpty() ? QString() : QStringLiteral("/") + cleanPath));
    return url.toString(QUrl::FullyEncoded);
}

bool SmbBackend::isSmbUrl(const QString &url) const
{
    const QUrl parsed(cleanedText(url));
    const QString scheme = parsed.scheme().toLower();
    return scheme == QLatin1String("smb") || scheme == QLatin1String("smb2");
}

QVariantMap SmbBackend::parseSmbUrl(const QString &url) const
{
    QVariantMap result;
    const QUrl parsed(cleanedText(url));
    const QString scheme = parsed.scheme().toLower();
    if (scheme != QLatin1String("smb") && scheme != QLatin1String("smb2")) {
        result.insert(QStringLiteral("valid"), false);
        return result;
    }

    const QString host = normalizedHost(parsed.host());
    const int port = parsed.port(DefaultSmbPort);
    QString path = parsed.path(QUrl::FullyDecoded);
    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }

    const QStringList parts = path.split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (host.isEmpty() || parts.isEmpty()) {
        result.insert(QStringLiteral("valid"), false);
        return result;
    }

    const QString share = normalizedShare(parts.first());
    QStringList fileParts = parts;
    fileParts.removeFirst();
    const QString filePath = normalizedPath(fileParts.join(QLatin1Char('/')));

    result.insert(QStringLiteral("valid"), !host.isEmpty() && !share.isEmpty());
    result.insert(QStringLiteral("host"), host);
    result.insert(QStringLiteral("port"), port > 0 ? port : DefaultSmbPort);
    result.insert(QStringLiteral("share"), share);
    result.insert(QStringLiteral("path"), filePath);
    result.insert(QStringLiteral("fileName"), fileNameFromPath(filePath));
    return result;
}

bool SmbBackend::isLikelyVideoFile(const QString &fileName) const
{
    return likelyVideoExtension(fileName);
}

bool SmbBackend::isLikelyImageFile(const QString &fileName) const
{
    return likelyImageExtension(fileName);
}

QString SmbBackend::displayPath(const QString &path) const
{
    const QString cleanPath = normalizedPath(path);
    return cleanPath.isEmpty() ? QStringLiteral("/") : QStringLiteral("/") + cleanPath;
}

qint64 SmbBackend::fileSize(const QString &host,
                             int port,
                             const QString &share,
                             const QString &path,
                             const QString &domain,
                             const QString &username,
                             const QString &password,
                             bool guest)
{
    LibSmb2 api;
    smb2_context *context = nullptr;
    QString error;
    if (!configureAndConnect(&api,
                             &context,
                             normalizedHost(host),
                             port > 0 ? port : DefaultSmbPort,
                             normalizedShare(share),
                             cleanedText(domain),
                             cleanedText(username),
                             password,
                             guest,
                             &error)) {
        setLastError(error);
        return -1;
    }

    if (!api.stat) {
        error = tr("The loaded libsmb2 library does not provide smb2_stat, so the SMB file size cannot be resolved.");
        api.disconnectShare(context);
        api.destroyContext(context);
        setLastError(error);
        return -1;
    }

    const QByteArray pathBytes = normalizedPath(path).toUtf8();
    smb2_stat_64 statBuffer;
    memset(&statBuffer, 0, sizeof(statBuffer));

    const int rc = api.stat(context, pathBytes.constData(), &statBuffer);
    if (rc != 0) {
        error = tr("Could not inspect SMB file size: %1")
                .arg(api.error(context));
        api.disconnectShare(context);
        api.destroyContext(context);
        setLastError(error);
        return -1;
    }

    api.disconnectShare(context);
    api.destroyContext(context);

    if (statBuffer.smb2_type == Smb2TypeDirectory) {
        setLastError(tr("The selected SMB item is a folder, not a media file."));
        return -1;
    }

    setLastError(QString());
    return static_cast<qint64>(statBuffer.smb2_size);
}

void SmbBackend::listDirectory(const QString &requestId,
                               const QString &host,
                               int port,
                               const QString &share,
                               const QString &path,
                               const QString &domain,
                               const QString &username,
                               const QString &password,
                               bool guest)
{
    ListRequest request;
    request.requestId = requestId;
    request.host = normalizedHost(host);
    request.port = port > 0 ? port : DefaultSmbPort;
    request.share = normalizedShare(share);
    request.path = normalizedPath(path);
    request.domain = cleanedText(domain);
    request.username = cleanedText(username);
    request.password = password;
    request.guest = guest;

    if (request.host.isEmpty() || request.share.isEmpty()) {
        const QString error = tr("Enter at least a server and share name.");
        setLastError(error);
        emit directoryReady(requestId, QVariantList(), error);
        return;
    }

    if (m_busy) {
        // QML queues user-initiated browse requests. Do not start another
        // libsmb2 operation here: on device, opening a media stream while a
        // queued folder listing is also active can crash the process.
        const QString error = tr("SMB is still working. Please wait and try again.");
        setLastError(error);
        emit directoryReady(requestId, QVariantList(), error);
        return;
    }

    startListRequest(request);
}

void SmbBackend::startListRequest(const ListRequest &request)
{
    setBusy(true);
    setLastError(QString());

    QFutureWatcher<ListResult> *watcher = new QFutureWatcher<ListResult>(this);
    m_listWatcher = watcher;
    connect(watcher, SIGNAL(finished()), this, SLOT(handleListFinished()));
    watcher->setFuture(QtConcurrent::run(&SmbBackend::listDirectorySync, request));
}

std::unique_ptr<SmbFileReader> SmbBackend::openFile(const QString &host,
                                                     int port,
                                                     const QString &share,
                                                     const QString &path,
                                                     const QString &domain,
                                                     const QString &username,
                                                     const QString &password,
                                                     bool guest,
                                                     QString *errorString) const
{
    std::unique_ptr<SmbFileReader> reader(new SmbFileReader);
    QString error;
    if (!configureAndConnect(&reader->d->api,
                             &reader->d->context,
                             normalizedHost(host),
                             port > 0 ? port : DefaultSmbPort,
                             normalizedShare(share),
                             cleanedText(domain),
                             cleanedText(username),
                             password,
                             guest,
                             &error)) {
        if (errorString) {
            *errorString = error;
        }
        return std::unique_ptr<SmbFileReader>();
    }

    const QByteArray pathBytes = normalizedPath(path).toUtf8();
    reader->d->handle = reader->d->api.open(reader->d->context,
                                            pathBytes.constData(),
                                            O_RDONLY);
    if (!reader->d->handle) {
        error = tr("Could not open the SMB file: %1")
                .arg(reader->d->api.error(reader->d->context));
        if (errorString) {
            *errorString = error;
        }
        return std::unique_ptr<SmbFileReader>();
    }

    if (reader->d->api.maxReadSize) {
        const qint64 serverMaxRead = reader->d->api.maxReadSize(reader->d->context);
        if (serverMaxRead > 0) {
            reader->d->maxRead = qBound(MinimumReadChunk, serverMaxRead, MaximumReadChunk);
        }
    }

    if (errorString) {
        errorString->clear();
    }
    return reader;
}

void SmbBackend::handleListFinished()
{
    QFutureWatcher<ListResult> *watcher = static_cast<QFutureWatcher<ListResult> *>(sender());
    if (!watcher) {
        return;
    }

    const ListResult result = watcher->result();
    watcher->deleteLater();
    if (m_listWatcher == watcher) {
        m_listWatcher = nullptr;
    }

    setBusy(false);
    setLastError(result.errorString);
    emit directoryReady(result.requestId, result.entries, result.errorString);
}

QString SmbBackend::cleanedText(const QString &value)
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

QString SmbBackend::normalizedHost(const QString &host)
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

QString SmbBackend::normalizedShare(const QString &share)
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

QString SmbBackend::normalizedPath(const QString &path)
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

QString SmbBackend::joinPath(const QString &basePath, const QString &childName)
{
    const QString base = normalizedPath(basePath);
    const QString child = cleanedText(childName);
    if (base.isEmpty()) {
        return normalizedPath(child);
    }
    return normalizedPath(base + QLatin1Char('/') + child);
}

QString SmbBackend::fileNameFromPath(const QString &path)
{
    const QString cleanPath = normalizedPath(path);
    const int slash = cleanPath.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? cleanPath.mid(slash + 1) : cleanPath;
}

QString SmbBackend::humanSize(qint64 bytes)
{
    if (bytes < 0) {
        return QString();
    }
    if (bytes < 1024) {
        return QObject::tr("%1 B").arg(bytes);
    }

    double value = bytes / 1024.0;
    const char *units[] = { "KiB", "MiB", "GiB", "TiB" };
    for (int i = 0; i < 4; ++i) {
        if (value < 1024.0 || i == 3) {
            return QStringLiteral("%1 %2")
                    .arg(value, 0, 'f', value < 10.0 ? 1 : 0)
                    .arg(QString::fromLatin1(units[i]));
        }
        value /= 1024.0;
    }
    return QString::number(bytes);
}

QString SmbBackend::serverForLibsmb2(const QString &host, int port)
{
    const QString cleanHost = normalizedHost(host);
    const int cleanPort = port > 0 ? port : DefaultSmbPort;
    if (cleanPort == DefaultSmbPort) {
        return cleanHost;
    }
    return QStringLiteral("%1:%2").arg(cleanHost).arg(cleanPort);
}

bool SmbBackend::likelyVideoExtension(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    return suffix == QLatin1String("mp4")
            || suffix == QLatin1String("m4v")
            || suffix == QLatin1String("mkv")
            || suffix == QLatin1String("avi")
            || suffix == QLatin1String("mov")
            || suffix == QLatin1String("webm")
            || suffix == QLatin1String("mpg")
            || suffix == QLatin1String("mpeg")
            || suffix == QLatin1String("ts")
            || suffix == QLatin1String("m2ts")
            || suffix == QLatin1String("flv")
            || suffix == QLatin1String("wmv")
            || suffix == QLatin1String("3gp")
            || suffix == QLatin1String("3g2")
            || suffix == QLatin1String("ogv")
            || suffix == QLatin1String("mts")
            || suffix == QLatin1String("m2v")
            || suffix == QLatin1String("vob")
            || suffix == QLatin1String("divx")
            || suffix == QLatin1String("asf")
            || suffix == QLatin1String("rm")
            || suffix == QLatin1String("rmvb");
}

bool SmbBackend::likelyImageExtension(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    return suffix == QLatin1String("jpg")
            || suffix == QLatin1String("jpeg")
            || suffix == QLatin1String("png")
            || suffix == QLatin1String("gif")
            || suffix == QLatin1String("bmp")
            || suffix == QLatin1String("webp")
            || suffix == QLatin1String("tif")
            || suffix == QLatin1String("tiff")
            || suffix == QLatin1String("jpe")
            || suffix == QLatin1String("jfif")
            || suffix == QLatin1String("svg")
            || suffix == QLatin1String("heic")
            || suffix == QLatin1String("heif");
}

SmbBackend::ListResult SmbBackend::listDirectorySync(const ListRequest &request)
{
    ListResult result;
    result.requestId = request.requestId;

    LibSmb2 api;
    smb2_context *context = nullptr;
    QString error;
    if (!configureAndConnect(&api,
                             &context,
                             request.host,
                             request.port,
                             request.share,
                             request.domain,
                             request.username,
                             request.password,
                             request.guest,
                             &error)) {
        result.errorString = error;
        return result;
    }

    const QByteArray pathBytes = normalizedPath(request.path).toUtf8();
    smb2dir *dir = api.openDir(context, pathBytes.constData());
    if (!dir) {
        const QString cleanPath = normalizedPath(request.path);
        result.errorString = QObject::tr("Could not open SMB folder %1: %2")
                .arg(cleanPath.isEmpty() ? QStringLiteral("/") : QStringLiteral("/") + cleanPath)
                .arg(api.error(context));
        api.disconnectShare(context);
        api.destroyContext(context);
        return result;
    }

    QList<QVariantMap> directories;
    QList<QVariantMap> files;

    for (;;) {
        smb2dirent *entry = api.readDir(context, dir);
        if (!entry) {
            break;
        }

        const QString name = entry->name ? QString::fromUtf8(entry->name) : QString();
        if (name.isEmpty()
                || name == QLatin1String(".")
                || name == QLatin1String("..")) {
            continue;
        }

        const bool isDirectory = entry->st.smb2_type == Smb2TypeDirectory;
        const qint64 size = static_cast<qint64>(entry->st.smb2_size);
        QVariantMap item = makeEntry(name, joinPath(request.path, name), isDirectory, size);
        if (isDirectory) {
            directories.append(item);
        } else {
            files.append(item);
        }
    }

    api.closeDir(context, dir);
    api.disconnectShare(context);
    api.destroyContext(context);

    const auto byName = [](const QVariantMap &a, const QVariantMap &b) {
        return QString::localeAwareCompare(a.value(QStringLiteral("name")).toString(),
                                           b.value(QStringLiteral("name")).toString()) < 0;
    };
    std::sort(directories.begin(), directories.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    for (int i = 0; i < directories.size(); ++i) {
        result.entries.append(directories.at(i));
    }
    for (int i = 0; i < files.size(); ++i) {
        result.entries.append(files.at(i));
    }

    return result;
}

void SmbBackend::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void SmbBackend::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}

void SmbBackend::updateBackendStatus() const
{
    QString error;
    LibSmb2 api;
    const bool ok = api.load(&error);
    const QString text = ok
            ? tr("SMB backend available: %1").arg(api.loadedFrom())
            : tr("SMB backend unavailable: %1").arg(error);
    if (m_backendStatus != text) {
        m_backendStatus = text;
        emit const_cast<SmbBackend *>(this)->backendStatusChanged();
    }
}
