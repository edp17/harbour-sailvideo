/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "localfilestreamserver.h"

#include "smb/smbbackend.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QMimeDatabase>
#include <QMimeType>
#include <QTcpSocket>
#include <QUrl>
#include <QUuid>

namespace {

const qint64 ChunkSize = 128 * 1024;
const qint64 MaxBufferedBytes = 512 * 1024;

QByteArray reasonPhrase(int statusCode)
{
    switch (statusCode) {
    case 200:
        return QByteArrayLiteral("OK");
    case 206:
        return QByteArrayLiteral("Partial Content");
    case 400:
        return QByteArrayLiteral("Bad Request");
    case 404:
        return QByteArrayLiteral("Not Found");
    case 405:
        return QByteArrayLiteral("Method Not Allowed");
    case 416:
        return QByteArrayLiteral("Range Not Satisfiable");
    case 500:
        return QByteArrayLiteral("Internal Server Error");
    default:
        return QByteArrayLiteral("Error");
    }
}

QByteArray headerValue(const QList<QByteArray> &lines, const QByteArray &headerName)
{
    const QByteArray expected = headerName.toLower() + QByteArrayLiteral(":");
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.toLower().startsWith(expected)) {
            return line.mid(expected.size()).trimmed();
        }
    }
    return QByteArray();
}

QString percentDecodedPathPart(const QByteArray &part)
{
    return QUrl::fromPercentEncoding(part);
}

QString cleanedText(const QString &value)
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

QString normalizedSmbPath(const QString &path)
{
    QString text = cleanedText(path);
    text.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (text.startsWith(QLatin1Char('/'))) {
        text.remove(0, 1);
    }
    while (text.endsWith(QLatin1Char('/'))) {
        text.chop(1);
    }
    return text;
}

QString fileNameFromSmbPath(const QString &path)
{
    const QString cleanPath = normalizedSmbPath(path);
    const int slash = cleanPath.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? cleanPath.mid(slash + 1) : cleanPath;
}

QString mimeTypeForName(const QString &fileName)
{
    QMimeDatabase mimeDatabase;
    const QMimeType mimeType = mimeDatabase.mimeTypeForFile(fileName,
                                                            QMimeDatabase::MatchExtension);
    return mimeType.isValid()
            ? mimeType.name()
            : QStringLiteral("application/octet-stream");
}

} // namespace

LocalFileStreamServer::LocalFileStreamServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, SIGNAL(newConnection()),
            this, SLOT(handleNewConnection()));
}

LocalFileStreamServer::~LocalFileStreamServer()
{
    clear();
}

void LocalFileStreamServer::setSmbBackend(SmbBackend *backend)
{
    m_smbBackend = backend;
}

bool LocalFileStreamServer::running() const
{
    return m_server.isListening();
}

QString LocalFileStreamServer::lastError() const
{
    return m_lastError;
}

qint64 LocalFileStreamServer::lastResolvedSize() const
{
    return m_lastResolvedSize;
}

QString LocalFileStreamServer::streamUrlForLocalFile(const QString &urlOrPath)
{
    const QString path = localFilePath(urlOrPath);
    setLastResolvedSize(0);

    if (path.isEmpty()) {
        setLastError(tr("Only local files can be served by the test stream."));
        return QString();
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        setLastError(tr("The selected local file is not readable."));
        return QString();
    }

    if (!ensureListening()) {
        return QString();
    }

    QMimeDatabase mimeDatabase;
    const QMimeType mimeType = mimeDatabase.mimeTypeForFile(fileInfo);

    StreamEntry entry;
    entry.type = StreamType::LocalFile;
    entry.filePath = fileInfo.absoluteFilePath();
    entry.fileName = fileInfo.fileName();
    entry.mimeType = mimeType.isValid()
            ? mimeType.name()
            : QStringLiteral("application/octet-stream");
    entry.size = fileInfo.size();
    setLastResolvedSize(entry.size);

    const QString token = createToken();
    m_entries.insert(token, entry);

    setLastError(QString());

    const QString encodedFileName = QString::fromLatin1(
                QUrl::toPercentEncoding(entry.fileName));
    return QStringLiteral("http://127.0.0.1:%1/%2/%3")
            .arg(m_server.serverPort())
            .arg(token)
            .arg(encodedFileName);
}

