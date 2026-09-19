/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 *
 * Cast V2 uses a small protobuf envelope around JSON namespace payloads.
 * SailVideo implements only the envelope fields needed by the sender protocol,
 * avoiding an external protobuf dependency.
 */

#include "castmanager.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUrl>
#include <QtGlobal>

namespace {

const char *SourceId = "sender-0";
const char *ReceiverId = "receiver-0";
const char *DefaultMediaReceiver = "CC1AD845";

const char *NsConnection = "urn:x-cast:com.google.cast.tp.connection";
const char *NsHeartbeat = "urn:x-cast:com.google.cast.tp.heartbeat";
const int MaxReconnectAttempts = 3;
const char *NsReceiver = "urn:x-cast:com.google.cast.receiver";
const char *NsMedia = "urn:x-cast:com.google.cast.media";

void appendVarint(QByteArray *output, quint64 value)
{
    if (!output) {
        return;
    }
    while (value >= 0x80) {
        output->append(char((value & 0x7f) | 0x80));
        value >>= 7;
    }
    output->append(char(value & 0x7f));
}

void appendKey(QByteArray *output, int fieldNumber, int wireType)
{
    appendVarint(output, quint64((fieldNumber << 3) | wireType));
}

void appendVarintField(QByteArray *output, int fieldNumber, quint64 value)
{
    appendKey(output, fieldNumber, 0);
    appendVarint(output, value);
}

void appendStringField(QByteArray *output, int fieldNumber, const QByteArray &value)
{
    appendKey(output, fieldNumber, 2);
    appendVarint(output, quint64(value.size()));
    output->append(value);
}

bool readVarint(const QByteArray &data, int *offset, quint64 *value)
{
    if (!offset || !value) {
        return false;
    }

    quint64 result = 0;
    int shift = 0;
    int pos = *offset;
    while (pos < data.size() && shift <= 63) {
        const quint8 byte = quint8(data.at(pos++));
        result |= quint64(byte & 0x7f) << shift;
        if (!(byte & 0x80)) {
            *offset = pos;
            *value = result;
            return true;
        }
        shift += 7;
    }
    return false;
}

bool skipField(const QByteArray &data, int wireType, int *offset)
{
    if (!offset) {
        return false;
    }

    if (wireType == 0) {
        quint64 ignored = 0;
        return readVarint(data, offset, &ignored);
    }
    if (wireType == 1) {
        if (*offset + 8 > data.size()) {
            return false;
        }
        *offset += 8;
        return true;
    }
    if (wireType == 2) {
        quint64 length = 0;
        if (!readVarint(data, offset, &length)
                || length > quint64(data.size() - *offset)) {
            return false;
        }
        *offset += int(length);
        return true;
    }
    if (wireType == 5) {
        if (*offset + 4 > data.size()) {
            return false;
        }
        *offset += 4;
        return true;
    }
    return false;
}

quint32 frameLength(const QByteArray &buffer)
{
    if (buffer.size() < 4) {
        return 0;
    }
    return (quint32(quint8(buffer.at(0))) << 24)
            | (quint32(quint8(buffer.at(1))) << 16)
            | (quint32(quint8(buffer.at(2))) << 8)
            | quint32(quint8(buffer.at(3)));
}

QByteArray frameMessage(const QByteArray &payload)
{
    QByteArray frame;
    const quint32 size = quint32(payload.size());
    frame.append(char((size >> 24) & 0xff));
    frame.append(char((size >> 16) & 0xff));
    frame.append(char((size >> 8) & 0xff));
    frame.append(char(size & 0xff));
    frame.append(payload);
    return frame;
}

} // namespace

CastManager::CastManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, SIGNAL(encrypted()),
            this, SLOT(handleEncrypted()));
    connect(&m_socket, SIGNAL(readyRead()),
            this, SLOT(handleReadyRead()));
    connect(&m_socket, SIGNAL(disconnected()),
            this, SLOT(handleDisconnected()));
    connect(&m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handleSocketError(QAbstractSocket::SocketError)));
    connect(&m_socket, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(handleSslErrors(QList<QSslError>)));

    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, SIGNAL(timeout()),
            this, SLOT(pollStatus()));

    m_disconnectTimer.setInterval(6000);
    m_disconnectTimer.setSingleShot(true);
    connect(&m_disconnectTimer, SIGNAL(timeout()),
            this, SLOT(handleDisconnectTimeout()));

    m_reconnectTimer.setInterval(1000);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, SIGNAL(timeout()),
            this, SLOT(attemptReconnect()));
}

CastManager::~CastManager()
{
    // Do not send receiver STOP merely because SailVideo exits. For direct web
    // media this lets the receiver continue. Bridged local/NAS media still
    // depends on the SailVideo process remaining alive to serve HTTP ranges.
    detach();
}

bool CastManager::connected() const { return m_connected; }
bool CastManager::casting() const { return m_casting; }
bool CastManager::playing() const { return m_playing; }
bool CastManager::disconnecting() const { return m_disconnecting; }
bool CastManager::mediaStopped() const { return m_mediaStopped; }
bool CastManager::rejoining() const { return m_rejoining; }
bool CastManager::imageMedia() const
{
    return m_activeContentType.trimmed().toLower().startsWith(QStringLiteral("image/"));
}

bool CastManager::mediaInfoKnown() const
{
    return !m_activeContentType.trimmed().isEmpty();
}
QString CastManager::deviceName() const { return m_deviceName; }
QString CastManager::host() const { return m_host; }
int CastManager::port() const { return m_port; }
QString CastManager::playerState() const { return m_playerState; }
qint64 CastManager::position() const { return m_position; }
qint64 CastManager::duration() const { return m_duration; }
int CastManager::volumePercent() const { return m_volumePercent; }
bool CastManager::muted() const { return m_muted; }
QString CastManager::statusText() const { return m_statusText; }
QString CastManager::lastError() const { return m_lastError; }

