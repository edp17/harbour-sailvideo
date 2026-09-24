/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "appsettings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

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

} // namespace

AppSettings::AppSettings(const QString &storageDirectory, QObject *parent)
    : QObject(parent)
    , m_storageDirectory(storageDirectory.trimmed().isEmpty()
                         ? defaultStorageDirectory()
                         : storageDirectory.trimmed())
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() { save(); });
    load();
}

QString AppSettings::storagePath() const
{
    return QDir(m_storageDirectory).filePath(QStringLiteral("settings.json"));
}

QString AppSettings::videoFillMode() const
{
    return m_videoFillMode;
}

void AppSettings::setVideoFillMode(const QString &mode)
{
    const QString cleanMode = normalizedFillMode(mode);
    if (m_videoFillMode == cleanMode) {
        return;
    }
    m_videoFillMode = cleanMode;
    emit videoFillModeChanged();
    scheduleSave();
}

int AppSettings::skipSeconds() const
{
    return m_skipSeconds;
}

void AppSettings::setSkipSeconds(int seconds)
{
    const int cleanSeconds = normalizedSkipSeconds(seconds);
    if (m_skipSeconds == cleanSeconds) {
        return;
    }
    m_skipSeconds = cleanSeconds;
    emit skipSecondsChanged();
    scheduleSave();
}

int AppSettings::pictureSlideshowSeconds() const
{
    return m_pictureSlideshowSeconds;
}

void AppSettings::setPictureSlideshowSeconds(int seconds)
{
    const int cleanSeconds = normalizedPictureSlideshowSeconds(seconds);
    if (m_pictureSlideshowSeconds == cleanSeconds) {
        return;
    }
    m_pictureSlideshowSeconds = cleanSeconds;
    emit pictureSlideshowSecondsChanged();
    scheduleSave();
}

bool AppSettings::keepDisplayOn() const
{
    return m_keepDisplayOn;
}

void AppSettings::setKeepDisplayOn(bool enabled)
{
    if (m_keepDisplayOn == enabled) {
        return;
    }
    m_keepDisplayOn = enabled;
    emit keepDisplayOnChanged();
    scheduleSave();
}

bool AppSettings::diagnosticsEnabled() const
{
    return m_diagnosticsEnabled;
}

void AppSettings::setDiagnosticsEnabled(bool enabled)
{
    if (m_diagnosticsEnabled == enabled) {
        return;
    }
    m_diagnosticsEnabled = enabled;
    emit diagnosticsEnabledChanged();
    scheduleSave();
}

bool AppSettings::nasShowOnlyVideos() const
{
    return m_nasShowOnlyVideos;
}

void AppSettings::setNasShowOnlyVideos(bool enabled)
{
    if (m_nasShowOnlyVideos == enabled) {
        return;
    }
    m_nasShowOnlyVideos = enabled;
    emit nasShowOnlyVideosChanged();
    scheduleSave();
}

bool AppSettings::nasShowHiddenFiles() const
{
    return m_nasShowHiddenFiles;
}

void AppSettings::setNasShowHiddenFiles(bool enabled)
{
    if (m_nasShowHiddenFiles == enabled) {
        return;
    }
    m_nasShowHiddenFiles = enabled;
    emit nasShowHiddenFilesChanged();
    scheduleSave();
}

int AppSettings::defaultVolumePercent() const
{
    return m_defaultVolumePercent;
}

void AppSettings::setDefaultVolumePercent(int percent)
{
    const int cleanPercent = normalizedPercent(percent, 0, 30);
    if (m_defaultVolumePercent == cleanPercent) {
        return;
    }
    m_defaultVolumePercent = cleanPercent;
    emit defaultVolumePercentChanged();
    scheduleSave();
}

int AppSettings::defaultBrightnessPercent() const
{
    return m_defaultBrightnessPercent;
}

void AppSettings::setDefaultBrightnessPercent(int percent)
{
    const int cleanPercent = normalizedPercent(percent, 20, 50);
    if (m_defaultBrightnessPercent == cleanPercent) {
        return;
    }
    m_defaultBrightnessPercent = cleanPercent;
    emit defaultBrightnessPercentChanged();
    scheduleSave();
}

QString AppSettings::lastNasSourceTitle() const
{
    return m_lastNasSourceTitle;
}

QString AppSettings::lastNasHost() const
{
    return m_lastNasHost;
}

int AppSettings::lastNasPort() const
{
    return m_lastNasPort > 0 ? m_lastNasPort : 445;
}

QString AppSettings::lastNasShare() const
{
    return m_lastNasShare;
}

QString AppSettings::lastNasPath() const
{
    return m_lastNasPath;
}

QString AppSettings::lastNasDomain() const
{
    return m_lastNasDomain;
}