QString LocalFileStreamServer::streamUrlForSmbFile(const QString &host,
                                                   int port,
                                                   const QString &share,
                                                   const QString &path,
                                                   const QString &domain,
                                                   const QString &username,
                                                   const QString &password,
                                                   bool guest,
                                                   qint64 size,
                                                   const QString &fileName)
{
    setLastResolvedSize(0);

    if (!m_smbBackend) {
        setLastError(tr("The SMB backend is not connected to the stream server."));
        return QString();
    }

    if (!m_smbBackend->backendAvailable()) {
        setLastError(m_smbBackend->backendStatus());
        return QString();
    }

    const QString cleanHost = cleanedText(host);
    const QString cleanShare = cleanedText(share);
    const QString cleanPath = normalizedSmbPath(path);
    if (cleanHost.isEmpty() || cleanShare.isEmpty() || cleanPath.isEmpty()) {
        setLastError(tr("The SMB file location is incomplete."));
        return QString();
    }

    qint64 resolvedSize = size;
    if (resolvedSize <= 0) {
        resolvedSize = m_smbBackend->fileSize(cleanHost,
                                              port > 0 ? port : 445,
                                              cleanShare,
                                              cleanPath,
                                              domain,
                                              username,
                                              password,
                                              guest);
    }

    if (resolvedSize <= 0) {
        const QString backendError = m_smbBackend->lastError();
        setLastError(backendError.isEmpty()
                     ? tr("The SMB file size is unknown, so it cannot be streamed seekably yet.")
                     : backendError);
        return QString();
    }

    if (!ensureListening()) {
        return QString();
    }

    StreamEntry entry;
    entry.type = StreamType::SmbFile;
    entry.fileName = cleanedText(fileName);
    if (entry.fileName.isEmpty()) {
        entry.fileName = fileNameFromSmbPath(cleanPath);
    }
    entry.mimeType = mimeTypeForName(entry.fileName);
    entry.size = resolvedSize;
    setLastResolvedSize(entry.size);
    entry.smbHost = cleanHost;
    entry.smbPort = port > 0 ? port : 445;
    entry.smbShare = cleanShare;
    entry.smbPath = cleanPath;
    entry.smbDomain = cleanedText(domain);
    entry.smbUsername = cleanedText(username);
    entry.smbPassword = password;
    entry.smbGuest = guest;

    const QString token = createToken();
    m_entries.insert(token, entry);
    setLastError(QString());

    const QString encodedFileName = QString::fromLatin1(
                QUrl::toPercentEncoding(entry.fileName));
    return QStringLiteral("http://127.0.0.1:%1/%2/%3")
            .arg(m_server.serverPort())
            .arg(token)
            .arg(encodedFileName);
}

bool LocalFileStreamServer::isLocalFile(const QString &urlOrPath) const
{
    return !localFilePath(urlOrPath).isEmpty();
}

void LocalFileStreamServer::clear()
{
    QList<QTcpSocket *> sockets = m_pendingRequests.keys();
    sockets.append(m_transfers.keys());
    for (int i = 0; i < sockets.size(); ++i) {
        if (sockets.at(i)) {
            sockets.at(i)->disconnectFromHost();
            closeTransfer(sockets.at(i));
            sockets.at(i)->deleteLater();
        }
    }

    m_pendingRequests.clear();
    m_entries.clear();
    if (m_server.isListening()) {
        m_server.close();
        emit runningChanged();
    }
}