bool CastManager::configurePendingMedia(const QString &mediaUrl,
                                        const QString &contentType,
                                        const QString &title,
                                        qint64 startPositionMs)
{
    const QString cleanUrl = mediaUrl.trimmed();
    const QUrl url(cleanUrl);
    if (cleanUrl.isEmpty()
            || !url.isValid()
            || (url.scheme().toLower() != QStringLiteral("http")
                && url.scheme().toLower() != QStringLiteral("https"))) {
        setLastError(tr("Chromecast needs an HTTP or HTTPS media URL."));
        return false;
    }

    m_pendingMediaUrl = cleanUrl;
    m_pendingContentType = contentType.trimmed().isEmpty()
            ? contentTypeForUrl(cleanUrl, title)
            : contentType.trimmed();
    m_pendingTitle = title.trimmed();
    m_pendingStartPosition = qMax<qint64>(0, startPositionMs);
    setPosition(m_pendingStartPosition);
    return true;
}

bool CastManager::startCasting(const QString &deviceName,
                               const QString &host,
                               int port,
                               const QString &mediaUrl,
                               const QString &contentType,
                               const QString &title,
                               qint64 startPositionMs,
                               int initialVolumePercent)
{
    const QString cleanHost = host.trimmed();
    if (cleanHost.isEmpty()) {
        setLastError(tr("Chromecast device address is missing."));
        return false;
    }

    detach();
    resetForNewRequest();
    setRejoining(false);
    m_initialVolumePercent = initialVolumePercent < 0
            ? -1
            : qBound(0, initialVolumePercent, 100);

    m_deviceName = deviceName.trimmed().isEmpty() ? tr("Chromecast") : deviceName.trimmed();
    m_host = cleanHost;
    m_port = port > 0 ? port : 8009;
    emit deviceChanged();

    if (!configurePendingMedia(mediaUrl, contentType, title, startPositionMs)) {
        return false;
    }

    setLastError(QString());
    setStatusText(tr("Connecting to %1").arg(m_deviceName));

    qInfo() << "SailVideo Cast: TLS connecting to"
            << m_deviceName << m_host << m_port
            << "startMs=" << m_pendingStartPosition
            << "initialVolume=" << m_initialVolumePercent
            << "contentType=" << m_pendingContentType;

    m_socket.connectToHostEncrypted(m_host, quint16(m_port));
    return true;
}

bool CastManager::rejoin(const QString &deviceName,
                         const QString &host,
                         int port)
{
    const QString cleanHost = host.trimmed();
    if (cleanHost.isEmpty()) {
        setLastError(tr("Chromecast device address is missing."));
        return false;
    }

    detach();
    resetForNewRequest();
    m_initialVolumePercent = -1;

    m_deviceName = deviceName.trimmed().isEmpty() ? tr("Chromecast") : deviceName.trimmed();
    m_host = cleanHost;
    m_port = port > 0 ? port : 8009;
    emit deviceChanged();

    m_pendingMediaUrl.clear();
    m_pendingContentType.clear();
    m_pendingTitle.clear();
    m_pendingStartPosition = 0;

    setRejoining(true);
    setLastError(QString());
    setStatusText(tr("Reconnecting to %1").arg(m_deviceName));

    qInfo() << "SailVideo Cast: rejoining receiver on"
            << m_deviceName << m_host << m_port;
    m_socket.connectToHostEncrypted(m_host, quint16(m_port));
    return true;
}

bool CastManager::replaceMedia(const QString &mediaUrl,
                               const QString &contentType,
                               const QString &title,
                               qint64 startPositionMs)
{
    if (!m_connected || m_transportId.isEmpty()) {
        setLastError(tr("Chromecast is not connected."));
        return false;
    }

    if (!configurePendingMedia(mediaUrl, contentType, title, startPositionMs)) {
        return false;
    }

    m_initialVolumePercent = -1;
    m_mediaSessionId = 0;
    m_loadSent = false;
    m_finishedEmitted = false;
    m_stopRequested = false;
    m_stoppedPosition = 0;
    setMediaStopped(false);
    setLastError(QString());

    qInfo() << "SailVideo Cast: replacing media"
            << "startMs=" << m_pendingStartPosition
            << "contentType=" << m_pendingContentType;
    sendLoad();
    return true;
}

bool CastManager::replaceMediaWithVolume(const QString &mediaUrl,
                                         const QString &contentType,
                                         const QString &title,
                                         qint64 startPositionMs,
                                         int initialVolumePercent)
{
    if (!m_connected || m_transportId.isEmpty()) {
        setLastError(tr("Chromecast is not connected."));
        return false;
    }

    if (!configurePendingMedia(mediaUrl, contentType, title, startPositionMs)) {
        return false;
    }

    m_initialVolumePercent = qBound(0, initialVolumePercent, 100);
    m_mediaSessionId = 0;
    m_loadSent = false;
    m_finishedEmitted = false;
    m_stopRequested = false;
    m_stoppedPosition = 0;
    setMediaStopped(false);
    setLastError(QString());

    qInfo() << "SailVideo Cast: replacing video media"
            << "startMs=" << m_pendingStartPosition
            << "contentType=" << m_pendingContentType
            << "requestedVolume=" << m_initialVolumePercent;

    // Keep the old receiver media active until the requested volume is
    // confirmed. processReceiverStatus() sends LOAD only after confirmation.
    qInfo() << "SailVideo Cast: applying handoff receiver volume"
            << m_initialVolumePercent
            << "and clearing receiver mute";
    setVolumePercent(m_initialVolumePercent);
    setMuted(false);
    sendReceiverStatus();
    return true;
}

