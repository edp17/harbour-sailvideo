/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "systemaudiocontroller.h"

#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

SystemAudioController::SystemAudioController(QObject *parent)
    : QObject(parent)
    , m_pactlPath(QStandardPaths::findExecutable(QStringLiteral("pactl")))
{
    if (m_pactlPath.isEmpty() && QFile::exists(QStringLiteral("/usr/bin/pactl"))) {
        m_pactlPath = QStringLiteral("/usr/bin/pactl");
    }

    if (m_pactlPath.isEmpty()) {
        qWarning() << "SailVideo: pactl was not found; audio gestures fall back to QMediaPlayer only";
    }
}

QString SystemAudioController::lastError() const
{
    return m_lastError;
}

bool SystemAudioController::available() const
{
    return !m_pactlPath.isEmpty();
}

bool SystemAudioController::setMuted(bool muted)
{
    if (m_pactlPath.isEmpty()) {
        setLastError(QStringLiteral("pactl is not available"));
        return false;
    }

    const bool ok = runPactlDetached(QStringList()
                                     << QStringLiteral("set-sink-mute")
                                     << QStringLiteral("@DEFAULT_SINK@")
                                     << (muted ? QStringLiteral("1") : QStringLiteral("0")));
    if (ok) {
        setLastError(QString());
    }
    return ok;
}

bool SystemAudioController::setVolumePercent(int percent)
{
    if (m_pactlPath.isEmpty()) {
        setLastError(QStringLiteral("pactl is not available"));
        return false;
    }

    const int bounded = qMax(0, qMin(100, percent));
    const bool volumeOk = runPactlDetached(QStringList()
                                           << QStringLiteral("set-sink-volume")
                                           << QStringLiteral("@DEFAULT_SINK@")
                                           << QStringLiteral("%1%").arg(bounded));
    const bool muteOk = runPactlDetached(QStringList()
                                         << QStringLiteral("set-sink-mute")
                                         << QStringLiteral("@DEFAULT_SINK@")
                                         << (bounded <= 0 ? QStringLiteral("1") : QStringLiteral("0")));

    const bool ok = volumeOk && muteOk;
    if (ok) {
        setLastError(QString());
    }
    return ok;
}

bool SystemAudioController::runPactlDetached(const QStringList &arguments) const
{
    if (!QProcess::startDetached(m_pactlPath, arguments)) {
        setLastError(QStringLiteral("Could not start pactl"));
        qWarning() << "SailVideo: could not start pactl" << arguments;
        return false;
    }

    return true;
}

void SystemAudioController::setLastError(const QString &message) const
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit const_cast<SystemAudioController *>(this)->lastErrorChanged();
}