void LocalFileStreamServer::handleNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        if (!socket) {
            continue;
        }

        connect(socket, SIGNAL(readyRead()),
                this, SLOT(readClientRequest()));
        connect(socket, SIGNAL(bytesWritten(qint64)),
                this, SLOT(pumpClientData()));
        connect(socket, SIGNAL(disconnected()),
                this, SLOT(cleanupClient()));
        connect(socket, SIGNAL(destroyed(QObject*)),
                this, SLOT(cleanupClient()));
        m_pendingRequests.insert(socket, QByteArray());
    }
}

void LocalFileStreamServer::readClientRequest()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || m_transfers.contains(socket)) {
        return;
    }

    QByteArray request = m_pendingRequests.value(socket);
    request.append(socket->readAll());

    const int headerEnd = request.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        if (request.size() > 65536) {
            sendSimpleResponse(socket, 400, reasonPhrase(400));
        } else {
            m_pendingRequests.insert(socket, request);
        }
        return;
    }

    m_pendingRequests.remove(socket);
    handleRequest(socket, request.left(headerEnd + 4));
}

void LocalFileStreamServer::pumpClientData()
{
    pumpClientData(qobject_cast<QTcpSocket *>(sender()));
}

void LocalFileStreamServer::pumpClientData(QTcpSocket *socket)
{
    if (!socket) {
        return;
    }

    Transfer *transfer = m_transfers.value(socket, nullptr);
    if (!transfer) {
        return;
    }

    while (transfer->remaining > 0 && socket->bytesToWrite() < MaxBufferedBytes) {
        const qint64 wanted = qMin(ChunkSize, transfer->remaining);
        QByteArray data;

        if (transfer->file) {
            data = transfer->file->read(wanted);
        } else if (transfer->smbReader) {
            QString error;
            data = transfer->smbReader->read(transfer->nextOffset, wanted, &error);
            if (data.isEmpty() && !error.isEmpty()) {
                setLastError(tr("SMB stream read failed: %1").arg(error));
                break;
            }
        }

        if (data.isEmpty()) {
            break;
        }

        const qint64 written = socket->write(data);
        if (written <= 0) {
            break;
        }

        transfer->remaining -= written;
        transfer->nextOffset += written;
        if (written < data.size()) {
            if (transfer->file) {
                transfer->file->seek(transfer->file->pos() - (data.size() - written));
            }
            transfer->nextOffset -= (data.size() - written);
            break;
        }
    }

    if (transfer->remaining <= 0) {
        closeTransfer(socket);
        socket->disconnectFromHost();
    }
}

void LocalFileStreamServer::cleanupClient()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    m_pendingRequests.remove(socket);
    closeTransfer(socket);
    socket->deleteLater();
}

bool LocalFileStreamServer::ensureListening()
{
    if (m_server.isListening()) {
        return true;
    }

    if (!m_server.listen(QHostAddress::LocalHost, 0)) {
        setLastError(tr("Could not start the local HTTP stream: %1")
                     .arg(m_server.errorString()));
        return false;
    }

    emit runningChanged();
    return true;
}

