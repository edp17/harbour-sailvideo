/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef CASTMANAGER_H
#define CASTMANAGER_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSslError>
#include <QSslSocket>
#include <QString>
#include <QTimer>

class CastManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool casting READ casting NOTIFY castingChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool disconnecting READ disconnecting NOTIFY disconnectingChanged)
    Q_PROPERTY(bool mediaStopped READ mediaStopped NOTIFY mediaStoppedChanged)
    Q_PROPERTY(bool rejoining READ rejoining NOTIFY rejoiningChanged)
    Q_PROPERTY(bool imageMedia READ imageMedia NOTIFY mediaInfoChanged)
    Q_PROPERTY(bool mediaInfoKnown READ mediaInfoKnown NOTIFY mediaInfoChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceChanged)
    Q_PROPERTY(QString host READ host NOTIFY deviceChanged)
    Q_PROPERTY(int port READ port NOTIFY deviceChanged)
    Q_PROPERTY(QString playerState READ playerState NOTIFY playerStateChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int volumePercent READ volumePercent NOTIFY volumePercentChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit CastManager(QObject *parent = nullptr);
    ~CastManager() override;

    bool connected() const;
    bool casting() const;
    bool playing() const;
    bool disconnecting() const;
    bool mediaStopped() const;
    bool rejoining() const;
    bool imageMedia() const;
    bool mediaInfoKnown() const;
    QString deviceName() const;
    QString host() const;
    int port() const;
    QString playerState() const;
    qint64 position() const;
    qint64 duration() const;
    int volumePercent() const;
    bool muted() const;
    QString statusText() const;
    QString lastError() const;

    Q_INVOKABLE bool startCasting(const QString &deviceName,
                                  const QString &host,
                                  int port,
                                  const QString &mediaUrl,
                                  const QString &contentType,
                                  const QString &title,
                                  qint64 startPositionMs,
                                  int initialVolumePercent);
    Q_INVOKABLE bool rejoin(const QString &deviceName,
                            const QString &host,
                            int port);
    Q_INVOKABLE bool replaceMedia(const QString &mediaUrl,
                                  const QString &contentType,
                                  const QString &title,
                                  qint64 startPositionMs = 0);
    Q_INVOKABLE bool replaceMediaWithVolume(const QString &mediaUrl,
                                            const QString &contentType,
                                            const QString &title,
                                            qint64 startPositionMs,
                                            int initialVolumePercent);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void stopMedia();
    Q_INVOKABLE void continueMedia();
    Q_INVOKABLE void setVolumePercent(int percent);
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void disconnectAndStopReceiver();
    Q_INVOKABLE void detach();
    Q_INVOKABLE QString contentTypeForUrl(const QString &url,
                                          const QString &title = QString()) const;

signals:
    void connectedChanged();
    void castingChanged();
    void playingChanged();
    void disconnectingChanged();
    void mediaStoppedChanged();
    void rejoiningChanged();
    void mediaInfoChanged();
    void deviceChanged();
    void playerStateChanged();
    void positionChanged();
    void durationChanged();
    void volumePercentChanged();
    void mutedChanged();
    void statusTextChanged();
    void lastErrorChanged();
    void playbackFinished();

private slots:
    void handleEncrypted();
    void handleReadyRead();
    void handleDisconnected();
    void handleSocketError(QAbstractSocket::SocketError error);
    void handleSslErrors(const QList<QSslError> &errors);
    void pollStatus();
    void handleDisconnectTimeout();

private:
    struct Envelope {
        QString sourceId;
        QString destinationId;
        QString nameSpace;
        QString payload;
    };

    void resetForNewRequest();
    void clearSessionState(bool preservePosition);
    void setConnected(bool value);
    void setCasting(bool value);
    void setDisconnecting(bool value);
    void setMediaStopped(bool value);
    void setRejoining(bool value);
    void setPlayerState(const QString &state);
    void setPosition(qint64 value);
    void setDuration(qint64 value);
    void setVolumeInternal(int percent);
    void setMutedInternal(bool muted);
    void setStatusText(const QString &text);
    void setLastError(const QString &message);
    bool configurePendingMedia(const QString &mediaUrl,
                               const QString &contentType,
                               const QString &title,
                               qint64 startPositionMs);

    void sendConnection(const QString &destination);
    void sendReceiverStatus();
    void sendLaunch();
    void sendLoad();
    void sendMediaStatus();
    void sendMediaCommand(const QString &type,
                          const QJsonObject &extra = QJsonObject());
    void sendJson(const QString &destination,
                  const QString &nameSpace,
                  QJsonObject payload,
                  bool addRequestId = true);
    void sendEnvelope(const QString &destination,
                      const QString &nameSpace,
                      const QString &payload);

    QByteArray encodeEnvelope(const QString &destination,
                              const QString &nameSpace,
                              const QString &payload) const;
    bool decodeEnvelope(const QByteArray &data, Envelope *envelope) const;
    void processEnvelope(const Envelope &envelope);
    void processReceiverStatus(const QJsonObject &message);
    void processMediaStatus(const QJsonObject &message);
    void processProtocolError(const QJsonObject &message);
    void finishDisconnect();

    int nextRequestId();

    QSslSocket m_socket;
    QByteArray m_readBuffer;
    QTimer m_pollTimer;
    QTimer m_disconnectTimer;

    bool m_connected = false;
    bool m_casting = false;
    bool m_playing = false;
    bool m_disconnecting = false;
    bool m_mediaStopped = false;
    bool m_rejoining = false;
    bool m_stopRequested = false;
    bool m_intentionalClose = false;
    bool m_launchSent = false;
    bool m_loadSent = false;
    bool m_finishedEmitted = false;

    QString m_deviceName;
    QString m_host;
    int m_port = 8009;
    QString m_playerState;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    int m_volumePercent = 100;
    bool m_muted = false;
    QString m_statusText;
    QString m_lastError;

    QString m_activeContentType;
    QString m_pendingMediaUrl;
    QString m_pendingContentType;
    QString m_pendingTitle;
    qint64 m_pendingStartPosition = 0;
    qint64 m_stoppedPosition = 0;
    int m_initialVolumePercent = -1;

    QString m_sessionId;
    QString m_transportId;
    int m_mediaSessionId = 0;
    int m_requestId = 1;
    int m_pollTick = 0;
};

#endif // CASTMANAGER_H
