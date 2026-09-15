/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef LOCALFILESTREAMSERVER_H
#define LOCALFILESTREAMSERVER_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QTcpServer>
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
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(qint64 lastResolvedSize READ lastResolvedSize NOTIFY lastResolvedSizeChanged)

public:
    explicit LocalFileStreamServer(QObject *parent = nullptr);
    ~LocalFileStreamServer() override;

    void setSmbBackend(SmbBackend *backend);

    bool running() const;
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
    Q_INVOKABLE bool isLocalFile(const QString &urlOrPath) const;
    Q_INVOKABLE void clear();

signals:
    void runningChanged();
    void lastErrorChanged();
    void lastResolvedSizeChanged();

private slots:
    void handleNewConnection();
    void readClientRequest();
    void pumpClientData();
    void cleanupClient();

private:
    enum class StreamType {
        LocalFile,
        SmbFile
    };

    struct StreamEntry {
        StreamType type = StreamType::LocalFile;
        QString filePath;
        QString fileName;
        QString mimeType;
        qint64 size = 0;

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
    };

    bool ensureListening();
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
                       bool headOnly);
    void pumpClientData(QTcpSocket *socket);
    void closeTransfer(QTcpSocket *socket);

    QTcpServer m_server;
    QString m_lastError;
    qint64 m_lastResolvedSize = 0;
    SmbBackend *m_smbBackend = nullptr;
    QHash<QString, StreamEntry> m_entries;
    QHash<QTcpSocket *, QByteArray> m_pendingRequests;
    QHash<QTcpSocket *, Transfer *> m_transfers;
};

#endif // LOCALFILESTREAMSERVER_H
