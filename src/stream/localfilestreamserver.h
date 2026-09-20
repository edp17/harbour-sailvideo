/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef LOCALFILESTREAMSERVER_H
#define LOCALFILESTREAMSERVER_H

#include <QHash>
#include <QHostAddress>
#include <QSet>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>

#include <memory>

class QFile;
class QTcpSocket;
class SmbBackend;
class SmbFileReader;

class LocalFileStreamServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool lanRunning READ lanRunning NOTIFY lanRunningChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(qint64 lastResolvedSize READ lastResolvedSize NOTIFY lastResolvedSizeChanged)

public:
    explicit LocalFileStreamServer(QObject *parent = nullptr);
    ~LocalFileStreamServer() override;

    void setSmbBackend(SmbBackend *backend);

    bool running() const;
    bool lanRunning() const;
    QString lastError() const;
    qint64 lastResolvedSize() const;

    Q_INVOKABLE QString streamUrlForLocalFile(const QString &urlOrPath);
    Q_INVOKABLE QString streamUrlForSmbFile(const QString &host,
                                            int port,
                                            const QString &share,
                                            const QString &path,
                                            const QString &domain,
                                            const QString &username,
                                            const QString &password,
                                            bool guest,
                                            qint64 size,
                                            const QString &fileName);
    Q_INVOKABLE QString lanStreamUrlForLocalFile(const QString &urlOrPath,
                                                      const QString &peerHost);
    Q_INVOKABLE QString lanStreamUrlForGrowingLocalFile(const QString &urlOrPath,
                                                        const QString &peerHost,
                                                        const QString &mimeType);
    Q_INVOKABLE void markGrowingLocalFileComplete(const QString &urlOrPath);
    Q_INVOKABLE void abortGrowingLocalFile(const QString &urlOrPath);
    Q_INVOKABLE QString lanStreamUrlForSmbFile(const QString &host,
                                               int port,
                                               const QString &share,
                                               const QString &path,
                                               const QString &domain,
                                               const QString &username,
                                               const QString &password,
                                               bool guest,
                                               qint64 size,
                                               const QString &fileName,
                                               const QString &peerHost);
    Q_INVOKABLE bool isLocalFile(const QString &urlOrPath) const;
    Q_INVOKABLE void stopLanSharing();
    Q_INVOKABLE void clear();

signals:
    void runningChanged();
    void lanRunningChanged();
    void lastErrorChanged();
    void lastResolvedSizeChanged();

private slots:
    void handleNewConnection();
    void readClientRequest();
    void pumpClientData();
    void pumpGrowingTransfers();
    void cleanupClient();

private:
    enum class StreamType {
        LocalFile,
        GrowingLocalFile,
        SmbFile
    };

    struct StreamEntry {
        StreamType type = StreamType::LocalFile;
        QString filePath;
        QString fileName;
        QString mimeType;
        qint64 size = 0;
        bool growingComplete = false;
        bool growingFailed = false;

        QString smbHost;
        int smbPort = 445;
        QString smbShare;
        QString smbPath;
        QString smbDomain;
        QString smbUsername;
        QString smbPassword;
        bool smbGuest = true;
    };

    struct Transfer {
        QFile *file = nullptr;
        std::unique_ptr<SmbFileReader> smbReader;
        qint64 remaining = 0;
        qint64 nextOffset = 0;
        QString token;
        bool growing = false;
    };

    bool ensureListening();
    bool ensureLanListening(const QString &peerHost);
    QHostAddress lanAddressForPeer(const QHostAddress &peer) const;
    QString lanUrlForLoopbackUrl(const QString &loopbackUrl,
                                 const QString &peerHost);
    QString localFilePath(const QString &urlOrPath) const;
    QString createToken() const;
    void setLastError(const QString &message);
    void setLastResolvedSize(qint64 size);

    void handleRequest(QTcpSocket *socket, const QByteArray &request);
    void sendSimpleResponse(QTcpSocket *socket,
                            int statusCode,
                            const QByteArray &reason,
                            const QByteArray &body = QByteArray());
    void sendRangeNotSatisfiable(QTcpSocket *socket, qint64 size);
    bool parseRange(const QByteArray &rangeHeader,
                    qint64 size,
                    qint64 *start,
                    qint64 *end) const;
    void startTransfer(QTcpSocket *socket,
                       const StreamEntry &entry,
                       qint64 start,
                       qint64 end,
                       bool partial,
                       bool headOnly,
                       bool allowSmbRetry = true);
    void startGrowingTransfer(QTcpSocket *socket,
                              const QString &token,
                              const StreamEntry &entry,
                              bool headOnly);
    void pumpClientData(QTcpSocket *socket);
    void closeTransfer(QTcpSocket *socket);

    QTcpServer m_server;
    QTcpServer m_lanServer;
    QHostAddress m_lanAllowedPeer;
    QSet<QTcpSocket *> m_lanClients;
    QString m_lastError;
    qint64 m_lastResolvedSize = 0;
    SmbBackend *m_smbBackend = nullptr;
    QHash<QString, StreamEntry> m_entries;
    QHash<QTcpSocket *, QByteArray> m_pendingRequests;
    QHash<QTcpSocket *, Transfer *> m_transfers;
    QTimer m_growingPumpTimer;
};

#endif // LOCALFILESTREAMSERVER_H
