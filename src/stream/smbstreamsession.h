/*
 * Copyright (C) 2026 edp17
 * GPL-3.0-or-later
 */
#ifndef SMBSTREAMSESSION_H
#define SMBSTREAMSESSION_H
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <memory>
class SmbFileReader;

class SmbStreamSession : public QObject
{
    Q_OBJECT
public:
    explicit SmbStreamSession(const QString &token,
                              const QString &host, int port,
                              const QString &share, const QString &path,
                              const QString &domain,
                              const QString &username,
                              const QString &password,
                              bool guest, QObject *parent = nullptr);
    ~SmbStreamSession() override;
signals:
    void requestReady(quint64 requestId);
    void chunkReady(quint64 requestId, const QByteArray &data, bool lastChunk);
    void requestFailed(quint64 requestId, const QString &message);
    void stopped(const QString &token);
public slots:
    void requestRange(quint64 requestId, qint64 offset, qint64 length,
                      bool allowOpenRetry);
    void cancelRequest(quint64 requestId);
    void acknowledge(quint64 requestId);
    void shutdown();
private slots:
    void processNext();
private:
    struct Request {
        quint64 id = 0;
        qint64 offset = 0;
        qint64 remaining = 0;
        bool allowOpenRetry = true;
        bool readyEmitted = false;
        bool waitingAck = false;
        int readReconnects = 0;
    };
    bool ensureReader(bool allowRetry, QString *errorString);
    void scheduleProcess();
    void removeQueuedId(quint64 requestId);
    QString m_token, m_host, m_share, m_path, m_domain, m_username, m_password;
    int m_port = 445;
    bool m_guest = true;
    std::unique_ptr<SmbFileReader> m_reader;
    QHash<quint64, Request> m_requests;
    QList<quint64> m_order;
    bool m_processScheduled = false;
    bool m_stopping = false;
};
#endif