QString AppSettings::lastNasUsername() const
{
    return m_lastNasUsername;
}

bool AppSettings::lastNasGuest() const
{
    return m_lastNasGuest;
}

bool AppSettings::hasLastNasSource() const
{
    return !m_lastNasHost.isEmpty() && !m_lastNasShare.isEmpty();
}

QString AppSettings::detachedCastDeviceName() const
{
    return m_detachedCastDeviceName;
}

QString AppSettings::detachedCastHost() const
{
    return m_detachedCastHost;
}

int AppSettings::detachedCastPort() const
{
    return m_detachedCastPort > 0 ? m_detachedCastPort : 8009;
}

bool AppSettings::hasDetachedCastSession() const
{
    return !m_detachedCastHost.isEmpty();
}

void AppSettings::setLastNasSource(const QString &title,
                                   const QString &host,
                                   int port,
                                   const QString &share,
                                   const QString &path,
                                   const QString &domain,
                                   const QString &username,
                                   bool guest)
{
    const QString cleanHost = cleanedText(host);
    const QString cleanShare = cleanedText(share);
    if (cleanHost.isEmpty() || cleanShare.isEmpty()) {
        return;
    }

    const QString cleanTitle = cleanedText(title).isEmpty()
            ? QStringLiteral("%1/%2").arg(cleanHost, cleanShare)
            : cleanedText(title);
    const int cleanPort = port > 0 ? port : 445;
    const QString cleanPath = normalizedPath(path);
    const QString cleanDomain = cleanedText(domain);
    const QString cleanUsername = cleanedText(username);

    if (m_lastNasSourceTitle == cleanTitle
            && m_lastNasHost == cleanHost
            && m_lastNasPort == cleanPort
            && m_lastNasShare == cleanShare
            && m_lastNasPath == cleanPath
            && m_lastNasDomain == cleanDomain
            && m_lastNasUsername == cleanUsername
            && m_lastNasGuest == guest) {
        return;
    }

    m_lastNasSourceTitle = cleanTitle;
    m_lastNasHost = cleanHost;
    m_lastNasPort = cleanPort;
    m_lastNasShare = cleanShare;
    m_lastNasPath = cleanPath;
    m_lastNasDomain = cleanDomain;
    m_lastNasUsername = cleanUsername;
    m_lastNasGuest = guest;
    emit lastNasSourceChanged();
    scheduleSave();
}

void AppSettings::clearLastNasSource()
{
    if (!hasLastNasSource()
            && m_lastNasSourceTitle.isEmpty()
            && m_lastNasPath.isEmpty()
            && m_lastNasDomain.isEmpty()
            && m_lastNasUsername.isEmpty()) {
        return;
    }

    m_lastNasSourceTitle.clear();
    m_lastNasHost.clear();
    m_lastNasPort = 445;
    m_lastNasShare.clear();
    m_lastNasPath.clear();
    m_lastNasDomain.clear();
    m_lastNasUsername.clear();
    m_lastNasGuest = true;
    emit lastNasSourceChanged();
    scheduleSave();
}

void AppSettings::setDetachedCastSession(const QString &deviceName,
                                         const QString &host,
                                         int port)
{
    const QString cleanHost = cleanedText(host);
    if (cleanHost.isEmpty()) {
        return;
    }

    const QString cleanName = cleanedText(deviceName);
    const int cleanPort = port > 0 ? port : 8009;
    if (m_detachedCastDeviceName == cleanName
            && m_detachedCastHost == cleanHost
            && m_detachedCastPort == cleanPort) {
        return;
    }

    m_detachedCastDeviceName = cleanName;
    m_detachedCastHost = cleanHost;
    m_detachedCastPort = cleanPort;
    emit detachedCastSessionChanged();

    // Persist immediately because this state is specifically needed after
    // SailVideo has been closed while the receiver keeps playing.
    save();
}

void AppSettings::clearDetachedCastSession()
{
    if (!hasDetachedCastSession() && m_detachedCastDeviceName.isEmpty()) {
        return;
    }

    m_detachedCastDeviceName.clear();
    m_detachedCastHost.clear();
    m_detachedCastPort = 8009;
    emit detachedCastSessionChanged();
    save();
}