void CastManager::resetForNewRequest()
{
    m_reconnectTimer.stop();
    m_reconnectScheduled = false;
    m_reconnectAttempts = 0;
    m_intentionalClose = false;
    m_readBuffer.clear();
    m_sessionId.clear();
    m_transportId.clear();
    m_mediaSessionId = 0;
    m_launchSent = false;
    m_loadSent = false;
    m_finishedEmitted = false;
    m_stopRequested = false;
    m_stoppedPosition = 0;
    m_initialVolumePercent = -1;
    m_pollTick = 0;
    const bool hadMediaInfo = !m_activeContentType.isEmpty();
    m_activeContentType.clear();
    if (hadMediaInfo) {
        emit mediaInfoChanged();
    }
    setDisconnecting(false);
    setMediaStopped(false);
    setRejoining(false);
    setConnected(false);
    setCasting(false);
    setPlayerState(QString());
    setDuration(0);
}

void CastManager::clearSessionState(bool preservePosition)
{
    m_reconnectTimer.stop();
    m_reconnectScheduled = false;
    m_reconnectAttempts = 0;
    m_sessionId.clear();
    m_transportId.clear();
    m_mediaSessionId = 0;
    m_launchSent = false;
    m_loadSent = false;
    m_pollTimer.stop();
    m_disconnectTimer.stop();
    m_stopRequested = false;
    m_stoppedPosition = 0;
    m_initialVolumePercent = -1;
    const bool hadMediaInfo = !m_activeContentType.isEmpty();
    m_activeContentType.clear();
    if (hadMediaInfo) {
        emit mediaInfoChanged();
    }
    setConnected(false);
    setCasting(false);
    setDisconnecting(false);
    setMediaStopped(false);
    setRejoining(false);
    setPlayerState(QString());
    if (!preservePosition) {
        setPosition(0);
        setDuration(0);
    }
}

void CastManager::handleEncrypted()
{
    setConnected(true);
    setStatusText(tr("Connected to %1").arg(m_deviceName));
    qInfo() << "SailVideo Cast: TLS encrypted channel ready for" << m_deviceName;

    sendConnection(QString::fromLatin1(ReceiverId));
    if (m_initialVolumePercent >= 0) {
        qInfo() << "SailVideo Cast: applying initial receiver volume"
                << m_initialVolumePercent
                << "and clearing receiver mute";
        setVolumePercent(m_initialVolumePercent);
        setMuted(false);
    }
    sendReceiverStatus();
    m_pollTimer.start();
}

void CastManager::handleSslErrors(const QList<QSslError> &errors)
{
    Q_UNUSED(errors)
    // Cast receivers use a local device certificate rather than normal web PKI.
    // The user explicitly selected this mDNS-discovered LAN endpoint.
    qInfo() << "SailVideo Cast: accepting Chromecast device TLS certificate";
    m_socket.ignoreSslErrors();
}

bool CastManager::canReconnect() const
{
    return !m_intentionalClose
            && !m_disconnecting
            && m_casting
            && !m_host.isEmpty()
            && m_reconnectAttempts < MaxReconnectAttempts;
}

void CastManager::scheduleReconnect(const QString &reason)
{
    if (!canReconnect()) {
        failConnection(reason);
        return;
    }

    if (m_reconnectScheduled || m_reconnectTimer.isActive()) {
        return;
    }

    m_pollTimer.stop();
    m_disconnectTimer.stop();
    m_readBuffer.clear();
    m_sessionId.clear();
    m_transportId.clear();
    m_mediaSessionId = 0;

    // Preserve Cast mode and media state. For local/SMB Cast this also keeps
    // the LAN HTTP bridge alive while only the control channel reconnects.
    setConnected(false);
    setRejoining(true);
    setLastError(QString());
    setStatusText(tr("Reconnecting to %1").arg(m_deviceName));

    m_reconnectScheduled = true;
    m_reconnectTimer.start();

    qWarning() << "SailVideo Cast: control connection lost; scheduling reconnect"
               << (m_reconnectAttempts + 1) << "of" << MaxReconnectAttempts
               << reason;
}

void CastManager::attemptReconnect()
{
    if (!m_reconnectScheduled || m_intentionalClose || m_disconnecting
            || !m_casting || m_host.isEmpty()) {
        m_reconnectScheduled = false;
        return;
    }

    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_reconnectTimer.start(250);
        return;
    }

    m_reconnectScheduled = false;
    ++m_reconnectAttempts;

    setStatusText(tr("Reconnecting to %1 (%2/%3)")
                  .arg(m_deviceName)
                  .arg(m_reconnectAttempts)
                  .arg(MaxReconnectAttempts));

    qInfo() << "SailVideo Cast: reconnecting TLS control channel to"
            << m_deviceName << m_host << m_port
            << "attempt" << m_reconnectAttempts;

    m_socket.connectToHostEncrypted(m_host, quint16(m_port));
}

void CastManager::failConnection(const QString &message)
{
    const QString text = message.isEmpty()
            ? tr("Chromecast connection failed.")
            : message;
    qWarning() << "SailVideo Cast:" << text;
    setLastError(text);
    setStatusText(text);
    clearSessionState(true);
}

void CastManager::handleSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    if (m_intentionalClose) {
        return;
    }

    const QString message = m_socket.errorString().isEmpty()
            ? tr("Chromecast connection failed.")
            : tr("Chromecast connection failed: %1").arg(m_socket.errorString());

    if (canReconnect()) {
        scheduleReconnect(message);
        return;
    }

    failConnection(message);
}

void CastManager::handleDisconnected()
{
    const bool intentional = m_intentionalClose;
    m_intentionalClose = false;

    if (intentional) {
        clearSessionState(true);
        return;
    }

    if (canReconnect()) {
        scheduleReconnect(tr("Chromecast connection closed."));
        return;
    }

    if (m_connected || m_casting || m_rejoining) {
        failConnection(tr("Chromecast connection closed."));
        return;
    }

    clearSessionState(true);
}

