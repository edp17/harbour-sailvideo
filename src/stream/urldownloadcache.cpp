/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "urldownloadcache.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>

namespace {

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

QString cleanedText(const QString &value)
{
    return value.trimmed();
}

} // namespace

UrlDownloadCache::UrlDownloadCache(const QString &storageDirectory, QObject *parent)
    : QObject(parent)
    , m_storageDirectory(storageDirectory.trimmed().isEmpty()
                         ? defaultStorageDirectory()
                         : storageDirectory.trimmed())
{
}

UrlDownloadCache::~UrlDownloadCache()
{
    cancel();
}

bool UrlDownloadCache::busy() const
{
    return m_busy;
}

QString UrlDownloadCache::lastError() const
{
    return m_lastError;
}

qint64 UrlDownloadCache::bytesReceived() const
{
    return m_bytesReceived;
}

qint64 UrlDownloadCache::bytesTotal() const
{
    return m_bytesTotal;
}

QString UrlDownloadCache::cacheDirectory() const
{
    return QDir(m_storageDirectory).filePath(QStringLiteral("url-cache"));
}

QString UrlDownloadCache::fileSuffixForUrl(const QUrl &url) const
{
    const QString path = url.path();
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    QString fileName = slash >= 0 ? path.mid(slash + 1) : path;
    const int queryStart = fileName.indexOf(QLatin1Char('?'));
    if (queryStart >= 0) {
        fileName = fileName.left(queryStart);
    }

    const QString suffix = QFileInfo(fileName).suffix().trimmed().toLower();
    if (suffix.isEmpty() || suffix.size() > 8) {
        return QStringLiteral("mp4");
    }
    return suffix;
}

QString UrlDownloadCache::cachePathForUrl(const QUrl &url) const
{
    const QByteArray hash = QCryptographicHash::hash(url.toString(QUrl::FullyEncoded).toUtf8(),
                                                     QCryptographicHash::Sha1).toHex();
    return QDir(cacheDirectory()).filePath(QString::fromLatin1(hash)
                                           + QLatin1Char('.')
                                           + fileSuffixForUrl(url));
}

QString UrlDownloadCache::cachedFileUrlFor(const QString &urlString) const
{
    const QUrl url(cleanedText(urlString));
    if (!url.isValid() || url.scheme().isEmpty()) {
        return QString();
    }

    const QString path = cachePathForUrl(url);
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.size() <= 0) {
        return QString();
    }

    return QUrl::fromLocalFile(info.absoluteFilePath()).toString();
}

bool UrlDownloadCache::prepare(const QString &urlString)
{
    const QUrl url(cleanedText(urlString));
    if (!url.isValid()
            || (url.scheme().toLower() != QLatin1String("http")
                && url.scheme().toLower() != QLatin1String("https"))) {
        finishWithError(tr("Enter a direct HTTP or HTTPS video URL."));
        return false;
    }

    cancel();

    QDir directory(cacheDirectory());
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        finishWithError(tr("Could not create the URL cache directory."));
        return false;
    }

    m_sourceUrl = url.toString();
    m_targetPath = cachePathForUrl(url);
    m_partPath = m_targetPath + QStringLiteral(".part");
    m_redirectCount = 0;
    setProgress(0, 0);
    setLastError(QString());

    const QFileInfo cachedInfo(m_targetPath);
    if (cachedInfo.exists() && cachedInfo.isFile() && cachedInfo.size() > 0) {
        const QString fileUrl = QUrl::fromLocalFile(cachedInfo.absoluteFilePath()).toString();
        QTimer::singleShot(0, this, [this, source = m_sourceUrl, fileUrl]() {
            emit ready(source, fileUrl);
        });
        return true;
    }

    return startRequest(url, 0);
}

