/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo.
 *
 * SailVideo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * SailVideo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <QByteArray>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <QUrl>
#include <QtQml>

#include <sailfishapp.h>

#include "application/appsettings.h"
#include "cast/castdevicemodel.h"
#include "cast/castmanager.h"
#include "cast/castmediapreparer.h"
#include "media/playbackhistorymodel.h"
#include "media/localvideomodel.h"
#include "media/localvideocategorymodel.h"
#include "media/localvideofoldermodel.h"
#include "sources/networksourcemodel.h"
#include "sources/networkdiscoverymodel.h"
#include "sources/nassourcemodel.h"
#include "smb/smbbackend.h"
#include "smb/smbcredentialstore.h"
#include "system/systemaudiocontroller.h"
#include "stream/localfilestreamserver.h"
#include "stream/urldownloadcache.h"

namespace {

QUrl commandLineMediaUrl(const QStringList &arguments)
{
    if (arguments.size() < 2) {
        return QUrl();
    }

    const QString argument = arguments.at(1).trimmed();
    if (argument.isEmpty()) {
        return QUrl();
    }

    const QUrl candidate(argument);
    if (candidate.isValid() && !candidate.scheme().isEmpty()) {
        return candidate;
    }

    const QFileInfo fileInfo(argument);
    return QUrl::fromLocalFile(fileInfo.absoluteFilePath());
}

} // namespace

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));

    // Route QtMultimedia through Sailfish's normal media-volume policy.
    // Keep this after application() for Sailjail/icon-launch compatibility,
    // but before createView()/QML creates MediaPlayer.
    qputenv("PULSE_PROP_media.role", QByteArray("x-maemo"));

    qmlRegisterType<NetworkDiscoveryModel>("SailVideo", 1, 0, "NetworkDiscoveryModel");
    qmlRegisterType<LocalVideoFolderModel>("SailVideo", 1, 0, "LocalVideoFolderModel");

    app->setOrganizationName(QStringLiteral("org.edp17"));
    app->setOrganizationDomain(QStringLiteral("edp17.org"));
    app->setApplicationName(QStringLiteral("SailVideo"));

    NasSourceModel nasSources;
    NetworkSourceModel networkSources(nasSources.storageDirectory());
    NetworkDiscoveryModel networkDiscovery;
    PlaybackHistoryModel playbackHistory(nasSources.storageDirectory());
    LocalVideoModel localVideos;
    LocalVideoCategoryModel localVideoCategories(&localVideos);
    AppSettings appSettings(nasSources.storageDirectory());
    CastDeviceModel castDeviceModel(nasSources.storageDirectory());
    CastManager castManager;
    CastMediaPreparer castMediaPreparer(nasSources.storageDirectory());
    QObject::connect(app.data(), &QGuiApplication::aboutToQuit,
                     &appSettings, [&appSettings]() { appSettings.save(); });
    SmbBackend smbBackend;
    SmbCredentialStore smbCredentialStore;
    SystemAudioController systemAudioController;
    LocalFileStreamServer localFileStreamServer;
    localFileStreamServer.setSmbBackend(&smbBackend);
    UrlDownloadCache urlDownloadCache(nasSources.storageDirectory());

    QScopedPointer<QQuickView> view(SailfishApp::createView());

    const QUrl initialMediaUrl = commandLineMediaUrl(app->arguments());
    view->rootContext()->setContextProperty(
        QStringLiteral("initialMediaUrl"),
        initialMediaUrl);
    view->rootContext()->setContextProperty(
        QStringLiteral("playbackHistory"),
        &playbackHistory);
    view->rootContext()->setContextProperty(
        QStringLiteral("localVideoModel"),
        &localVideos);
    view->rootContext()->setContextProperty(
        QStringLiteral("localVideoCategoryModel"),
        &localVideoCategories);
    view->rootContext()->setContextProperty(
        QStringLiteral("appSettings"),
        &appSettings);
    view->rootContext()->setContextProperty(
        QStringLiteral("networkSources"),
        &networkSources);
    view->rootContext()->setContextProperty(
        QStringLiteral("networkDiscovery"),
        &networkDiscovery);
    view->rootContext()->setContextProperty(
        QStringLiteral("nasSources"),
        &nasSources);
    view->rootContext()->setContextProperty(
        QStringLiteral("smbBackend"),
        &smbBackend);
    view->rootContext()->setContextProperty(
        QStringLiteral("smbCredentialStore"),
        &smbCredentialStore);
    view->rootContext()->setContextProperty(
        QStringLiteral("systemAudioController"),
        &systemAudioController);
    view->rootContext()->setContextProperty(
        QStringLiteral("localFileStreamServer"),
        &localFileStreamServer);
    view->rootContext()->setContextProperty(
        QStringLiteral("urlDownloadCache"),
        &urlDownloadCache);
    view->rootContext()->setContextProperty(
        QStringLiteral("castDeviceModel"),
        &castDeviceModel);
    view->rootContext()->setContextProperty(
        QStringLiteral("castManager"),
        &castManager);
    view->rootContext()->setContextProperty(
        QStringLiteral("castMediaPreparer"),
        &castMediaPreparer);

    qInfo() << "SailVideo: URL download cache helper exposed";
    qInfo() << "SailVideo: URL sources saved to" << networkSources.storagePath();
    qInfo() << "SailVideo: Chromecast sender support enabled";

    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-sailvideo.qml")));
    view->show();

    return app->exec();
}