void CastManager::handleReadyRead()
{
    m_readBuffer.append(m_socket.readAll());

    while (m_readBuffer.size() >= 4) {
        const quint32 length = frameLength(m_readBuffer);
        if (length == 0 || length > 4 * 1024 * 1024) {
            setLastError(tr("Invalid Chromecast protocol frame."));
            m_socket.abort();
            return;
        }
        if (m_readBuffer.size() < int(length) + 4) {
            return;
        }

        const QByteArray payload = m_readBuffer.mid(4, int(length));
        m_readBuffer.remove(0, int(length) + 4);

        Envelope envelope;
        if (decodeEnvelope(payload, &envelope)) {
            processEnvelope(envelope);
        }
    }
}

QByteArray CastManager::encodeEnvelope(const QString &destination,
                                       const QString &nameSpace,
                                       const QString &payload) const
{
    QByteArray encoded;
    appendVarintField(&encoded, 1, 0); // CASTV2_1_0
    appendStringField(&encoded, 2, QByteArray(SourceId));
    appendStringField(&encoded, 3, destination.toUtf8());
    appendStringField(&encoded, 4, nameSpace.toUtf8());
    appendVarintField(&encoded, 5, 0); // STRING payload
    appendStringField(&encoded, 6, payload.toUtf8());
    return encoded;
}

bool CastManager::decodeEnvelope(const QByteArray &data, Envelope *envelope) const
{
    if (!envelope) {
        return false;
    }

    int offset = 0;
    while (offset < data.size()) {
        quint64 key = 0;
        if (!readVarint(data, &offset, &key)) {
            return false;
        }

        const int field = int(key >> 3);
        const int wireType = int(key & 0x07);
        if ((field == 2 || field == 3 || field == 4 || field == 6)
                && wireType == 2) {
            quint64 length = 0;
            if (!readVarint(data, &offset, &length)
                    || length > quint64(data.size() - offset)) {
                return false;
            }

            const QString value = QString::fromUtf8(data.constData() + offset, int(length));
            offset += int(length);
            if (field == 2) {
                envelope->sourceId = value;
            } else if (field == 3) {
                envelope->destinationId = value;
            } else if (field == 4) {
                envelope->nameSpace = value;
            } else if (field == 6) {
                envelope->payload = value;
            }
        } else if (!skipField(data, wireType, &offset)) {
            return false;
        }
    }

    return !envelope->nameSpace.isEmpty();
}

void CastManager::sendEnvelope(const QString &destination,
                               const QString &nameSpace,
                               const QString &payload)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        return;
    }

    const QByteArray frame = frameMessage(encodeEnvelope(destination, nameSpace, payload));
    m_socket.write(frame);
}

int CastManager::nextRequestId()
{
    if (m_requestId >= 0x3fffffff) {
        m_requestId = 1;
    }
    return m_requestId++;
}

void CastManager::sendJson(const QString &destination,
                           const QString &nameSpace,
                           QJsonObject payload,
                           bool addRequestId)
{
    if (addRequestId && !payload.contains(QStringLiteral("requestId"))) {
        payload.insert(QStringLiteral("requestId"), nextRequestId());
    }

    const QString type = payload.value(QStringLiteral("type")).toString();
    if (nameSpace != QString::fromLatin1(NsHeartbeat)) {
        qInfo() << "SailVideo Cast: send" << type
                << "namespace=" << nameSpace
                << "destination=" << destination;
    }

    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    sendEnvelope(destination, nameSpace, QString::fromUtf8(json));
}

void CastManager::sendConnection(const QString &destination)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("CONNECT"));
    sendJson(destination, QString::fromLatin1(NsConnection), payload, false);
}

void CastManager::sendReceiverStatus()
{
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("GET_STATUS"));
    sendJson(QString::fromLatin1(ReceiverId), QString::fromLatin1(NsReceiver), payload);
}

void CastManager::sendLaunch()
{
    if (m_launchSent) {
        return;
    }

    m_launchSent = true;
    setStatusText(tr("Launching Cast receiver on %1").arg(m_deviceName));

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("LAUNCH"));
    payload.insert(QStringLiteral("appId"), QString::fromLatin1(DefaultMediaReceiver));
    sendJson(QString::fromLatin1(ReceiverId), QString::fromLatin1(NsReceiver), payload);

    qInfo() << "SailVideo Cast: launching Default Media Receiver"
            << DefaultMediaReceiver;
}

void CastManager::sendLoad()
{
    if (m_loadSent || m_transportId.isEmpty() || m_pendingMediaUrl.isEmpty()) {
        return;
    }

    m_loadSent = true;
    m_finishedEmitted = false;
    sendConnection(m_transportId);

    const bool picture =
            m_pendingContentType.trimmed().toLower().startsWith(QStringLiteral("image/"));

    QJsonObject metadata;
    metadata.insert(QStringLiteral("metadataType"), picture ? 4 : 0);
    if (!m_pendingTitle.isEmpty()) {
        metadata.insert(QStringLiteral("title"), m_pendingTitle);
    }

    QJsonObject media;
    media.insert(QStringLiteral("contentId"), m_pendingMediaUrl);
    media.insert(QStringLiteral("streamType"), QStringLiteral("BUFFERED"));
    media.insert(QStringLiteral("contentType"),
                 m_pendingContentType.isEmpty()
                 ? QStringLiteral("video/mp4")
                 : m_pendingContentType);
    media.insert(QStringLiteral("metadata"), metadata);

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("LOAD"));
    payload.insert(QStringLiteral("media"), media);
    payload.insert(QStringLiteral("autoplay"), true);
    if (!picture) {
        payload.insert(QStringLiteral("currentTime"),
                       double(m_pendingStartPosition) / 1000.0);
    }
    sendJson(m_transportId, QString::fromLatin1(NsMedia), payload);

    // Initial receiver volume is only an initial handoff value. Once LOAD has
    // been issued, subsequent volume changes must be controlled by the user.
    m_initialVolumePercent = -1;

    setMediaStopped(false);
    setCasting(true);
    setStatusText(tr("Starting on %1").arg(m_deviceName));
    qInfo() << "SailVideo Cast: LOAD sent"
            << "startMs=" << m_pendingStartPosition
            << "contentType=" << m_pendingContentType;
}