void AppSettings::resetToDefaults()
{
    const bool fillChanged = m_videoFillMode != QLatin1String("fit");
    const bool skipChanged = m_skipSeconds != 10;
    const bool slideshowChanged = m_pictureSlideshowSeconds != 5;
    const bool keepChanged = !m_keepDisplayOn;
    const bool diagnosticsChanged = m_diagnosticsEnabled;
    const bool videosChanged = m_nasShowOnlyVideos;
    const bool hiddenChanged = m_nasShowHiddenFiles;
    const bool lastNasChanged = hasLastNasSource();
    const bool castSessionChanged = hasDetachedCastSession();
    const bool volumeChanged = m_defaultVolumePercent != 30;
    const bool brightnessChanged = m_defaultBrightnessPercent != 50;

    m_videoFillMode = QStringLiteral("fit");
    m_skipSeconds = 10;
    m_pictureSlideshowSeconds = 5;
    m_keepDisplayOn = true;
    m_diagnosticsEnabled = false;
    m_nasShowOnlyVideos = false;
    m_nasShowHiddenFiles = false;
    m_defaultVolumePercent = 30;
    m_defaultBrightnessPercent = 50;
    m_lastNasSourceTitle.clear();
    m_lastNasHost.clear();
    m_lastNasPort = 445;
    m_lastNasShare.clear();
    m_lastNasPath.clear();
    m_lastNasDomain.clear();
    m_lastNasUsername.clear();
    m_lastNasGuest = true;
    m_detachedCastDeviceName.clear();
    m_detachedCastHost.clear();
    m_detachedCastPort = 8009;

    if (fillChanged) emit videoFillModeChanged();
    if (skipChanged) emit skipSecondsChanged();
    if (slideshowChanged) emit pictureSlideshowSecondsChanged();
    if (keepChanged) emit keepDisplayOnChanged();
    if (diagnosticsChanged) emit diagnosticsEnabledChanged();
    if (videosChanged) emit nasShowOnlyVideosChanged();
    if (hiddenChanged) emit nasShowHiddenFilesChanged();
    if (volumeChanged) emit defaultVolumePercentChanged();
    if (brightnessChanged) emit defaultBrightnessPercentChanged();
    if (lastNasChanged) emit lastNasSourceChanged();
    if (castSessionChanged) emit detachedCastSessionChanged();
    scheduleSave();
}

QString AppSettings::videoFillModeDescription() const
{
    if (m_videoFillMode == QLatin1String("crop")) {
        return tr("Fill screen by cropping the video edges if needed.");
    }
    if (m_videoFillMode == QLatin1String("stretch")) {
        return tr("Stretch video to the screen size. This may distort the picture.");
    }
    return tr("Fit the whole video on screen without cropping.");
}

void AppSettings::load()
{
    QFile file(storagePath());
    if (!file.exists()) {
        scheduleSave();
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "SailVideo: failed to open settings store" << storagePath() << file.errorString();
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "SailVideo: failed to parse settings store" << storagePath() << parseError.errorString();
        return;
    }
    const QJsonObject root = document.object();
    m_videoFillMode = normalizedFillMode(root.value(QStringLiteral("videoFillMode")).toString(QStringLiteral("fit")));
    m_skipSeconds = normalizedSkipSeconds(root.value(QStringLiteral("skipSeconds")).toInt(10));
    m_pictureSlideshowSeconds = normalizedPictureSlideshowSeconds(
                root.value(QStringLiteral("pictureSlideshowSeconds")).toInt(5));
    m_keepDisplayOn = root.value(QStringLiteral("keepDisplayOn")).toBool(true);
    m_diagnosticsEnabled = root.value(QStringLiteral("diagnosticsEnabled")).toBool(false);
    m_nasShowOnlyVideos = root.value(QStringLiteral("nasShowOnlyVideos")).toBool(false);
    m_nasShowHiddenFiles = root.value(QStringLiteral("nasShowHiddenFiles")).toBool(false);
    m_defaultVolumePercent = normalizedPercent(root.value(QStringLiteral("defaultVolumePercent")).toInt(30), 0, 30);
    m_defaultBrightnessPercent = normalizedPercent(root.value(QStringLiteral("defaultBrightnessPercent")).toInt(50), 20, 50);
    m_lastNasSourceTitle = cleanedText(root.value(QStringLiteral("lastNasSourceTitle")).toString());
    m_lastNasHost = cleanedText(root.value(QStringLiteral("lastNasHost")).toString());
    m_lastNasPort = root.value(QStringLiteral("lastNasPort")).toInt(445);
    if (m_lastNasPort <= 0) {
        m_lastNasPort = 445;
    }
    m_lastNasShare = cleanedText(root.value(QStringLiteral("lastNasShare")).toString());
    m_lastNasPath = normalizedPath(root.value(QStringLiteral("lastNasPath")).toString());
    m_lastNasDomain = cleanedText(root.value(QStringLiteral("lastNasDomain")).toString());
    m_lastNasUsername = cleanedText(root.value(QStringLiteral("lastNasUsername")).toString());
    m_lastNasGuest = root.value(QStringLiteral("lastNasGuest")).toBool(true);
    m_detachedCastDeviceName = cleanedText(root.value(QStringLiteral("detachedCastDeviceName")).toString());
    m_detachedCastHost = cleanedText(root.value(QStringLiteral("detachedCastHost")).toString());
    m_detachedCastPort = root.value(QStringLiteral("detachedCastPort")).toInt(8009);
    if (m_detachedCastPort <= 0) {
        m_detachedCastPort = 8009;
    }
}

