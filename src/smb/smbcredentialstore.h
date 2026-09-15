
/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef SMBCREDENTIALSTORE_H
#define SMBCREDENTIALSTORE_H

#include <QObject>
#include <QString>

#include <Secrets/secretmanager.h>

class SmbCredentialStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit SmbCredentialStore(QObject *parent = nullptr);

    bool available() const;
    QString lastError() const;
    QString statusText() const;

    Q_INVOKABLE QString passwordFor(const QString &host,
                                    int port,
                                    const QString &share,
                                    const QString &domain,
                                    const QString &username,
                                    bool guest);
    Q_INVOKABLE bool storePasswordFor(const QString &host,
                                      int port,
                                      const QString &share,
                                      const QString &domain,
                                      const QString &username,
                                      bool guest,
                                      const QString &password);
    Q_INVOKABLE bool deletePasswordFor(const QString &host,
                                       int port,
                                       const QString &share,
                                       const QString &domain,
                                       const QString &username,
                                       bool guest);
    Q_INVOKABLE QString secretNameFor(const QString &host,
                                      int port,
                                      const QString &share,
                                      const QString &domain,
                                      const QString &username,
                                      bool guest) const;

signals:
    void availableChanged();
    void lastErrorChanged();
    void statusTextChanged();

private slots:
    void updateAvailability();

private:
    bool ensureCollection();
    bool collectionExists(QString *errorString = nullptr);
    void setLastError(const QString &message);
    void updateStatusText();

    static QString normalizedKeyMaterial(const QString &host,
                                         int port,
                                         const QString &share,
                                         const QString &domain,
                                         const QString &username,
                                         bool guest);
    static QString collectionName();
    static QString storagePluginName();

    Sailfish::Secrets::SecretManager m_manager;
    bool m_collectionEnsured = false;
    bool m_available = false;
    QString m_lastError;
    QString m_statusText;
};

#endif // SMBCREDENTIALSTORE_H
