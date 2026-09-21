/*
 * Copyright (C) 2026 edp17
 * GPL-3.0-or-later
 */
#include "smbstreamsession.h"
#include "smb/smbbackend.h"
#include <QDebug>
#include <QMetaObject>
#include <QThread>
namespace { const qint64 SessionReadChunk = 64 * 1024; }

SmbStreamSession::SmbStreamSession(const QString &token,
                                   const QString &host, int port,
                                   const QString &share, const QString &path,
                                   const QString &domain,
                                   const QString &username,
                                   const QString &password,
                                   bool guest, QObject *parent)
    : QObject(parent), m_token(token), m_host(host),
      m_share(share), m_path(path), m_domain(domain),
      m_username(username), m_password(password),
      m_port(port > 0 ? port : 445), m_guest(guest) {}

SmbStreamSession::~SmbStreamSession() = default;

void SmbStreamSession::requestRange(quint64 id, qint64 offset,
                                    qint64 length, bool retry)
{
    if (m_stopping || !id || length <= 0) {
        emit requestFailed(id, tr("Invalid SMB Range request."));
        return;
    }
    Request r;
    r.id = id;
    r.offset = qMax<qint64>(0, offset);
    r.remaining = length;
    r.allowOpenRetry = retry;
    m_requests.insert(id, r);
    removeQueuedId(id);
    m_order.prepend(id);
    qInfo() << "SailVideo SMB session:" << m_token.left(8)
            << "range" << id << "offset" << r.offset
            << "length" << r.remaining << "active" << m_requests.size();
    scheduleProcess();
}

void SmbStreamSession::cancelRequest(quint64 id)
{
    const bool removed = m_requests.remove(id) > 0;
    removeQueuedId(id);
    if (removed)
        qInfo() << "SailVideo SMB session:" << m_token.left(8)
                << "cancelled range" << id << "remaining" << m_requests.size();
}

void SmbStreamSession::acknowledge(quint64 id)
{
    auto it = m_requests.find(id);
    if (it == m_requests.end() || m_stopping || !it->waitingAck) return;
    it->waitingAck = false;
    removeQueuedId(id);
    m_order.append(id);
    scheduleProcess();
}

void SmbStreamSession::shutdown()
{
    if (m_stopping) return;
    m_stopping = true;
    m_requests.clear();
    m_order.clear();
    m_reader.reset();
    qInfo() << "SailVideo SMB session:" << m_token.left(8) << "stopped";
    emit stopped(m_token);
}

bool SmbStreamSession::ensureReader(bool retry, QString *errorString)
{
    if (m_reader && m_reader->isOpen()) {
        if (errorString) errorString->clear();
        return true;
    }

    const int attempts = retry ? 2 : 1;
    QString error;
    for (int i = 0; i < attempts && !m_stopping; ++i) {
        SmbBackend backend;
        m_reader = backend.openFile(m_host, m_port, m_share, m_path,
                                    m_domain, m_username, m_password,
                                    m_guest, &error);
        if (m_reader && m_reader->isOpen()) {
            if (errorString) errorString->clear();
            qInfo() << "SailVideo SMB session:" << m_token.left(8)
                    << "opened persistent reader";
            return true;
        }
        m_reader.reset();
        if (i + 1 < attempts && !m_stopping) {
            qInfo() << "SailVideo SMB session:" << m_token.left(8)
                    << "cold-NAS open retry";
            QThread::msleep(800);
        }
    }
    if (errorString)
        *errorString = error.isEmpty() ? tr("Could not open SMB file stream.")
                                      : error;
    return false;
}

void SmbStreamSession::scheduleProcess()
{
    if (m_stopping || m_processScheduled || m_order.isEmpty()) return;
    m_processScheduled = true;
    QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}

void SmbStreamSession::removeQueuedId(quint64 id)
{
    m_order.removeAll(id);
}

void SmbStreamSession::processNext()
{
    m_processScheduled = false;
    if (m_stopping) return;

    quint64 id = 0;
    while (!m_order.isEmpty()) {
        const quint64 candidate = m_order.takeFirst();
        auto ci = m_requests.constFind(candidate);
        if (ci != m_requests.constEnd() && !ci->waitingAck) {
            id = candidate;
            break;
        }
    }
    if (!id) return;

    auto it = m_requests.find(id);
    if (it == m_requests.end()) {
        scheduleProcess();
        return;
    }

    QString error;
    if (!ensureReader(it->allowOpenRetry, &error)) {
        m_requests.erase(it);
        emit requestFailed(id, error);
        scheduleProcess();
        return;
    }

    it = m_requests.find(id);
    if (it == m_requests.end()) {
        scheduleProcess();
        return;
    }
    if (!it->readyEmitted) {
        it->readyEmitted = true;
        emit requestReady(id);
    }

    const qint64 wanted = qMin(SessionReadChunk, it->remaining);
    QString readError;
    const QByteArray bytes = m_reader->read(it->offset, wanted, &readError);

    it = m_requests.find(id);
    if (it == m_requests.end()) {
        scheduleProcess();
        return;
    }

    if (bytes.isEmpty()) {
        if (it->readReconnects < 1 && !m_stopping) {
            ++it->readReconnects;
            m_reader.reset();
            m_order.append(id);
            qInfo() << "SailVideo SMB session:" << m_token.left(8)
                    << "reopening after read failure" << id << readError;
            scheduleProcess();
            return;
        }
        const QString msg = readError.isEmpty()
                ? tr("The SMB stream ended unexpectedly.")
                : tr("SMB stream read failed: %1").arg(readError);
        m_requests.erase(it);
        emit requestFailed(id, msg);
        scheduleProcess();
        return;
    }

    it->offset += bytes.size();
    it->remaining -= bytes.size();
    const bool last = it->remaining <= 0;
    if (last) m_requests.erase(it);
    else it->waitingAck = true;

    emit chunkReady(id, bytes, last);
    scheduleProcess();
}