bool UrlDownloadCache::startRequest(const QUrl &url, int redirectCount)
{
    if (redirectCount > 5) {
        finishWithError(tr("Too many redirects while downloading the URL."));
        return false;
    }

    if (m_file.isOpen()) {
        m_file.close();
    }

    QFile::remove(m_partPath);
    m_file.setFileName(m_partPath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        finishWithError(tr("Could not write the URL cache file."));
        return false;
    }

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "SailVideo/0.9");
    request.setRawHeader("Accept", "video/*,*/*");

    m_redirectCount = redirectCount;
    m_reply = m_manager.get(request);
    connect(m_reply, &QNetworkReply::readyRead,
            this, &UrlDownloadCache::handleReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &UrlDownloadCache::handleDownloadProgress);
    connect(m_reply, &QNetworkReply::finished,
            this, &UrlDownloadCache::handleFinished);

    setBusy(true);
    qDebug() << "SailVideo: downloading URL to cache" << url << m_targetPath;
    return true;
}

void UrlDownloadCache::cancel()
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    if (m_file.isOpen()) {
        m_file.close();
    }

    if (!m_partPath.isEmpty()) {
        QFile::remove(m_partPath);
    }

    setBusy(false);
    setProgress(0, 0);
}

void UrlDownloadCache::handleReadyRead()
{
    if (!m_reply || !m_file.isOpen()) {
        return;
    }

    const QByteArray data = m_reply->readAll();
    if (!data.isEmpty()) {
        m_file.write(data);
    }
}

void UrlDownloadCache::handleDownloadProgress(qint64 received, qint64 total)
{
    setProgress(received, total);
    emit downloadProgress(m_sourceUrl, received, total);
}

void UrlDownloadCache::handleFinished()
{
    QNetworkReply *reply = m_reply;
    if (!reply) {
        return;
    }

    handleReadyRead();

    const QVariant redirectTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    const QUrl redirectUrl = redirectTarget.isValid()
            ? reply->url().resolved(redirectTarget.toUrl())
            : QUrl();

    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();

    reply->deleteLater();
    m_reply = nullptr;

    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }

    if (redirectUrl.isValid()
            && redirectUrl != reply->url()
            && networkError == QNetworkReply::NoError) {
        QFile::remove(m_partPath);
        startRequest(redirectUrl, m_redirectCount + 1);
        return;
    }

    if (networkError != QNetworkReply::NoError) {
        QFile::remove(m_partPath);
        finishWithError(networkErrorText.isEmpty()
                        ? tr("URL download failed.")
                        : networkErrorText);
        return;
    }

    QFile::remove(m_targetPath);
    if (!QFile::rename(m_partPath, m_targetPath)) {
        QFile::remove(m_partPath);
        finishWithError(tr("Could not finalise the URL cache file."));
        return;
    }

    const QFileInfo info(m_targetPath);
    if (!info.exists() || info.size() <= 0) {
        QFile::remove(m_targetPath);
        finishWithError(tr("The URL downloaded an empty file."));
        return;
    }

    setBusy(false);
    setLastError(QString());
    const QString fileUrl = QUrl::fromLocalFile(info.absoluteFilePath()).toString();
    qDebug() << "SailVideo: URL cached" << m_sourceUrl << fileUrl << "bytes:" << info.size();
    emit ready(m_sourceUrl, fileUrl);
}

void UrlDownloadCache::finishWithError(const QString &message)
{
    if (m_file.isOpen()) {
        m_file.close();
    }
    if (!m_partPath.isEmpty()) {
        QFile::remove(m_partPath);
    }

    setBusy(false);
    const QString cleanMessage = message.trimmed().isEmpty()
            ? tr("URL download failed.")
            : message.trimmed();
    setLastError(cleanMessage);
    if (!m_sourceUrl.isEmpty()) {
        emit failed(m_sourceUrl, cleanMessage);
    }
}

void UrlDownloadCache::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void UrlDownloadCache::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}

void UrlDownloadCache::setProgress(qint64 received, qint64 total)
{
    if (m_bytesReceived == received && m_bytesTotal == total) {
        return;
    }
    m_bytesReceived = received;
    m_bytesTotal = total;
    emit progressChanged();
}
