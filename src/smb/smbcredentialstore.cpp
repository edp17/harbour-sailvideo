
/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "smbcredentialstore.h"

#include "smbbackend.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QStringList>

#include <Secrets/collectionnamesrequest.h>
#include <Secrets/createcollectionrequest.h>
#include <Secrets/deletesecretrequest.h>
#include <Secrets/result.h>
#include <Secrets/secret.h>
#include <Secrets/storedsecretrequest.h>
#include <Secrets/storesecretrequest.h>

using namespace Sailfish::Secrets;

namespace {

const int DefaultSmbPort = 445;

bool succeeded(const Result &result)
{
    return result.code() == Result::Succeeded;
}

QString resultMessage(const Result &result)
{
    const QString message = result.errorMessage().trimmed();
    return message.isEmpty()
            ? QObject::tr("Unknown Sailfish Secrets error.")
            : message;
}

} // namespace

SmbCredentialStore::SmbCredentialStore(QObject *parent)
    : QObject(parent)
{
    connect(&m_manager, SIGNAL(isInitializedChanged()),
            this, SLOT(updateAvailability()));
    updateAvailability();
}

bool SmbCredentialStore::available() const
{
    return m_available;
}

QString SmbCredentialStore::lastError() const
{
    return m_lastError;
}

QString SmbCredentialStore::statusText() const
{
    return m_statusText;
}

QString SmbCredentialStore::passwordFor(const QString &host,
                                        int port,
                                        const QString &share,
                                        const QString &domain,
                                        const QString &username,
                                        bool guest)
{
    if (guest) {
        return QString();
    }

    QString collectionError;
    if (!collectionExists(&collectionError)) {
        if (!collectionError.isEmpty()) {
            setLastError(tr("Could not inspect Sailfish Secrets collection: %1").arg(collectionError));
        }
        return QString();
    }

    StoredSecretRequest request;
    request.setManager(&m_manager);
    request.setIdentifier(Secret::Identifier(secretNameFor(host, port, share, domain, username, guest),
                                             collectionName(),
                                             storagePluginName()));
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished();

    if (!succeeded(request.result())) {
        // Missing passwords are normal for new NAS sources. Do not make that a
        // hard user-visible error unless the user explicitly tries to save one.
        qWarning() << "SailVideo: could not retrieve SMB password:"
                   << request.result().errorMessage();
        return QString();
    }

    setLastError(QString());
    return QString::fromUtf8(request.secret().data());
}

bool SmbCredentialStore::storePasswordFor(const QString &host,
                                          int port,
                                          const QString &share,
                                          const QString &domain,
                                          const QString &username,
                                          bool guest,
                                          const QString &password)
{
    if (guest || password.isEmpty()) {
        return deletePasswordFor(host, port, share, domain, username, guest);
    }

    if (!ensureCollection()) {
        return false;
    }

    // Updating an existing secret can fail with a uniqueness error on some
    // Sailfish Secrets versions, so delete first and then write the fresh value.
    deletePasswordFor(host, port, share, domain, username, guest);

    Secret secret(Secret::Identifier(secretNameFor(host, port, share, domain, username, guest),
                                     collectionName(),
                                     storagePluginName()));
    secret.setData(password.toUtf8());
    secret.setType(Secret::TypeBlob);
    secret.setFilterData(QLatin1String("application"), QLatin1String("SailVideo"));
    secret.setFilterData(QLatin1String("kind"), QLatin1String("smb-password"));
    secret.setFilterData(QLatin1String("host"), SmbBackend::normalizedHost(host));
    secret.setFilterData(QLatin1String("share"), SmbBackend::normalizedShare(share));
    secret.setFilterData(QLatin1String("username"), SmbBackend::cleanedText(username));

    StoreSecretRequest request;
    request.setManager(&m_manager);
    request.setSecretStorageType(StoreSecretRequest::CollectionSecret);
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.setSecret(secret);
    request.startRequest();
    request.waitForFinished();

    if (!succeeded(request.result())) {
        setLastError(tr("Could not save NAS password in Sailfish Secrets: %1")
                     .arg(resultMessage(request.result())));
        return false;
    }

    setLastError(QString());
    return true;
}