void CastManager::sendMediaStatus()
{
    if (m_transportId.isEmpty()) {
        return;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("GET_STATUS"));
    sendJson(m_transportId, QString::fromLatin1(NsMedia), payload);
}

void CastManager::sendMediaCommand(const QString &type, const QJsonObject &extra)
{
    if (m_transportId.isEmpty() || m_mediaSessionId <= 0) {
        return;
    }

    QJsonObject payload = extra;
    payload.insert(QStringLiteral("type"), type);
    payload.insert(QStringLiteral("mediaSessionId"), m_mediaSessionId);
    sendJson(m_transportId, QString::fromLatin1(NsMedia), payload);
}

void CastManager::play()
{
    sendMediaCommand(QStringLiteral("PLAY"));
}

void CastManager::pause()
{
    sendMediaCommand(QStringLiteral("PAUSE"));
}

void CastManager::seek(qint64 positionMs)
{
    positionMs = qMax<qint64>(0, positionMs);

    if (m_mediaSessionId <= 0) {
        // A seek made while the receiver is still launching should become the
        // LOAD start position instead of being silently lost.
        m_pendingStartPosition = positionMs;
        setPosition(positionMs);
        setStatusText(tr("Starting at requested position on %1").arg(m_deviceName));
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("currentTime"), double(positionMs) / 1000.0);
    payload.insert(QStringLiteral("resumeState"), QStringLiteral("PLAYBACK_START"));
    sendMediaCommand(QStringLiteral("SEEK"), payload);
    setPosition(positionMs);
    setStatusText(tr("Seeking on %1").arg(m_deviceName));
}

void CastManager::stopMedia()
{
    if (m_transportId.isEmpty() || m_mediaSessionId <= 0) {
        return;
    }

    m_stoppedPosition = qMax<qint64>(0, m_position);
    m_pendingStartPosition = m_stoppedPosition;
    m_stopRequested = true;
    qInfo() << "SailVideo Cast: stopping media at"
            << m_stoppedPosition << "ms";
    sendMediaCommand(QStringLiteral("STOP"));
    m_mediaSessionId = 0;
    setMediaStopped(true);
    setPlayerState(QStringLiteral("IDLE"));
    setStatusText(tr("Media stopped on %1").arg(m_deviceName));
}

void CastManager::continueMedia()
{
    if (!m_connected) {
        setLastError(tr("Chromecast is not connected."));
        return;
    }

    if (!m_mediaStopped && m_mediaSessionId > 0) {
        play();
        return;
    }

    if (m_pendingMediaUrl.isEmpty()) {
        setLastError(tr("The stopped media URL is no longer available."));
        return;
    }

    qint64 resumePosition = m_stoppedPosition > 0
            ? m_stoppedPosition
            : qMax<qint64>(0, m_pendingStartPosition);
    if (m_duration > 0 && resumePosition >= m_duration - 1000) {
        resumePosition = 0;
    }

    m_pendingStartPosition = resumePosition;
    setPosition(resumePosition);

    qInfo() << "SailVideo Cast: continuing media at"
            << m_pendingStartPosition << "ms";

    m_mediaSessionId = 0;
    m_loadSent = false;
    m_finishedEmitted = false;
    m_stopRequested = false;
    setMediaStopped(false);
    setLastError(QString());

    if (!m_transportId.isEmpty()) {
        sendLoad();
    } else {
        sendReceiverStatus();
    }
}

void CastManager::setVolumePercent(int percent)
{
    const int bounded = qBound(0, percent, 100);

    QJsonObject volume;
    volume.insert(QStringLiteral("level"), double(bounded) / 100.0);
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("SET_VOLUME"));
    payload.insert(QStringLiteral("volume"), volume);
    sendJson(QString::fromLatin1(ReceiverId), QString::fromLatin1(NsReceiver), payload);
    setVolumeInternal(bounded);
}

void CastManager::setMuted(bool muted)
{
    QJsonObject volume;
    volume.insert(QStringLiteral("muted"), muted);
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("SET_VOLUME"));
    payload.insert(QStringLiteral("volume"), volume);
    sendJson(QString::fromLatin1(ReceiverId), QString::fromLatin1(NsReceiver), payload);
    setMutedInternal(muted);
}

void CastManager::disconnectAndStopReceiver()
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        finishDisconnect();
        return;
    }

    setDisconnecting(true);
    setStatusText(tr("Stopping Cast receiver on %1").arg(m_deviceName));
    m_pollTimer.start();

    if (!m_sessionId.isEmpty()) {
        QJsonObject payload;
        payload.insert(QStringLiteral("type"), QStringLiteral("STOP"));
        payload.insert(QStringLiteral("sessionId"), m_sessionId);
        sendJson(QString::fromLatin1(ReceiverId), QString::fromLatin1(NsReceiver), payload);
        qInfo() << "SailVideo Cast: receiver STOP sent for active session";
    } else {
        sendReceiverStatus();
    }

    m_disconnectTimer.start();
}

void CastManager::detach()
{
    m_pollTimer.stop();
    m_disconnectTimer.stop();

    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_intentionalClose = true;
        if (!m_transportId.isEmpty()) {
            QJsonObject payload;
            payload.insert(QStringLiteral("type"), QStringLiteral("CLOSE"));
            sendJson(m_transportId, QString::fromLatin1(NsConnection), payload, false);
        }
        m_socket.abort();
    }

    clearSessionState(true);
}