QString LocalFileStreamServer::localFilePath(const QString &urlOrPath) const
{
    QString text = urlOrPath.trimmed();
    for (int i = 0; i < 3; ++i) {
        if (text.size() >= 2
                && ((text.startsWith(QLatin1Char('\'')) && text.endsWith(QLatin1Char('\'')))
                    || (text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"'))))) {
            text = text.mid(1, text.size() - 2).trimmed();
        } else {
            break;
        }
    }

    if (text.isEmpty()) {
        return QString();
    }

    const QUrl url(text);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }

    if (url.scheme().isEmpty() && QFileInfo(text).isAbsolute()) {
        return text;
    }

    return QString();
}

QString LocalFileStreamServer::createToken() const
{
    const QByteArray uuid = QUuid::createUuid().toByteArray();
    const QByteArray now = QByteArray::number(QDateTime::currentMSecsSinceEpoch());
    return QString::fromLatin1(QCryptographicHash::hash(uuid + now,
                                                        QCryptographicHash::Sha256)
                               .toHex().left(32));
}

void LocalFileStreamServer::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

void LocalFileStreamServer::handleRequest(QTcpSocket *socket, const QByteArray &request)
{
    const QList<QByteArray> lines = request.split('\n');
    if (lines.isEmpty()) {
        sendSimpleResponse(socket, 400, reasonPhrase(400));
        return;
    }

    const QList<QByteArray> requestParts = lines.first().trimmed().split(' ');
    if (requestParts.size() < 3) {
        sendSimpleResponse(socket, 400, reasonPhrase(400));
        return;
    }

    const QByteArray method = requestParts.at(0).trimmed().toUpper();
    if (method != QByteArrayLiteral("GET") && method != QByteArrayLiteral("HEAD")) {
        sendSimpleResponse(socket, 405, reasonPhrase(405));
        return;
    }

    QByteArray target = requestParts.at(1).trimmed();
    const int queryIndex = target.indexOf('?');
    if (queryIndex >= 0) {
        target = target.left(queryIndex);
    }

    if (target.startsWith('/')) {
        target.remove(0, 1);
    }

    const int slashIndex = target.indexOf('/');
    const QByteArray tokenBytes = slashIndex >= 0 ? target.left(slashIndex) : target;
    const QString token = percentDecodedPathPart(tokenBytes);
    if (token.isEmpty() || !m_entries.contains(token)) {
        sendSimpleResponse(socket, 404, reasonPhrase(404));
        return;
    }

    const StreamEntry entry = m_entries.value(token);
    if (entry.type == StreamType::LocalFile && !QFileInfo(entry.filePath).isReadable()) {
        sendSimpleResponse(socket, 404, reasonPhrase(404));
        return;
    }

    if (entry.type == StreamType::SmbFile && !m_smbBackend) {
        sendSimpleResponse(socket, 500, reasonPhrase(500));
        return;
    }

    qint64 start = 0;
    qint64 end = qMax<qint64>(0, entry.size - 1);
    bool partial = false;

    const QByteArray rangeHeader = headerValue(lines, QByteArrayLiteral("Range"));
    if (!rangeHeader.isEmpty()) {
        if (!parseRange(rangeHeader, entry.size, &start, &end)) {
            sendRangeNotSatisfiable(socket, entry.size);
            return;
        }
        partial = true;
    }

    startTransfer(socket, entry, start, end, partial, method == QByteArrayLiteral("HEAD"));
}

void LocalFileStreamServer::sendSimpleResponse(QTcpSocket *socket,
                                               int statusCode,
                                               const QByteArray &reason,
                                               const QByteArray &body)
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + reason + "\r\n";
    response += "Connection: close\r\n";
    response += "Content-Type: text/plain; charset=utf-8\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "\r\n";
    response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

void LocalFileStreamServer::sendRangeNotSatisfiable(QTcpSocket *socket, qint64 size)
{
    QByteArray response;
    response += "HTTP/1.1 416 Range Not Satisfiable\r\n";
    response += "Accept-Ranges: bytes\r\n";
    response += "Content-Range: bytes */" + QByteArray::number(size) + "\r\n";
    response += "Content-Length: 0\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    socket->write(response);
    socket->disconnectFromHost();
}

bool LocalFileStreamServer::parseRange(const QByteArray &rangeHeader,
                                       qint64 size,
                                       qint64 *start,
                                       qint64 *end) const
{
    if (!start || !end || size < 0) {
        return false;
    }

    const QByteArray prefix = QByteArrayLiteral("bytes=");
    const QByteArray value = rangeHeader.trimmed();
    if (!value.toLower().startsWith(prefix)) {
        return false;
    }

    QByteArray range = value.mid(prefix.size()).trimmed();
    if (range.contains(',')) {
        range = range.left(range.indexOf(','));
    }

    const int dash = range.indexOf('-');
    if (dash < 0) {
        return false;
    }

    const QByteArray first = range.left(dash).trimmed();
    const QByteArray last = range.mid(dash + 1).trimmed();
    bool ok = false;

    if (first.isEmpty()) {
        const qint64 suffixLength = last.toLongLong(&ok);
        if (!ok || suffixLength <= 0) {
            return false;
        }
        *start = qMax<qint64>(0, size - suffixLength);
        *end = qMax<qint64>(0, size - 1);
    } else {
        const qint64 parsedStart = first.toLongLong(&ok);
        if (!ok || parsedStart < 0) {
            return false;
        }
        *start = parsedStart;

        if (last.isEmpty()) {
            *end = qMax<qint64>(0, size - 1);
        } else {
            const qint64 parsedEnd = last.toLongLong(&ok);
            if (!ok || parsedEnd < parsedStart) {
                return false;
            }
            *end = qMin(parsedEnd, qMax<qint64>(0, size - 1));
        }
    }

    if (size == 0) {
        return *start == 0 && *end == 0;
    }

    return *start >= 0 && *start < size && *end >= *start;
}

void LocalFileStreamServer::startTransfer(QTcpSocket *socket,
                                          const StreamEntry &entry,
                                          qint64 start,
                                          qint64 end,
                                          bool partial,
                                          bool headOnly)
{
    const qint64 contentLength = entry.size > 0 ? end - start + 1 : 0;
    const int statusCode = partial ? 206 : 200;

    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " "
            + reasonPhrase(statusCode) + "\r\n";
    response += "Accept-Ranges: bytes\r\n";
    response += "Content-Type: " + entry.mimeType.toUtf8() + "\r\n";
    response += "Content-Length: " + QByteArray::number(contentLength) + "\r\n";
    if (partial) {
        response += "Content-Range: bytes " + QByteArray::number(start)
                + "-" + QByteArray::number(end)
                + "/" + QByteArray::number(entry.size) + "\r\n";
    }
    response += "Cache-Control: no-store\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    socket->write(response);

    if (headOnly || contentLength <= 0) {
        socket->disconnectFromHost();
        return;
    }

    Transfer *transfer = new Transfer;
    transfer->remaining = contentLength;
    transfer->nextOffset = start;

    if (entry.type == StreamType::LocalFile) {
        QFile *file = new QFile(entry.filePath, this);
        if (!file->open(QIODevice::ReadOnly) || !file->seek(start)) {
            delete file;
            delete transfer;
            socket->disconnectFromHost();
            return;
        }
        transfer->file = file;
    } else {
        QString error;
        transfer->smbReader = m_smbBackend->openFile(entry.smbHost,
                                                     entry.smbPort,
                                                     entry.smbShare,
                                                     entry.smbPath,
                                                     entry.smbDomain,
                                                     entry.smbUsername,
                                                     entry.smbPassword,
                                                     entry.smbGuest,
                                                     &error);
        if (!transfer->smbReader || !transfer->smbReader->isOpen()) {
            setLastError(error.isEmpty() ? tr("Could not open SMB file stream.") : error);
            delete transfer;
            socket->disconnectFromHost();
            return;
        }
    }

    m_transfers.insert(socket, transfer);
    pumpClientData(socket);
}

void LocalFileStreamServer::closeTransfer(QTcpSocket *socket)
{
    Transfer *transfer = m_transfers.take(socket);
    if (!transfer) {
        return;
    }

    if (transfer->file) {
        transfer->file->close();
        transfer->file->deleteLater();
    }
    transfer->smbReader.reset();
    delete transfer;
}

void LocalFileStreamServer::setLastResolvedSize(qint64 size)
{
    const qint64 cleanSize = qMax<qint64>(0, size);
    if (m_lastResolvedSize == cleanSize) {
        return;
    }

    m_lastResolvedSize = cleanSize;
    emit lastResolvedSizeChanged();
}