bool SmbCredentialStore::deletePasswordFor(const QString &host,
                                           int port,
                                           const QString &share,
                                           const QString &domain,
                                           const QString &username,
                                           bool guest)
{
    Q_UNUSED(guest)

    QString collectionError;
    if (!collectionExists(&collectionError)) {
        if (!collectionError.isEmpty()) {
            qWarning() << "SailVideo: could not inspect SMB credential collection before delete:"
                       << collectionError;
        }
        return true;
    }

    DeleteSecretRequest request;
    request.setManager(&m_manager);
    request.setIdentifier(Secret::Identifier(secretNameFor(host, port, share, domain, username, false),
                                             collectionName(),
                                             storagePluginName()));
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished();

    if (!succeeded(request.result())) {
        // Deleting a non-existent secret is harmless during updates/removals.
        qWarning() << "SailVideo: could not delete SMB password:"
                   << request.result().errorMessage();
    }

    setLastError(QString());
    return true;
}

QString SmbCredentialStore::secretNameFor(const QString &host,
                                          int port,
                                          const QString &share,
                                          const QString &domain,
                                          const QString &username,
                                          bool guest) const
{
    const QByteArray digest = QCryptographicHash::hash(
                normalizedKeyMaterial(host, port, share, domain, username, guest).toUtf8(),
                QCryptographicHash::Sha256).toHex();
    return QStringLiteral("smb-") + QString::fromLatin1(digest);
}

void SmbCredentialStore::updateAvailability()
{
    const bool nowAvailable = m_manager.isInitialized();
    if (m_available != nowAvailable) {
        m_available = nowAvailable;
        emit availableChanged();
    }
    updateStatusText();
}

bool SmbCredentialStore::ensureCollection()
{
    if (m_collectionEnsured) {
        return true;
    }

    QString errorString;
    if (collectionExists(&errorString)) {
        m_collectionEnsured = true;
        return true;
    }

    if (!errorString.isEmpty()) {
        setLastError(tr("Could not inspect Sailfish Secrets collection: %1").arg(errorString));
        return false;
    }

    CreateCollectionRequest request;
    request.setManager(&m_manager);
    request.setCollectionName(collectionName());
    request.setAccessControlMode(SecretManager::OwnerOnlyMode);
    request.setCollectionLockType(CreateCollectionRequest::DeviceLock);
    request.setDeviceLockUnlockSemantic(SecretManager::DeviceLockKeepUnlocked);
    request.setStoragePluginName(storagePluginName());
    request.setEncryptionPluginName(storagePluginName());
    request.setUserInteractionMode(SecretManager::SystemInteraction);
    request.startRequest();
    request.waitForFinished();

    if (!succeeded(request.result())) {
        setLastError(tr("Could not create Sailfish Secrets collection: %1")
                     .arg(resultMessage(request.result())));
        return false;
    }

    m_collectionEnsured = true;
    setLastError(QString());
    return true;
}

bool SmbCredentialStore::collectionExists(QString *errorString)
{
    CollectionNamesRequest request;
    request.setManager(&m_manager);
    request.setStoragePluginName(storagePluginName());
    request.startRequest();
    request.waitForFinished();

    if (!succeeded(request.result())) {
        if (errorString) {
            *errorString = resultMessage(request.result());
        }
        return false;
    }

    if (errorString) {
        errorString->clear();
    }
    return request.collectionNames().contains(collectionName());
}

void SmbCredentialStore::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
    updateStatusText();
}

void SmbCredentialStore::updateStatusText()
{
    const QString text = m_lastError.isEmpty()
            ? (m_available
               ? tr("Sailfish Secrets available")
               : tr("Sailfish Secrets initializing"))
            : m_lastError;
    if (m_statusText == text) {
        return;
    }
    m_statusText = text;
    emit statusTextChanged();
}

QString SmbCredentialStore::normalizedKeyMaterial(const QString &host,
                                                  int port,
                                                  const QString &share,
                                                  const QString &domain,
                                                  const QString &username,
                                                  bool guest)
{
    const QString cleanHost = SmbBackend::normalizedHost(host).toLower();
    const QString cleanShare = SmbBackend::normalizedShare(share);
    const QString cleanDomain = SmbBackend::cleanedText(domain).toLower();
    const QString cleanUser = guest
            ? QStringLiteral("guest")
            : SmbBackend::cleanedText(username);

    QStringList parts;
    parts << QStringLiteral("smb")
          << cleanHost
          << QString::number(port > 0 ? port : DefaultSmbPort)
          << cleanShare
          << cleanDomain
          << cleanUser
          << (guest ? QStringLiteral("guest") : QStringLiteral("user"));
    return parts.join(QLatin1Char('|'));
}

QString SmbCredentialStore::collectionName()
{
    return QStringLiteral("SailVideoSmbCredentials");
}

QString SmbCredentialStore::storagePluginName()
{
    return SecretManager::DefaultEncryptedStoragePluginName;
}
