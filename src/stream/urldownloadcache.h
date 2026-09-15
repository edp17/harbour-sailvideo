/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef URLDOWNLOADCACHE_H
#define URLDOWNLOADCACHE_H

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkReply;

class UrlDownloadCache : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(qint64 bytesReceived READ bytesReceived NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY progressChanged)

public:
    explicit UrlDownloadCache(const QString &storageDirectory = QString(), QObject *parent = nullptr);
    ~UrlDownloadCache() override;

    bool busy() const;
    QString lastError() const;
    qint64 bytesReceived() const;
    qint64 bytesTotal() const;

    Q_INVOKABLE bool prepare(const QString &urlString);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QString cachedFileUrlFor(const QString &urlString) const;

signals:
    void busyChanged();
    void lastErrorChanged();
    void progressChanged();
    void downloadProgress(const QString &sourceUrl, qint64 bytesReceived, qint64 bytesTotal);
    void ready(const QString &sourceUrl, const QString &fileUrl);
    void failed(const QString &sourceUrl, const QString &message);

private slots:
    void handleReadyRead();
    void handleDownloadProgress(qint64 received, qint64 total);
    void handleFinished();

private:
    QString cacheDirectory() const;
    QString cachePathForUrl(const QUrl &url) const;
    QString fileSuffixForUrl(const QUrl &url) const;
    bool startRequest(const QUrl &url, int redirectCount);
    void finishWithError(const QString &message);
    void setBusy(bool busy);
    void setLastError(const QString &message);
    void setProgress(qint64 received, qint64 total);

    QString m_storageDirectory;
    QNetworkAccessManager m_manager;
    QNetworkReply *m_reply = nullptr;
    QFile m_file;
    QString m_sourceUrl;
    QString m_targetPath;
    QString m_partPath;
    int m_redirectCount = 0;
    bool m_busy = false;
    QString m_lastError;
    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = 0;
};

#endif // URLDOWNLOADCACHE_H