void AppSettings::scheduleSave()
{
    if (!m_saveTimer.isActive()) {
        m_saveTimer.start();
    }
}

bool AppSettings::save() const
{
    QDir directory(m_storageDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        qWarning() << "SailVideo: failed to create settings directory" << m_storageDirectory;
        return false;
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 5);
    root.insert(QStringLiteral("videoFillMode"), m_videoFillMode);
    root.insert(QStringLiteral("skipSeconds"), m_skipSeconds);
    root.insert(QStringLiteral("pictureSlideshowSeconds"), m_pictureSlideshowSeconds);
    root.insert(QStringLiteral("keepDisplayOn"), m_keepDisplayOn);
    root.insert(QStringLiteral("diagnosticsEnabled"), m_diagnosticsEnabled);
    root.insert(QStringLiteral("nasShowOnlyVideos"), m_nasShowOnlyVideos);
    root.insert(QStringLiteral("nasShowHiddenFiles"), m_nasShowHiddenFiles);
    root.insert(QStringLiteral("defaultVolumePercent"), m_defaultVolumePercent);
    root.insert(QStringLiteral("defaultBrightnessPercent"), m_defaultBrightnessPercent);
    root.insert(QStringLiteral("lastNasSourceTitle"), m_lastNasSourceTitle);
    root.insert(QStringLiteral("lastNasHost"), m_lastNasHost);
    root.insert(QStringLiteral("lastNasPort"), m_lastNasPort);
    root.insert(QStringLiteral("lastNasShare"), m_lastNasShare);
    root.insert(QStringLiteral("lastNasPath"), m_lastNasPath);
    root.insert(QStringLiteral("lastNasDomain"), m_lastNasDomain);
    root.insert(QStringLiteral("lastNasUsername"), m_lastNasUsername);
    root.insert(QStringLiteral("lastNasGuest"), m_lastNasGuest);
    root.insert(QStringLiteral("detachedCastDeviceName"), m_detachedCastDeviceName);
    root.insert(QStringLiteral("detachedCastHost"), m_detachedCastHost);
    root.insert(QStringLiteral("detachedCastPort"), m_detachedCastPort);

    QSaveFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "SailVideo: failed to open settings store for writing" << storagePath() << file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning() << "SailVideo: failed to commit settings store" << storagePath() << file.errorString();
        return false;
    }
    qDebug() << "SailVideo: settings saved to" << storagePath();
    return true;
}

QString AppSettings::normalizedFillMode(const QString &mode)
{
    const QString clean = mode.trimmed().toLower();
    if (clean == QLatin1String("crop") || clean == QLatin1String("stretch")) {
        return clean;
    }
    return QStringLiteral("fit");
}

int AppSettings::normalizedSkipSeconds(int seconds)
{
    if (seconds <= 7) return 5;
    if (seconds <= 20) return 10;
    if (seconds <= 45) return 30;
    if (seconds <= 90) return 60;
    return 120;
}

int AppSettings::normalizedPictureSlideshowSeconds(int seconds)
{
    if (seconds <= 3) return 3;
    if (seconds <= 5) return 5;
    if (seconds <= 10) return 10;
    if (seconds <= 15) return 15;
    return 30;
}

int AppSettings::normalizedPercent(int percent, int minimum, int fallback)
{
    if (percent < minimum || percent > 100) {
        return fallback;
    }
    return percent;
}

QString AppSettings::cleanedText(const QString &value)
{
    return value.trimmed();
}

QString AppSettings::normalizedPath(const QString &path)
{
    QString text = cleanedText(path);
    text.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (text.startsWith(QLatin1Char('/'))) {
        text.remove(0, 1);
    }
    while (text.endsWith(QLatin1Char('/'))) {
        text.chop(1);
    }

    QStringList parts;
    const QStringList rawParts = text.split(QLatin1Char('/'), QString::SkipEmptyParts);
    for (int i = 0; i < rawParts.size(); ++i) {
        const QString part = rawParts.at(i).trimmed();
        if (part.isEmpty() || part == QLatin1String(".")) {
            continue;
        }
        if (part == QLatin1String("..")) {
            if (!parts.isEmpty()) {
                parts.removeLast();
            }
            continue;
        }
        parts.append(part);
    }
    return parts.join(QLatin1Char('/'));
}