QString CastManager::contentTypeForUrl(const QString &url, const QString &title) const
{
    QString fileName = title.trimmed();
    if (fileName.isEmpty()) {
        fileName = QUrl(url).path();
    }

    const QString lower = fileName.toLower();

    // Google Cast documents MP4 rather than QuickTime/MOV as a progressive
    // container. Many MOV files use the same ISO-BMFF family and contain
    // Cast-compatible H.264/AAC tracks, so advertise those as video/mp4.
    // This does not transcode unsupported codecs such as ProRes.
    if (lower.endsWith(QStringLiteral(".mov"))) {
        return QStringLiteral("video/mp4");
    }

    QMimeDatabase database;
    const QMimeType type = database.mimeTypeForFile(fileName,
                                                    QMimeDatabase::MatchExtension);
    if (type.isValid() && type.name() != QStringLiteral("application/octet-stream")) {
        return type.name();
    }
    if (lower.endsWith(QStringLiteral(".mkv"))) {
        return QStringLiteral("video/x-matroska");
    }
    if (lower.endsWith(QStringLiteral(".webm"))) {
        return QStringLiteral("video/webm");
    }
    if (lower.endsWith(QStringLiteral(".avi"))) {
        return QStringLiteral("video/x-msvideo");
    }
    if (lower.endsWith(QStringLiteral(".mov"))) {
        return QStringLiteral("video/mp4");
    }
    if (lower.endsWith(QStringLiteral(".jpg"))
            || lower.endsWith(QStringLiteral(".jpeg"))) {
        return QStringLiteral("image/jpeg");
    }
    if (lower.endsWith(QStringLiteral(".png"))) {
        return QStringLiteral("image/png");
    }
    if (lower.endsWith(QStringLiteral(".gif"))) {
        return QStringLiteral("image/gif");
    }
    if (lower.endsWith(QStringLiteral(".webp"))) {
        return QStringLiteral("image/webp");
    }
    if (lower.endsWith(QStringLiteral(".ts"))
            || lower.endsWith(QStringLiteral(".mts"))
            || lower.endsWith(QStringLiteral(".m2ts"))) {
        return QStringLiteral("video/mp2t");
    }
    return QStringLiteral("video/mp4");
}

void CastManager::pollStatus()
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        return;
    }

    if (m_disconnecting) {
        sendReceiverStatus();
        return;
    }

    ++m_pollTick;
    if (!m_transportId.isEmpty() && m_loadSent) {
        sendMediaStatus();
    }
    if ((m_initialVolumePercent >= 0 && !m_loadSent)
            || m_pollTick % 5 == 0
            || m_transportId.isEmpty()) {
        sendReceiverStatus();
    }

    QJsonObject ping;
    ping.insert(QStringLiteral("type"), QStringLiteral("PING"));
    sendJson(QString::fromLatin1(ReceiverId), QString::fromLatin1(NsHeartbeat), ping, false);
}

void CastManager::processEnvelope(const Envelope &envelope)
{
    const QJsonDocument document = QJsonDocument::fromJson(envelope.payload.toUtf8());
    if (!document.isObject()) {
        return;
    }

    const QJsonObject message = document.object();
    const QString type = message.value(QStringLiteral("type")).toString();

    if (envelope.nameSpace == QString::fromLatin1(NsHeartbeat)) {
        if (type == QStringLiteral("PING")) {
            QJsonObject pong;
            pong.insert(QStringLiteral("type"), QStringLiteral("PONG"));
            sendJson(envelope.sourceId.isEmpty()
                     ? QString::fromLatin1(ReceiverId)
                     : envelope.sourceId,
                     QString::fromLatin1(NsHeartbeat),
                     pong,
                     false);
        }
        return;
    }

    qInfo() << "SailVideo Cast: receive" << type
            << "namespace=" << envelope.nameSpace;

    if (envelope.nameSpace == QString::fromLatin1(NsReceiver)) {
        if (type == QStringLiteral("RECEIVER_STATUS")) {
            processReceiverStatus(message);
        } else if (type == QStringLiteral("LAUNCH_ERROR")
                   || type == QStringLiteral("INVALID_REQUEST")) {
            processProtocolError(message);
        }
        return;
    }

    if (envelope.nameSpace == QString::fromLatin1(NsMedia)) {
        if (type == QStringLiteral("MEDIA_STATUS")) {
            processMediaStatus(message);
        } else if (type == QStringLiteral("LOAD_FAILED")
                   || type == QStringLiteral("LOAD_CANCELLED")
                   || type == QStringLiteral("INVALID_REQUEST")) {
            processProtocolError(message);
        }
    }
}

