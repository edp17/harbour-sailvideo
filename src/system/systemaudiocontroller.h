/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef SYSTEMAUDIOCONTROLLER_H
#define SYSTEMAUDIOCONTROLLER_H

#include <QObject>
#include <QString>

class SystemAudioController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit SystemAudioController(QObject *parent = nullptr);

    QString lastError() const;
    bool available() const;

    Q_INVOKABLE bool setMuted(bool muted);
    Q_INVOKABLE bool setVolumePercent(int percent);

signals:
    void lastErrorChanged();

private:
    bool runPactlDetached(const QStringList &arguments) const;
    void setLastError(const QString &message) const;

    mutable QString m_lastError;
    QString m_pactlPath;
};

#endif // SYSTEMAUDIOCONTROLLER_H
