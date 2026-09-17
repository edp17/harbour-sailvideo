/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QString>
#include <QTimer>

class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
    Q_PROPERTY(QString videoFillMode READ videoFillMode WRITE setVideoFillMode NOTIFY videoFillModeChanged)
    Q_PROPERTY(int skipSeconds READ skipSeconds WRITE setSkipSeconds NOTIFY skipSecondsChanged)
    Q_PROPERTY(int pictureSlideshowSeconds READ pictureSlideshowSeconds WRITE setPictureSlideshowSeconds NOTIFY pictureSlideshowSecondsChanged)
    Q_PROPERTY(bool keepDisplayOn READ keepDisplayOn WRITE setKeepDisplayOn NOTIFY keepDisplayOnChanged)
    Q_PROPERTY(bool nasShowOnlyVideos READ nasShowOnlyVideos WRITE setNasShowOnlyVideos NOTIFY nasShowOnlyVideosChanged)
    Q_PROPERTY(bool nasShowHiddenFiles READ nasShowHiddenFiles WRITE setNasShowHiddenFiles NOTIFY nasShowHiddenFilesChanged)
    Q_PROPERTY(int defaultVolumePercent READ defaultVolumePercent WRITE setDefaultVolumePercent NOTIFY defaultVolumePercentChanged)
    Q_PROPERTY(int defaultBrightnessPercent READ defaultBrightnessPercent WRITE setDefaultBrightnessPercent NOTIFY defaultBrightnessPercentChanged)
    Q_PROPERTY(QString lastNasSourceTitle READ lastNasSourceTitle NOTIFY lastNasSourceChanged)
    Q_PROPERTY(QString lastNasHost READ lastNasHost NOTIFY lastNasSourceChanged)
    Q_PROPERTY(int lastNasPort READ lastNasPort NOTIFY lastNasSourceChanged)
    Q_PROPERTY(QString lastNasShare READ lastNasShare NOTIFY lastNasSourceChanged)
    Q_PROPERTY(QString lastNasPath READ lastNasPath NOTIFY lastNasSourceChanged)
    Q_PROPERTY(QString lastNasDomain READ lastNasDomain NOTIFY lastNasSourceChanged)
    Q_PROPERTY(QString lastNasUsername READ lastNasUsername NOTIFY lastNasSourceChanged)
    Q_PROPERTY(bool lastNasGuest READ lastNasGuest NOTIFY lastNasSourceChanged)
    Q_PROPERTY(bool hasLastNasSource READ hasLastNasSource NOTIFY lastNasSourceChanged)
    Q_PROPERTY(QString detachedCastDeviceName READ detachedCastDeviceName NOTIFY detachedCastSessionChanged)
    Q_PROPERTY(QString detachedCastHost READ detachedCastHost NOTIFY detachedCastSessionChanged)
    Q_PROPERTY(int detachedCastPort READ detachedCastPort NOTIFY detachedCastSessionChanged)
    Q_PROPERTY(bool hasDetachedCastSession READ hasDetachedCastSession NOTIFY detachedCastSessionChanged)

public:
    explicit AppSettings(const QString &storageDirectory = QString(), QObject *parent = nullptr);
    ~AppSettings() override = default;

    QString storagePath() const;
    QString videoFillMode() const;
    void setVideoFillMode(const QString &mode);
    int skipSeconds() const;
    void setSkipSeconds(int seconds);
    int pictureSlideshowSeconds() const;
    void setPictureSlideshowSeconds(int seconds);
    bool keepDisplayOn() const;
    void setKeepDisplayOn(bool enabled);
    bool nasShowOnlyVideos() const;
    void setNasShowOnlyVideos(bool enabled);
    bool nasShowHiddenFiles() const;
    void setNasShowHiddenFiles(bool enabled);
    int defaultVolumePercent() const;
    void setDefaultVolumePercent(int percent);
    int defaultBrightnessPercent() const;
    void setDefaultBrightnessPercent(int percent);

    QString lastNasSourceTitle() const;
    QString lastNasHost() const;
    int lastNasPort() const;
    QString lastNasShare() const;
    QString lastNasPath() const;
    QString lastNasDomain() const;
    QString lastNasUsername() const;
    bool lastNasGuest() const;
    bool hasLastNasSource() const;

    QString detachedCastDeviceName() const;
    QString detachedCastHost() const;
    int detachedCastPort() const;
    bool hasDetachedCastSession() const;

    Q_INVOKABLE void setLastNasSource(const QString &title,
                                      const QString &host,
                                      int port,
                                      const QString &share,
                                      const QString &path,
                                      const QString &domain,
                                      const QString &username,
                                      bool guest);
    Q_INVOKABLE void clearLastNasSource();
    Q_INVOKABLE void setDetachedCastSession(const QString &deviceName,
                                            const QString &host,
                                            int port);
    Q_INVOKABLE void clearDetachedCastSession();
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE QString videoFillModeDescription() const;

    // Used by main.cpp to flush the delayed settings write on shutdown.
    // This is intentionally not Q_INVOKABLE.
    bool save() const;

signals:
    void videoFillModeChanged();
    void skipSecondsChanged();
    void pictureSlideshowSecondsChanged();
    void keepDisplayOnChanged();
    void nasShowOnlyVideosChanged();
    void nasShowHiddenFilesChanged();
    void defaultVolumePercentChanged();
    void defaultBrightnessPercentChanged();
    void lastNasSourceChanged();
    void detachedCastSessionChanged();

private:
    void load();
    void scheduleSave();
    static QString normalizedFillMode(const QString &mode);
    static int normalizedSkipSeconds(int seconds);
    static int normalizedPictureSlideshowSeconds(int seconds);
    static int normalizedPercent(int percent, int minimum, int fallback);
    static QString cleanedText(const QString &value);
    static QString normalizedPath(const QString &path);

    QString m_storageDirectory;
    QTimer m_saveTimer;
    QString m_videoFillMode = QStringLiteral("fit");
    int m_skipSeconds = 10;
    int m_pictureSlideshowSeconds = 5;
    bool m_keepDisplayOn = true;
    bool m_nasShowOnlyVideos = false;
    bool m_nasShowHiddenFiles = false;
    int m_defaultVolumePercent = 30;
    int m_defaultBrightnessPercent = 50;
    QString m_lastNasSourceTitle;
    QString m_lastNasHost;
    int m_lastNasPort = 445;
    QString m_lastNasShare;
    QString m_lastNasPath;
    QString m_lastNasDomain;
    QString m_lastNasUsername;
    bool m_lastNasGuest = true;
    QString m_detachedCastDeviceName;
    QString m_detachedCastHost;
    int m_detachedCastPort = 8009;
};

#endif // APPSETTINGS_H