void CastManager::processReceiverStatus(const QJsonObject &message)
{
    const QJsonObject status = message.value(QStringLiteral("status")).toObject();
    const QJsonObject volume = status.value(QStringLiteral("volume")).toObject();
    bool initialVolumeReady = m_initialVolumePercent < 0;
    bool initialMuteReady = m_initialVolumePercent < 0;

    if (volume.contains(QStringLiteral("level"))) {
        const int reportedVolume =
                qBound(0,
                       qRound(volume.value(QStringLiteral("level")).toDouble() * 100.0),
                       100);
        if (m_initialVolumePercent >= 0) {
            if (qAbs(reportedVolume - m_initialVolumePercent) > 1) {
                qInfo() << "SailVideo Cast: receiver reported volume"
                        << reportedVolume
                        << "before LOAD; reapplying requested initial volume"
                        << m_initialVolumePercent;
                setVolumePercent(m_initialVolumePercent);
                initialVolumeReady = false;
            } else {
                qInfo() << "SailVideo Cast: receiver volume confirmed at"
                        << reportedVolume << "before LOAD";
                setVolumeInternal(reportedVolume);
                initialVolumeReady = true;
            }
        } else {
            setVolumeInternal(reportedVolume);
        }
    } else if (m_initialVolumePercent >= 0) {
        initialVolumeReady = false;
    }
    if (volume.contains(QStringLiteral("muted"))) {
        const bool reportedMuted =
                volume.value(QStringLiteral("muted")).toBool();
        setMutedInternal(reportedMuted);
        if (m_initialVolumePercent >= 0) {
            if (reportedMuted) {
                qInfo() << "SailVideo Cast: receiver still muted before LOAD;"
                        << "reapplying unmute";
                setMuted(false);
                initialMuteReady = false;
            } else {
                qInfo() << "SailVideo Cast: receiver unmute confirmed before LOAD";
                initialMuteReady = true;
            }
        }
    } else if (m_initialVolumePercent >= 0) {
        initialMuteReady = false;
    }

    const QJsonArray applications = status.value(QStringLiteral("applications")).toArray();
    QJsonObject receiverApp;
    for (int i = 0; i < applications.size(); ++i) {
        const QJsonObject app = applications.at(i).toObject();
        if (app.value(QStringLiteral("appId")).toString()
                == QString::fromLatin1(DefaultMediaReceiver)) {
            receiverApp = app;
            break;
        }
    }

    if (receiverApp.isEmpty()) {
        if (m_disconnecting) {
            qInfo() << "SailVideo Cast: receiver session terminated";
            finishDisconnect();
            return;
        }

        m_sessionId.clear();
        m_transportId.clear();
        m_mediaSessionId = 0;

        if (m_rejoining) {
            const QString message = tr("No active Cast receiver session was found on %1.")
                    .arg(m_deviceName);
            qInfo() << "SailVideo Cast:" << message;
            setLastError(message);
            setStatusText(message);
            setRejoining(false);
            detach();
            return;
        }

        if (!m_pendingMediaUrl.isEmpty() && !m_launchSent) {
            sendLaunch();
        } else if (m_casting) {
            setCasting(false);
            setStatusText(tr("Cast receiver stopped."));
        }
        return;
    }

    const QString newSessionId = receiverApp.value(QStringLiteral("sessionId")).toString();
    const QString newTransportId = receiverApp.value(QStringLiteral("transportId")).toString();

    if (!newSessionId.isEmpty()) {
        m_sessionId = newSessionId;
    }
    if (!newTransportId.isEmpty() && newTransportId != m_transportId) {
        m_transportId = newTransportId;
        sendConnection(m_transportId);
    }

    if (m_disconnecting) {
        return;
    }

    if (m_rejoining) {
        // Query the media already playing on the receiver; do not issue
        // another LOAD merely because the sender control socket reconnected.
        if (!m_transportId.isEmpty()) {
            sendMediaStatus();
        }
        return;
    }

    if (!m_pendingMediaUrl.isEmpty() && !m_loadSent) {
        if (!initialVolumeReady || !initialMuteReady) {
            setStatusText(tr("Preparing Cast audio on %1").arg(m_deviceName));
            return;
        }
        sendLoad();
    } else if (!m_transportId.isEmpty()) {
        sendMediaStatus();
    }
}

void CastManager::processMediaStatus(const QJsonObject &message)
{
    const QJsonArray statuses = message.value(QStringLiteral("status")).toArray();
    if (statuses.isEmpty()) {
        if (m_rejoining) {
            const QString text = tr("The Chromecast receiver is running, but no media is active.");
            qInfo() << "SailVideo Cast:" << text;
            setLastError(text);
            setStatusText(text);
            setRejoining(false);
            detach();
        } else if (m_stopRequested || m_mediaStopped) {
            m_stopRequested = false;
            m_mediaSessionId = 0;
            setMediaStopped(true);
            setPlayerState(QStringLiteral("IDLE"));
            setCasting(true);
            setStatusText(tr("Media stopped on %1").arg(m_deviceName));
        }
        return;
    }

    const QJsonObject status = statuses.at(0).toObject();

    if (m_reconnectAttempts > 0 || m_reconnectScheduled) {
        qInfo() << "SailVideo Cast: control channel rejoined successfully";
        m_reconnectTimer.stop();
        m_reconnectScheduled = false;
        m_reconnectAttempts = 0;
    }

    const int sessionId = status.value(QStringLiteral("mediaSessionId")).toInt();
    if (sessionId > 0) {
        m_mediaSessionId = sessionId;
    }

    const QString state = status.value(QStringLiteral("playerState")).toString();
    if (!state.isEmpty()) {
        setPlayerState(state);
    }
    if (status.contains(QStringLiteral("currentTime"))) {
        const qint64 reportedPosition =
                qRound64(status.value(QStringLiteral("currentTime")).toDouble() * 1000.0);
        if ((m_stopRequested || m_mediaStopped)
                && reportedPosition <= 0
                && m_stoppedPosition > 0) {
            // Some receivers report currentTime=0 while processing STOP.
            // Keep the position captured before STOP so Start / continue can
            // reload at the actual stopped point instead of from the beginning.
            setPosition(m_stoppedPosition);
        } else {
            setPosition(reportedPosition);
        }
    }

    const QJsonObject media = status.value(QStringLiteral("media")).toObject();
    const QString remoteContentId = media.value(QStringLiteral("contentId")).toString().trimmed();
    const QString remoteContentType = media.value(QStringLiteral("contentType")).toString().trimmed();
    const QJsonObject metadata = media.value(QStringLiteral("metadata")).toObject();
    const QString remoteTitle = metadata.value(QStringLiteral("title")).toString().trimmed();

    const bool remoteMatchesPending =
            !remoteContentId.isEmpty()
            && !m_pendingMediaUrl.isEmpty()
            && remoteContentId == m_pendingMediaUrl;

    QString confirmedContentType = remoteContentType;
    if (confirmedContentType.isEmpty() && remoteMatchesPending) {
        confirmedContentType = m_pendingContentType;
    }

    if (!confirmedContentType.isEmpty()
            && confirmedContentType != m_activeContentType) {
        m_activeContentType = confirmedContentType;
        emit mediaInfoChanged();
    }

    // An old MEDIA_STATUS can arrive while a picture/video replacement is in
    // flight. Never let that stale status overwrite the new pending LOAD.
    // Rejoin is the exception: there is no pending sender request, so the
    // receiver is authoritative.
    const bool adoptRemoteAsPending =
            m_rejoining || m_pendingMediaUrl.isEmpty() || remoteMatchesPending;
    if (adoptRemoteAsPending) {
        if (!remoteContentId.isEmpty()) {
            m_pendingMediaUrl = remoteContentId;
        }
        if (!remoteContentType.isEmpty()) {
            m_pendingContentType = remoteContentType;
        }
        if (!remoteTitle.isEmpty()) {
            m_pendingTitle = remoteTitle;
        }
    }

    if (media.contains(QStringLiteral("duration"))) {
        setDuration(qRound64(media.value(QStringLiteral("duration")).toDouble() * 1000.0));
    }

    if (remoteMatchesPending || m_rejoining || m_pendingMediaUrl.isEmpty()) {
        m_pendingStartPosition = m_position;
    }
    m_stopRequested = false;
    if (state == QStringLiteral("PLAYING")
            || state == QStringLiteral("PAUSED")
            || state == QStringLiteral("BUFFERING")) {
        m_stoppedPosition = 0;
    }
    setMediaStopped(false);
    setRejoining(false);
    setCasting(true);

    if (m_playerState == QStringLiteral("PLAYING")) {
        setStatusText(tr("Casting to %1").arg(m_deviceName));
    } else if (m_playerState == QStringLiteral("PAUSED")) {
        setStatusText(imageMedia()
                      ? tr("Displaying picture on %1").arg(m_deviceName)
                      : tr("Paused on %1").arg(m_deviceName));
    } else if (m_playerState == QStringLiteral("BUFFERING")) {
        setStatusText(tr("Buffering on %1").arg(m_deviceName));
    } else if (m_playerState == QStringLiteral("IDLE")) {
        const QString idleReason = status.value(QStringLiteral("idleReason")).toString();
        setMediaStopped(true);
        if (idleReason == QStringLiteral("FINISHED")) {
            m_pendingStartPosition = 0;
            m_stoppedPosition = 0;
            if (!imageMedia() && !m_finishedEmitted) {
                m_finishedEmitted = true;
                emit playbackFinished();
            }
            setStatusText(imageMedia()
                          ? tr("Picture displayed on %1").arg(m_deviceName)
                          : tr("Finished on %1").arg(m_deviceName));
        } else {
            setStatusText(tr("Media stopped on %1").arg(m_deviceName));
        }
    }
}

