/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef SMBBACKEND_H
#define SMBBACKEND_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>

#include <memory>

class QFutureWatcherBase;

enum class SmbEntryType {
    File,
    Directory,
    Other
};

class SmbFileReader
{
public:
    ~SmbFileReader();

    bool isOpen() const;
    qint64 maxReadSize() const;
    QByteArray read(qint64 offset, qint64 count, QString *errorString);
    QString errorString() const;

private:
    friend class SmbBackend;
    SmbFileReader();

    struct Private;
    std::unique_ptr<Private> d;
};

class SmbBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString backendStatus READ backendStatus NOTIFY backendStatusChanged)

public:
    explicit SmbBackend(QObject *parent = nullptr);
    ~SmbBackend() override;

    bool busy() const;
    QString lastError() const;
    QString backendStatus() const;

    Q_INVOKABLE bool backendAvailable() const;
    Q_INVOKABLE QString smbUrlForFile(const QString &host,
                                      int port,
                                      const QString &share,
                                      const QString &path) const;
    Q_INVOKABLE bool isSmbUrl(const QString &url) const;
    Q_INVOKABLE QVariantMap parseSmbUrl(const QString &url) const;
    Q_INVOKABLE bool isLikelyVideoFile(const QString &fileName) const;
    Q_INVOKABLE bool isLikelyImageFile(const QString &fileName) const;
    Q_INVOKABLE QString displayPath(const QString &path) const;
    Q_INVOKABLE qint64 fileSize(const QString &host,
                                int port,
                                const QString &share,
                                const QString &path,
                                const QString &domain,
                                const QString &username,
                                const QString &password,
                                bool guest);

    Q_INVOKABLE void listDirectory(const QString &requestId,
                                   const QString &host,
                                   int port,
                                   const QString &share,
                                   const QString &path,
                                   const QString &domain,
                                   const QString &username,
                                   const QString &password,
                                   bool guest);

    static QString cleanedText(const QString &value);
    static QString normalizedHost(const QString &host);
    static QString normalizedShare(const QString &share);
    static QString normalizedPath(const QString &path);
    static QString joinPath(const QString &basePath, const QString &childName);
    static QString fileNameFromPath(const QString &path);
    static QString humanSize(qint64 bytes);
    static QString serverForLibsmb2(const QString &host, int port);
    static bool likelyVideoExtension(const QString &fileName);
    static bool likelyImageExtension(const QString &fileName);

    std::unique_ptr<SmbFileReader> openFile(const QString &host,
                                            int port,
                                            const QString &share,
                                            const QString &path,
                                            const QString &domain,
                                            const QString &username,
                                            const QString &password,
                                            bool guest,
                                            QString *errorString) const;

signals:
    void busyChanged();
    void lastErrorChanged();
    void backendStatusChanged();
    void directoryReady(const QString &finishedRequestId,
                        const QVariantList &entries,
                        const QString &errorString);

private slots:
    void handleListFinished();

private:
    struct ListRequest {
        QString requestId;
        QString host;
        int port = 445;
        QString share;
        QString path;
        QString domain;
        QString username;
        QString password;
        bool guest = true;
    };

    struct ListResult {
        QString requestId;
        QVariantList entries;
        QString errorString;
    };

    static ListResult listDirectorySync(const ListRequest &request);

    void startListRequest(const ListRequest &request);
    void setBusy(bool busy);
    void setLastError(const QString &message);
    void updateBackendStatus() const;

    bool m_busy = false;
    QString m_lastError;
    mutable QString m_backendStatus;
    QFutureWatcherBase *m_listWatcher = nullptr;
};

#endif // SMBBACKEND_H