void CastManager::processProtocolError(const QJsonObject &message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    const QString reason = message.value(QStringLiteral("reason")).toString();
    const QString text = reason.isEmpty()
            ? tr("Chromecast error: %1").arg(type)
            : tr("Chromecast error: %1 (%2)").arg(type, reason);

    qWarning() << "SailVideo Cast:" << text;
    setLastError(text);
    setStatusText(text);
    setCasting(false);

    // Return the sender to a clean state after a LOAD/receiver protocol error.
    // The QML layer sees connected=false and resumes the still-loaded local
    // player at the preserved position.
    detach();
}

void CastManager::handleDisconnectTimeout()
{
    qWarning() << "SailVideo Cast: receiver STOP confirmation timed out; closing sender";
    finishDisconnect();
}

void CastManager::finishDisconnect()
{
    m_pollTimer.stop();
    m_disconnectTimer.stop();
    setDisconnecting(false);
    setMediaStopped(false);
    setRejoining(false);
    setCasting(false);
    setStatusText(tr("Disconnected from %1").arg(m_deviceName));

    m_intentionalClose = true;
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.disconnectFromHost();
        if (m_socket.state() != QAbstractSocket::UnconnectedState) {
            m_socket.abort();
        }
    }
    setConnected(false);
}

void CastManager::setConnected(bool value)
{
    if (m_connected == value) return;
    m_connected = value;
    emit connectedChanged();
}

void CastManager::setCasting(bool value)
{
    if (m_casting == value) return;
    m_casting = value;
    emit castingChanged();
}

void CastManager::setDisconnecting(bool value)
{
    if (m_disconnecting == value) return;
    m_disconnecting = value;
    emit disconnectingChanged();
}

void CastManager::setMediaStopped(bool value)
{
    if (m_mediaStopped == value) return;
    m_mediaStopped = value;
    emit mediaStoppedChanged();
}

void CastManager::setRejoining(bool value)
{
    if (m_rejoining == value) return;
    m_rejoining = value;
    emit rejoiningChanged();
}

void CastManager::setPlayerState(const QString &state)
{
    if (m_playerState == state) return;
    m_playerState = state;
    const bool nowPlaying = state == QStringLiteral("PLAYING");
    if (m_playing != nowPlaying) {
        m_playing = nowPlaying;
        emit playingChanged();
    }
    emit playerStateChanged();
}

void CastManager::setPosition(qint64 value)
{
    value = qMax<qint64>(0, value);
    if (m_position == value) return;
    m_position = value;
    emit positionChanged();
}

void CastManager::setDuration(qint64 value)
{
    value = qMax<qint64>(0, value);
    if (m_duration == value) return;
    m_duration = value;
    emit durationChanged();
}

void CastManager::setVolumeInternal(int percent)
{
    percent = qBound(0, percent, 100);
    if (m_volumePercent == percent) return;
    m_volumePercent = percent;
    emit volumePercentChanged();
}

void CastManager::setMutedInternal(bool muted)
{
    if (m_muted == muted) return;
    m_muted = muted;
    emit mutedChanged();
}

void CastManager::setStatusText(const QString &text)
{
    if (m_statusText == text) return;
    m_statusText = text;
    emit statusTextChanged();
}

void CastManager::setLastError(const QString &message)
{
    if (m_lastError == message) return;
    m_lastError = message;
    emit lastErrorChanged();
}
