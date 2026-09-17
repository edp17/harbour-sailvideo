/*
    Copyright (C) 2026 edp17 and chatGPT

    This file is part of harbour-sailvideo.

    The harbour-sailvideo is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The harbour-sailvideo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the harbour-sailvideo. If not, see <http://www.gnu.org/licenses/>.
*/
import QtQuick 2.0
import QtMultimedia 5.0
import Sailfish.Silica 1.0
import Nemo.KeepAlive 1.2

import "pages"

ApplicationWindow {
    id: appWindow

    property alias player: mediaPlayer
    property string currentMediaUrl: ""
    property string currentPlaybackUrl: ""
    property string currentMediaTitle: ""
    property string playbackError: ""
    property string playbackStatus: ""
    property int pendingResumePosition: 0
    property bool pendingResumeSeek: false
    property int pendingResumeAttempts: 0
    property int lastKnownPosition: 0
    property bool playerSuspended: false
    property bool currentUsesStreamBridge: false
    property string currentSourceKind: ""
    property string currentSmbHost: ""
    property int currentSmbPort: 445
    property string currentSmbShare: ""
    property string currentSmbPath: ""
    property string currentSmbDomain: ""
    property string currentSmbUsername: ""
    property string currentSmbPassword: ""
    property bool currentSmbGuest: true
    property var currentSmbSize: 0
    property bool playbackCompleted: false
    property bool hasMedia: currentMediaUrl.length > 0
    property var playbackQueue: []
    property int playbackQueueIndex: -1
    property bool hasPreviousVideo: playbackQueueIndex > 0
    property bool hasNextVideo: playbackQueueIndex >= 0 && playbackQueueIndex < playbackQueue.length - 1
    property var pictureQueue: []
    property int pictureQueueIndex: -1
    property bool hasPreviousPicture: pictureQueueIndex > 0
    property bool hasNextPicture: pictureQueueIndex >= 0 && pictureQueueIndex < pictureQueue.length - 1

    // Unified SMB folder media queue. The specialised video/picture queues
    // remain useful for video-only playback and picture-only slideshows, while
    // this queue lets Cast previous/next move naturally through mixed folders.
    property var folderMediaQueue: []
    property int folderMediaQueueIndex: -1
    property bool hasPreviousFolderMedia: folderMediaQueueIndex > 0
    property bool hasNextFolderMedia: folderMediaQueueIndex >= 0
                                      && folderMediaQueueIndex < folderMediaQueue.length - 1
    property bool castFolderNavigationActive: castMode
                                              && folderMediaQueueIndex >= 0
                                              && folderMediaQueue.length > 0
    property bool hasPreviousVideoControl: castFolderNavigationActive
                                           && videoCastActive
                                           ? hasPreviousFolderMedia
                                           : hasPreviousVideo
    property bool hasNextVideoControl: castFolderNavigationActive
                                       && videoCastActive
                                       ? hasNextFolderMedia
                                       : hasNextVideo
    property bool hasPreviousPictureControl: castFolderNavigationActive
                                             && castActivePicture
                                             ? hasPreviousFolderMedia
                                             : hasPreviousPicture
    property bool hasNextPictureControl: castFolderNavigationActive
                                         && castActivePicture
                                         ? hasNextFolderMedia
                                         : hasNextPicture
    property bool castPictureSlideshowRunning: false
    property bool remoteQueueSwitch: false
    property bool keepCastControlPageDuringQueueSwitch: false

    property string currentPictureTitle: ""
    property string currentPictureSource: ""
    property string currentPictureKind: ""
    property string currentPictureSmbHost: ""
    property int currentPictureSmbPort: 445
    property string currentPictureSmbShare: ""
    property string currentPictureSmbPath: ""
    property string currentPictureSmbDomain: ""
    property string currentPictureSmbUsername: ""
    property string currentPictureSmbPassword: ""
    property bool currentPictureSmbGuest: true
    property var currentPictureSmbSize: 0
    property real videoDimming: 0.0
    property int playbackVolumePercent: 30
    property string pendingNetworkSourceUrl: ""
    property string pendingNetworkTitle: ""
    property int pendingNetworkResumePosition: 0
    property var urlDownloadCacheObject: null
    property string adjustmentStatus: ""
    property bool userSeekPending: false

    // Chromecast sender state. castRequested remains true while a Cast transfer
    // is being established, before the TLS session reports connected.
    property bool castRequested: false
    property bool castResumeLocalAfterDisconnect: false
    property bool castDisconnectOnly: false
    property bool castRejoinPending: false
    // What the user came to the Cast page intending to send.
    property string castTargetKind: "video"

    // What the receiver is actually showing. During initial connection there
    // is no receiver media status yet, so use the requested kind as fallback.
    // During picture <-> video replacement, keep the old confirmed type until
    // Chromecast acknowledges the new media.
    property bool castActivePicture: castMode
                                     && (castManager.mediaInfoKnown
                                         ? castManager.imageMedia
                                         : castTargetKind === "picture")
    property bool videoCastActive: castMode && !castActivePicture

    property int castReturnPosition: 0
    property bool castUsesLanBridge: false
    property string castLastDeviceName: ""
    property string castLastHost: ""
    property int castLastPort: 8009
    property bool castMode: castRequested
                            || castManager.connected
                            || castManager.casting
                            || castManager.disconnecting


    allowedOrientations: Orientation.All

    function integerSetting(name, fallback, minimum, maximum) {
        try {
            var value = appSettings ? appSettings[name] : undefined
            if (value === undefined || value === null || isNaN(value)) {
                return fallback
            }
            value = Math.round(value)
            if (value < minimum || value > maximum) {
                return fallback
            }
            return value
        } catch (ignoredSettingError) {
            return fallback
        }
    }

    function defaultVolumePercent() {
        return integerSetting("defaultVolumePercent", 30, 0, 100)
    }

    function defaultBrightnessPercent() {
        return integerSetting("defaultBrightnessPercent", 50, 20, 100)
    }

    function bindUrlDownloadCache() {
        try {
            if (typeof urlDownloadCache !== "undefined" && urlDownloadCache) {
                urlDownloadCacheObject = urlDownloadCache
            } else {
                urlDownloadCacheObject = null
            }
        } catch (ignoredUrlCacheError) {
            urlDownloadCacheObject = null
        }
    }

    function hasUrlDownloadCache() {
        return urlDownloadCacheObject !== null
                && urlDownloadCacheObject !== undefined
                && typeof urlDownloadCacheObject.prepare === "function"
    }

    DisplayBlanking {
        preventBlanking: appSettings.keepDisplayOn
                         && mediaPlayer.playbackState === MediaPlayer.PlayingState
                         && !appWindow.playerSuspended
    }

    function twoDigits(value) {
        return value < 10 ? "0" + value : value
    }

    function formatTime(milliseconds) {
        if (!milliseconds || milliseconds < 0) {
            return "00:00"
        }

        var totalSeconds = Math.floor(milliseconds / 1000)
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor((totalSeconds % 3600) / 60)
        var seconds = totalSeconds % 60

        if (hours > 0) {
            return hours + ":" + twoDigits(minutes) + ":" + twoDigits(seconds)
        }

        return twoDigits(minutes) + ":" + twoDigits(seconds)
    }

    function skipMilliseconds() {
        return Math.max(1, appSettings.skipSeconds) * 1000
    }

    function fillModeShortLabel() {
        if (appSettings.videoFillMode === "crop") {
            return qsTr("Crop")
        }
        if (appSettings.videoFillMode === "stretch") {
            return qsTr("Stretch")
        }
        return qsTr("Fit")
    }

    function cycleVideoFillMode() {
        if (appSettings.videoFillMode === "fit") {
            appSettings.videoFillMode = "crop"
        } else if (appSettings.videoFillMode === "crop") {
            appSettings.videoFillMode = "stretch"
        } else {
            appSettings.videoFillMode = "fit"
        }
        showAdjustmentStatus(qsTr("Scaling: %1").arg(fillModeShortLabel()))
    }

    function showAdjustmentStatus(message) {
        adjustmentStatus = message || ""
        if (adjustmentStatus.length > 0) {
            adjustmentStatusTimer.restart()
        }
    }

    function volumePercent() {
        if (videoCastActive) {
            return castManager.volumePercent
        }
        return playbackVolumePercent
    }

    function setVolumePercent(value) {
        var bounded = Math.max(0, Math.min(100, Math.round(value)))

        if (videoCastActive) {
            if (castManager.connected) {
                castManager.setVolumePercent(bounded)
                showAdjustmentStatus(qsTr("Cast volume: %1%").arg(bounded))
            } else {
                showAdjustmentStatus(qsTr("Chromecast is connecting"))
            }
            return
        }

        var mute = bounded <= 0
        playbackVolumePercent = bounded

        // Keep QMediaPlayer updated for platforms where Qt volume is honoured.
        mediaPlayer.muted = mute
        mediaPlayer.volume = mute ? 0.0 : bounded / 100.0

        // On Sailfish hardware-decoded playback, QMediaPlayer volume/mute can
        // be ignored by the underlying GStreamer/droidmedia path. Use the
        // PulseAudio default sink as the effective media-volume path when pactl
        // is available. The C++ call is detached, so gesture handling is not
        // blocked by a shell command.
        if (typeof systemAudioController !== "undefined"
                && systemAudioController
                && systemAudioController.available) {
            systemAudioController.setVolumePercent(bounded)
        }

        showAdjustmentStatus(qsTr("Media volume: %1%").arg(bounded))
    }

    function adjustVolume(deltaPercent) {
        setVolumePercent(volumePercent() + deltaPercent)
    }

    function brightnessPercent() {
        return Math.round((1.0 - videoDimming) * 100)
    }

    function setBrightnessPercent(value) {
        var bounded = Math.max(20, Math.min(100, Math.round(value)))
        videoDimming = (100 - bounded) / 100.0
        showAdjustmentStatus(qsTr("Video brightness: %1%").arg(bounded))
    }

    function adjustVideoBrightness(deltaPercent) {
        setBrightnessPercent(brightnessPercent() + deltaPercent)
    }

    function resetPlaybackAdjustmentsForNewVideo() {
        setVolumePercent(defaultVolumePercent())
        setBrightnessPercent(defaultBrightnessPercent())
        adjustmentStatus = ""
        adjustmentStatusTimer.stop()
    }

    function formatBytes(bytes) {
        if (!bytes || bytes <= 0) {
            return ""
        }
        if (bytes >= 1024 * 1024 * 1024) {
            return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
        }
        if (bytes >= 1024 * 1024) {
            return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        }
        if (bytes >= 1024) {
            return Math.round(bytes / 1024) + " KB"
        }
        return bytes + " B"
    }

    function playbackPosition() {
        if (videoCastActive) {
            return Math.max(0, castManager.position)
        }
        return Math.max(0, mediaPlayer.position)
    }

    function playbackDuration() {
        if (videoCastActive && castManager.duration > 0) {
            return castManager.duration
        }
        return Math.max(0, mediaPlayer.duration)
    }

    function playbackIsPlaying() {
        if (videoCastActive) {
            return castManager.playing
        }
        return mediaPlayer.playbackState === MediaPlayer.PlayingState
    }

    function togglePlayback() {
        if (videoCastActive) {
            if (castManager.mediaStopped) {
                castManager.continueMedia()
            } else if (castManager.playing) {
                castManager.pause()
            } else {
                castManager.play()
            }
            return
        }

        if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
            mediaPlayer.pause()
        } else if (playerSuspended) {
            resumeCurrentMedia()
        } else {
            mediaPlayer.play()
        }
    }

    function openCastDevices() {
        if (!hasMedia) {
            return
        }

        castTargetKind = "video"
        if (castDeviceModel.count === 0 && !castDeviceModel.discovering) {
            castDeviceModel.startDiscovery()
        }
        pageStack.push(Qt.resolvedUrl("pages/CastDevicesPage.qml"))
    }

    function openCastDevicesForPicture() {
        if (!currentPictureSource || currentPictureSource.length === 0) {
            return
        }

        castTargetKind = "picture"
        if (castDeviceModel.count === 0 && !castDeviceModel.discovering) {
            castDeviceModel.startDiscovery()
        }
        pageStack.push(Qt.resolvedUrl("pages/CastDevicesPage.qml"))
    }

    function castUrlForCurrentMedia(peerHost) {
        castUsesLanBridge = false

        if (currentSourceKind === "network"
                && isNetworkUrl(currentMediaUrl)) {
            return currentMediaUrl
        }

        if (currentSourceKind === "smb") {
            var smbLanUrl = localFileStreamServer.lanStreamUrlForSmbFile(
                        currentSmbHost,
                        currentSmbPort,
                        currentSmbShare,
                        currentSmbPath,
                        currentSmbDomain,
                        currentSmbUsername,
                        currentSmbPassword,
                        currentSmbGuest,
                        currentSmbSize,
                        currentMediaTitle,
                        peerHost)
            if (smbLanUrl && smbLanUrl.length > 0) {
                castUsesLanBridge = true
            }
            return smbLanUrl
        }

        if (localFileStreamServer.isLocalFile(currentMediaUrl)) {
            var localLanUrl = localFileStreamServer.lanStreamUrlForLocalFile(
                        currentMediaUrl, peerHost)
            if (localLanUrl && localLanUrl.length > 0) {
                castUsesLanBridge = true
            }
            return localLanUrl
        }

        // currentPlaybackUrl may be a cached local URL source. Do not expose
        // localhost to Chromecast. Original HTTP/HTTPS sources were handled
        // above; other source types are not Cast-compatible yet.
        return ""
    }

    function castUrlForCurrentPicture(peerHost) {
        castUsesLanBridge = false

        if (currentPictureKind === "smb") {
            var pictureLanUrl = localFileStreamServer.lanStreamUrlForSmbFile(
                        currentPictureSmbHost,
                        currentPictureSmbPort,
                        currentPictureSmbShare,
                        currentPictureSmbPath,
                        currentPictureSmbDomain,
                        currentPictureSmbUsername,
                        currentPictureSmbPassword,
                        currentPictureSmbGuest,
                        currentPictureSmbSize,
                        currentPictureTitle,
                        peerHost)
            if (pictureLanUrl && pictureLanUrl.length > 0) {
                castUsesLanBridge = true
            }
            return pictureLanUrl
        }

        if (isNetworkUrl(currentPictureSource)) {
            return currentPictureSource
        }

        return ""
    }

    function startCastingToDevice(deviceName, host, port) {
        var picture = castTargetKind === "picture"
        if ((!picture && !hasMedia)
                || (picture && (!currentPictureSource || currentPictureSource.length === 0))
                || castManager.disconnecting) {
            return false
        }

        var cleanHost = cleanedValue(host)
        if (cleanHost.length === 0) {
            playbackError = qsTr("The Chromecast address is missing.")
            playbackStatus = playbackError
            return false
        }

        var remoteUrl = picture
                ? castUrlForCurrentPicture(cleanHost)
                : castUrlForCurrentMedia(cleanHost)
        if (!remoteUrl || remoteUrl.length === 0) {
            playbackError = localFileStreamServer.lastError.length > 0
                    ? localFileStreamServer.lastError
                    : (picture
                       ? qsTr("This picture cannot currently be exposed to Chromecast.")
                       : qsTr("This video cannot currently be exposed to Chromecast."))
            playbackStatus = playbackError
            return false
        }

        var startPosition = 0
        var initialCastVolume = picture ? -1 : playbackVolumePercent
        var mediaTitle = picture ? currentPictureTitle : currentMediaTitle
        if (!picture) {
            startPosition = playbackPosition()
            if (startPosition <= 0 && lastKnownPosition > 0) {
                startPosition = lastKnownPosition
            }

            savePlaybackPosition(false)
            lastKnownPosition = Math.max(0, startPosition)

            // Leave the local source loaded so a proper Disconnect can continue
            // on the phone at the remote position without reopening SMB or URL media.
            if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                mediaPlayer.pause()
            }
        }

        castLastDeviceName = cleanedValue(deviceName)
        castLastHost = cleanHost
        castLastPort = port > 0 ? port : 8009
        castRequested = true
        castResumeLocalAfterDisconnect = false
        castDisconnectOnly = false
        castRejoinPending = false
        castReturnPosition = startPosition
        playerSuspended = picture ? playerSuspended : false
        playbackError = ""
        playbackStatus = qsTr("Connecting to %1").arg(
                    castLastDeviceName.length > 0
                    ? castLastDeviceName
                    : qsTr("Chromecast"))

        var contentType = castManager.contentTypeForUrl(remoteUrl, mediaTitle)
        var started = castManager.startCasting(
                    castLastDeviceName,
                    castLastHost,
                    castLastPort,
                    remoteUrl,
                    contentType,
                    mediaTitle,
                    startPosition,
                    initialCastVolume)
        if (!started) {
            castRequested = false
            if (castUsesLanBridge) {
                localFileStreamServer.stopLanSharing()
                castUsesLanBridge = false
            }
            playbackError = castManager.lastError
            playbackStatus = playbackError
            if (!picture && mediaPlayer.source && String(mediaPlayer.source).length > 0) {
                mediaPlayer.play()
            }
            return false
        }

        console.log("SailVideo Cast: transferring current "
                    + (picture ? "picture" : "video") + " to "
                    + castLastDeviceName + " url=" + remoteUrl
                    + " positionMs=" + startPosition)
        return true
    }

    function castCurrentPictureToConnectedDevice() {
        if (castTargetKind !== "picture"
                || !castManager.connected
                || !currentPictureSource
                || currentPictureSource.length === 0) {
            return false
        }

        var remoteUrl = castUrlForCurrentPicture(castManager.host)
        if (!remoteUrl || remoteUrl.length === 0) {
            playbackError = localFileStreamServer.lastError.length > 0
                    ? localFileStreamServer.lastError
                    : qsTr("This picture cannot currently be exposed to Chromecast.")
            return false
        }

        var contentType = castManager.contentTypeForUrl(remoteUrl, currentPictureTitle)
        return castManager.replaceMedia(remoteUrl,
                                        contentType,
                                        currentPictureTitle,
                                        0)
    }


    function castCurrentVideoToConnectedDevice(startPositionMs) {
        if (castTargetKind !== "video"
                || !castManager.connected
                || !hasMedia) {
            return false
        }

        var remoteUrl = castUrlForCurrentMedia(castManager.host)
        if (!remoteUrl || remoteUrl.length === 0) {
            playbackError = localFileStreamServer.lastError.length > 0
                    ? localFileStreamServer.lastError
                    : qsTr("This video cannot currently be exposed to Chromecast.")
            playbackStatus = playbackError
            return false
        }

        var startPosition = startPositionMs !== undefined && startPositionMs !== null
                ? Math.max(0, Math.round(startPositionMs))
                : Math.max(0, lastKnownPosition)

        if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
            mediaPlayer.pause()
        }

        lastKnownPosition = startPosition
        var contentType = castManager.contentTypeForUrl(remoteUrl, currentMediaTitle)
        var requestedVolume = castManager.mediaInfoKnown && !castManager.imageMedia
                ? castManager.volumePercent
                : playbackVolumePercent
        return castManager.replaceMediaWithVolume(remoteUrl,
                                                  contentType,
                                                  currentMediaTitle,
                                                  startPosition,
                                                  requestedVolume)
    }

    function castRequestedMediaToConnectedDevice(startSlideshow) {
        if (!castManager.connected || castManager.disconnecting) {
            return false
        }

        if (castTargetKind === "picture") {
            var pictureStarted = castCurrentPictureToConnectedDevice()
            if (pictureStarted && startSlideshow === true) {
                castPictureSlideshowRunning = true
            }
            return pictureStarted
        }

        castPictureSlideshowRunning = false
        var startPosition = mediaPlayer.position > 0
                ? mediaPlayer.position
                : Math.max(0, lastKnownPosition)
        return castCurrentVideoToConnectedDevice(startPosition)
    }

    function toggleCastPictureSlideshowFromControlPage() {
        if (castPictureSlideshowRunning) {
            castPictureSlideshowRunning = false
            return true
        }

        if (castTargetKind !== "picture" || pictureQueue.length < 2) {
            return false
        }

        if (!castActivePicture) {
            return castRequestedMediaToConnectedDevice(true)
        }

        castPictureSlideshowRunning = true
        return true
    }

    function returnToActiveCastMedia() {
        if (!castMode) {
            return false
        }

        var picture = castActivePicture
        castTargetKind = picture ? "picture" : "video"
        if (picture && currentPictureSource && currentPictureSource.length > 0) {
            if (!pageStack.currentPage
                    || pageStack.currentPage.objectName !== "pictureViewerPage") {
                pageStack.push(Qt.resolvedUrl("pages/PictureViewerPage.qml"))
            }
            return true
        }

        if (!picture && hasMedia) {
            if (!pageStack.currentPage
                    || pageStack.currentPage.objectName !== "playerPage") {
                pageStack.push(Qt.resolvedUrl("pages/PlayerPage.qml"))
            }
            return true
        }

        if (!pageStack.currentPage
                || pageStack.currentPage.objectName !== "castDevicesPage") {
            pageStack.push(Qt.resolvedUrl("pages/CastDevicesPage.qml"))
        }
        return false
    }

    function completeCastDisconnectOnly() {
        castPictureSlideshowRunning = false
        castRequested = false
        castResumeLocalAfterDisconnect = false
        castDisconnectOnly = false
        castRejoinPending = false
        if (castUsesLanBridge) {
            localFileStreamServer.stopLanSharing()
            castUsesLanBridge = false
        }
        appSettings.clearDetachedCastSession()
        playbackStatus = qsTr("Chromecast disconnected")
    }

    function completeCastReturnToPhone() {
        castPictureSlideshowRunning = false
        var target = Math.max(0, castReturnPosition)
        castRequested = false
        castResumeLocalAfterDisconnect = false
        castDisconnectOnly = false
        castRejoinPending = false
        appSettings.clearDetachedCastSession()
        if (castUsesLanBridge) {
            localFileStreamServer.stopLanSharing()
            castUsesLanBridge = false
        }

        if (!hasMedia) {
            return
        }

        playbackError = ""
        playerSuspended = false
        pendingResumeSeek = false
        pendingResumeAttempts = 0
        resumeSeekTimer.stop()

        if (mediaPlayer.source && String(mediaPlayer.source).length > 0) {
            if (target > 0) {
                mediaPlayer.seek(target)
            }
            lastKnownPosition = target
            mediaPlayer.play()
            playbackStatus = ""
        } else {
            lastKnownPosition = target
            resumeCurrentMedia()
        }
    }

    function disconnectCastAndResume() {
        if (!castMode) {
            return
        }

        appSettings.clearDetachedCastSession()

        if (castActivePicture) {
            castResumeLocalAfterDisconnect = false
            castDisconnectOnly = true
            if (castManager.connected || castManager.casting) {
                castManager.disconnectAndStopReceiver()
            } else {
                castManager.detach()
                completeCastDisconnectOnly()
            }
            return
        }

        castReturnPosition = playbackPosition()
        if (castReturnPosition > 0) {
            lastKnownPosition = castReturnPosition
        }
        savePlaybackPosition(false)
        castResumeLocalAfterDisconnect = true
        castDisconnectOnly = false

        if (castManager.connected || castManager.casting) {
            castManager.disconnectAndStopReceiver()
        } else {
            castManager.detach()
            completeCastReturnToPhone()
        }
    }

    function detachCastKeepPlaying() {
        if (!castMode) {
            return
        }

        if (videoCastActive) {
            var remotePosition = playbackPosition()
            if (remotePosition > 0) {
                lastKnownPosition = remotePosition
            }
            savePlaybackPosition(false)
        }

        appSettings.setDetachedCastSession(
                    castManager.deviceName.length > 0 ? castManager.deviceName : castLastDeviceName,
                    castManager.host.length > 0 ? castManager.host : castLastHost,
                    castManager.port > 0 ? castManager.port : castLastPort)

        castResumeLocalAfterDisconnect = false
        castDisconnectOnly = false
        castRejoinPending = false
        castRequested = false
        castManager.detach()
        playbackStatus = qsTr("Cast playback left running on TV")
        // Intentionally keep the LAN bridge alive. Local/NAS playback can
        // continue while SailVideo remains running; direct URLs no longer
        // depend on the phone once detached.
    }

    function tryRejoinDetachedCast(openPage) {
        if (!appSettings.hasDetachedCastSession || castManager.disconnecting) {
            return false
        }

        if (castManager.connected || castManager.casting) {
            return true
        }

        castLastDeviceName = cleanedValue(appSettings.detachedCastDeviceName)
        castLastHost = cleanedValue(appSettings.detachedCastHost)
        castLastPort = appSettings.detachedCastPort > 0
                ? appSettings.detachedCastPort
                : 8009

        if (castLastHost.length === 0) {
            return false
        }

        castRequested = true
        castRejoinPending = true
        castResumeLocalAfterDisconnect = false
        castDisconnectOnly = false
        playbackError = ""
        playbackStatus = qsTr("Reconnecting to %1").arg(
                    castLastDeviceName.length > 0
                    ? castLastDeviceName
                    : qsTr("Chromecast"))

        if (openPage
                && (!pageStack.currentPage
                    || pageStack.currentPage.objectName !== "castDevicesPage")) {
            pageStack.push(Qt.resolvedUrl("pages/CastDevicesPage.qml"))
        }

        var started = castManager.rejoin(castLastDeviceName,
                                         castLastHost,
                                         castLastPort)
        if (!started) {
            castRequested = false
            castRejoinPending = false
            playbackError = castManager.lastError
            playbackStatus = playbackError
            return false
        }

        return true
    }

    function handleCastFailureIfIdle() {
        if (castManager.connected
                || castManager.casting
                || castManager.disconnecting
                || castManager.lastError.length === 0) {
            return false
        }

        if (castRejoinPending) {
            castRequested = false
            castRejoinPending = false
            playbackError = castManager.lastError
            playbackStatus = playbackError
            return true
        }

        if (!castRequested) {
            return false
        }

        var failedPosition = castReturnPosition > 0
                ? castReturnPosition
                : lastKnownPosition
        var failedPictureCast = castTargetKind === "picture"
        castRequested = false
        if (castUsesLanBridge) {
            localFileStreamServer.stopLanSharing()
            castUsesLanBridge = false
        }
        playbackError = castManager.lastError
        playbackStatus = playbackError
        lastKnownPosition = failedPosition
        if (!failedPictureCast
                && mediaPlayer.source
                && String(mediaPlayer.source).length > 0) {
            if (failedPosition > 0) {
                mediaPlayer.seek(failedPosition)
            }
            mediaPlayer.play()
        }
        return true
    }

    function leavePlayerView() {
        if (videoCastActive) {
            savePlaybackPosition(false)
            return
        }
        suspendPlayback()
    }

    function requestSeek(targetPosition) {
        if (!hasMedia || userSeekPending) {
            return
        }

        var duration = playbackDuration()
        if (duration <= 0) {
            return
        }

        var safeTarget = Math.max(0, Math.min(duration, Math.round(targetPosition)))
        if (duration > 5000) {
            safeTarget = Math.min(safeTarget, duration - 1000)
        }

        if (videoCastActive) {
            userSeekPending = true
            playbackStatus = qsTr("Seeking on %1").arg(
                        castManager.deviceName.length > 0
                        ? castManager.deviceName
                        : qsTr("Chromecast"))
            lastKnownPosition = safeTarget
            playbackHistory.updatePosition(currentMediaUrl, duration, safeTarget)
            castManager.seek(safeTarget)
            seekSettleTimer.restart()
            return
        }

        pendingResumeSeek = false
        pendingResumeAttempts = 0
        resumeSeekTimer.stop()
        userSeekPending = true
        playbackStatus = qsTr("Seeking")
        lastKnownPosition = safeTarget
        playbackHistory.updatePosition(currentMediaUrl, duration, safeTarget)
        mediaPlayer.seek(safeTarget)
        seekSettleTimer.restart()
    }

    function restartCurrentMedia() {
        if (!hasMedia) {
            return
        }

        if (videoCastActive) {
            if (castManager.mediaStopped) {
                castManager.seek(0)
                castManager.continueMedia()
            } else {
                castManager.seek(0)
                castManager.play()
            }
            if (videoCastActive) {
                lastKnownPosition = 0
                playbackHistory.updatePosition(currentMediaUrl, playbackDuration(), 0)
            }
            return
        }

        if (playerSuspended) {
            lastKnownPosition = 0
            playbackHistory.updatePosition(currentMediaUrl, mediaPlayer.duration, 0)
            resumeCurrentMedia()
            return
        }

        requestSeek(0)
        if (mediaPlayer.playbackState !== MediaPlayer.PlayingState) {
            mediaPlayer.play()
        }
    }

    function cleanedValue(value) {
        if (value === undefined || value === null) {
            return ""
        }

        var text = String(value).replace(/^\s+|\s+$/g, "")
        for (var i = 0; i < 3; ++i) {
            if (text.length >= 2
                    && ((text.charAt(0) === "'" && text.charAt(text.length - 1) === "'")
                        || (text.charAt(0) === "\"" && text.charAt(text.length - 1) === "\""))) {
                text = text.substring(1, text.length - 1).replace(/^\s+|\s+$/g, "")
            } else {
                break
            }
        }
        return text
    }

    function normalizedMediaUrl(mediaUrl) {
        return cleanedValue(mediaUrl)
    }

    function titleFromPickerProperties(properties) {
        if (!properties) {
            return ""
        }

        var candidates = [
            properties.title,
            properties.fileName,
            properties.name,
            properties.displayName
        ]

        for (var i = 0; i < candidates.length; ++i) {
            var candidate = cleanedValue(candidates[i])
            if (candidate.length > 0) {
                return candidate
            }
        }

        return ""
    }

    function mediaUrlFromPickerProperties(properties) {
        if (!properties) {
            return ""
        }

        var candidates = [
            properties.url,
            properties.filePath,
            properties.path
        ]

        for (var i = 0; i < candidates.length; ++i) {
            var candidate = cleanedValue(candidates[i])
            if (candidate.length > 0) {
                return candidate
            }
        }

        return ""
    }

    function isCurrentLocalFile() {
        return hasMedia && localFileStreamServer.isLocalFile(currentMediaUrl)
    }

    function isNetworkUrl(mediaUrl) {
        return networkSources.isSupportedPlaybackUrl(mediaUrl)
    }

    function isSmbUrl(mediaUrl) {
        return smbBackend.isSmbUrl(mediaUrl)
    }

    function isPictureFile(fileName) {
        return smbBackend.isLikelyImageFile(fileName)
    }

    function clearPlaybackQueue() {
        playbackQueue = []
        playbackQueueIndex = -1
    }

    function clearPictureQueue() {
        pictureQueue = []
        pictureQueueIndex = -1
    }

    function clearFolderMediaQueue() {
        folderMediaQueue = []
        folderMediaQueueIndex = -1
    }

    function normalizedQueuePath(value) {
        var text = cleanedValue(value).replace(/\\/g, "/")
        while (text.indexOf("//") >= 0) {
            text = text.replace(/\/\//g, "/")
        }
        while (text.length > 0 && text.charAt(0) === "/") {
            text = text.substring(1)
        }
        while (text.length > 0 && text.charAt(text.length - 1) === "/") {
            text = text.substring(0, text.length - 1)
        }
        return text
    }

    function queuePathLeaf(value) {
        var text = normalizedQueuePath(value)
        var slash = text.lastIndexOf("/")
        return slash >= 0 ? text.substring(slash + 1) : text
    }

    function setLocalPlaybackQueue(model, selectedIndex) {
        clearPictureQueue()
        clearFolderMediaQueue()
        var queue = []
        var selected = -1

        if (!model || model.count === undefined
                || typeof model.urlAt !== "function"
                || typeof model.titleAt !== "function") {
            clearPlaybackQueue()
            return
        }

        for (var i = 0; i < model.count; ++i) {
            var url = cleanedValue(model.urlAt(i))
            if (url.length === 0) {
                continue
            }

            if (i === selectedIndex) {
                selected = queue.length
            }
            queue.push({
                           kind: "local",
                           url: url,
                           title: cleanedValue(model.titleAt(i))
                       })
        }

        playbackQueue = queue
        playbackQueueIndex = selected
    }

    function setSmbPictureQueueFromModel(model, selectedPath, host, port, share, domain, username, password, guest) {
        var queue = []
        var selected = -1
        var cleanSelectedPath = normalizedQueuePath(selectedPath)
        var selectedLeaf = queuePathLeaf(selectedPath)

        if (!model || model.count === undefined) {
            clearPictureQueue()
            return
        }

        for (var i = 0; i < model.count; ++i) {
            var entry = model.get(i)
            if (!entry || entry.isDirectory === true || entry.isDirectory === 1) {
                continue
            }
            if (!(entry.isImage === true || entry.isImage === 1 || smbBackend.isLikelyImageFile(entry.name))) {
                continue
            }

            var rawPath = cleanedValue(entry.path)
            var cleanPath = normalizedQueuePath(rawPath)
            var item = {
                kind: "smb",
                host: cleanedValue(host),
                port: port > 0 ? port : 445,
                share: cleanedValue(share),
                path: rawPath,
                domain: cleanedValue(domain),
                username: cleanedValue(username),
                password: password || "",
                guest: guest === true,
                size: entry.size > 0 ? entry.size : 0,
                title: cleanedValue(entry.name)
            }

            if (cleanPath === cleanSelectedPath
                    || (selected < 0
                        && selectedLeaf.length > 0
                        && queuePathLeaf(cleanPath) === selectedLeaf)) {
                selected = queue.length
            }
            queue.push(item)
        }

        pictureQueue = queue
        pictureQueueIndex = selected
    }


    function setSmbFolderMediaQueueFromModel(model, selectedPath, host, port, share, domain, username, password, guest) {
        var queue = []
        var selected = -1
        var cleanSelectedPath = normalizedQueuePath(selectedPath)
        var selectedLeaf = queuePathLeaf(selectedPath)

        if (!model || model.count === undefined) {
            clearFolderMediaQueue()
            return
        }

        for (var i = 0; i < model.count; ++i) {
            var entry = model.get(i)
            if (!entry || entry.isDirectory === true || entry.isDirectory === 1) {
                continue
            }

            var imageEntry = entry.isImage === true
                    || entry.isImage === 1
                    || smbBackend.isLikelyImageFile(entry.name)
            var videoEntry = entry.isVideo === true
                    || entry.isVideo === 1
                    || smbBackend.isLikelyVideoFile(entry.name)
            if (!imageEntry && !videoEntry) {
                continue
            }

            var rawPath = cleanedValue(entry.path)
            var cleanPath = normalizedQueuePath(rawPath)
            var item = {
                mediaType: imageEntry ? "picture" : "video",
                kind: "smb",
                host: cleanedValue(host),
                port: port > 0 ? port : 445,
                share: cleanedValue(share),
                path: rawPath,
                domain: cleanedValue(domain),
                username: cleanedValue(username),
                password: password || "",
                guest: guest === true,
                size: entry.size > 0 ? entry.size : 0,
                title: cleanedValue(entry.name)
            }

            if (cleanPath === cleanSelectedPath
                    || (selected < 0
                        && selectedLeaf.length > 0
                        && queuePathLeaf(cleanPath) === selectedLeaf)) {
                selected = queue.length
            }
            queue.push(item)
        }

        folderMediaQueue = queue
        folderMediaQueueIndex = selected
    }

    function queueIndexForPath(queue, path) {
        var cleanPath = normalizedQueuePath(path)
        var leaf = queuePathLeaf(path)
        for (var i = 0; i < queue.length; ++i) {
            var item = queue[i]
            if (!item || !item.path) {
                continue
            }
            var candidate = normalizedQueuePath(item.path)
            if (candidate === cleanPath
                    || (leaf.length > 0 && queuePathLeaf(candidate) === leaf)) {
                return i
            }
        }
        return -1
    }

    function syncSmbQueueIndices(path, mediaType) {
        var folderIndex = queueIndexForPath(folderMediaQueue, path)
        if (folderIndex >= 0) {
            folderMediaQueueIndex = folderIndex
        }

        if (mediaType === "picture") {
            var pictureIndex = queueIndexForPath(pictureQueue, path)
            if (pictureIndex >= 0) {
                pictureQueueIndex = pictureIndex
            }
        } else if (mediaType === "video") {
            var videoIndex = queueIndexForPath(playbackQueue, path)
            if (videoIndex >= 0) {
                playbackQueueIndex = videoIndex
            }
        }
    }

    function openFolderMediaQueueIndex(index, stayOnCastPage) {
        if (index < 0 || index >= folderMediaQueue.length) {
            return false
        }

        var item = folderMediaQueue[index]
        if (!item || item.kind !== "smb") {
            return false
        }

        var wasCasting = castMode && castManager.connected
        folderMediaQueueIndex = index
        keepCastControlPageDuringQueueSwitch = stayOnCastPage === true
        remoteQueueSwitch = wasCasting

        if (item.mediaType === "picture") {
            castTargetKind = "picture"
            var pictureOpened = openSmbPicture(item.host, item.port, item.share, item.path,
                                               item.domain, item.username, item.password,
                                               item.guest, item.size, item.title)
            remoteQueueSwitch = false
            keepCastControlPageDuringQueueSwitch = false
            return pictureOpened
        }

        castPictureSlideshowRunning = false
        castTargetKind = "video"
        var videoOpened = openSmbFile(item.host, item.port, item.share, item.path,
                                      item.domain, item.username, item.password,
                                      item.guest, item.size, item.title, 0)
        remoteQueueSwitch = false
        keepCastControlPageDuringQueueSwitch = false

        if (videoOpened && wasCasting) {
            return castCurrentVideoToConnectedDevice(0)
        }
        return videoOpened
    }

    function showPreviousVideoControl() {
        if (castFolderNavigationActive && videoCastActive) {
            return openFolderMediaQueueIndex(folderMediaQueueIndex - 1, false)
        }
        return playPreviousVideo()
    }

    function showNextVideoControl() {
        if (castFolderNavigationActive && videoCastActive) {
            return openFolderMediaQueueIndex(folderMediaQueueIndex + 1, false)
        }
        return playNextVideo()
    }

    function showPreviousPictureControl() {
        castPictureSlideshowRunning = false
        if (castFolderNavigationActive && castActivePicture) {
            return openFolderMediaQueueIndex(folderMediaQueueIndex - 1, false)
        }
        return showPreviousPicture()
    }

    function showNextPictureControl() {
        castPictureSlideshowRunning = false
        if (castFolderNavigationActive && castActivePicture) {
            return openFolderMediaQueueIndex(folderMediaQueueIndex + 1, false)
        }
        return showNextPicture()
    }

    function showPreviousCastMediaFromControlPage() {
        castPictureSlideshowRunning = false
        if (castFolderNavigationActive) {
            return openFolderMediaQueueIndex(folderMediaQueueIndex - 1, true)
        }
        if (castActivePicture) {
            return showPreviousPicture()
        }
        return playPreviousVideo()
    }

    function showNextCastMediaFromControlPage() {
        castPictureSlideshowRunning = false
        if (castFolderNavigationActive) {
            return openFolderMediaQueueIndex(folderMediaQueueIndex + 1, true)
        }
        if (castActivePicture) {
            return showNextPicture()
        }
        return playNextVideo()
    }

    function advanceCastPictureSlideshow() {
        if (!castPictureSlideshowRunning
                || !castMode
                || !castActivePicture
                || pictureQueue.length < 2) {
            castPictureSlideshowRunning = false
            return false
        }

        if (pictureQueueIndex < 0) {
            pictureQueueIndex = queueIndexForPath(pictureQueue, currentPictureSmbPath)
        }

        var nextIndex = pictureQueueIndex + 1
        if (nextIndex >= pictureQueue.length) {
            nextIndex = 0
        }

        keepCastControlPageDuringQueueSwitch = true
        var advanced = openPictureQueueIndex(nextIndex)
        keepCastControlPageDuringQueueSwitch = false
        return advanced
    }

    function openPictureQueueIndex(index) {
        if (index < 0 || index >= pictureQueue.length) {
            return false
        }

        var item = pictureQueue[index]
        pictureQueueIndex = index
        if (item.kind === "smb") {
            return openSmbPicture(item.host, item.port, item.share, item.path,
                                  item.domain, item.username, item.password,
                                  item.guest, item.size, item.title)
        }

        return false
    }

    function showPreviousPicture() {
        return openPictureQueueIndex(pictureQueueIndex - 1)
    }

    function showNextPicture() {
        return openPictureQueueIndex(pictureQueueIndex + 1)
    }

    function setSmbPlaybackQueueFromModel(model, selectedPath, host, port, share, domain, username, password, guest) {
        var queue = []
        var selected = -1
        var cleanSelectedPath = normalizedQueuePath(selectedPath)
        var selectedLeaf = queuePathLeaf(selectedPath)

        if (!model || model.count === undefined) {
            clearPlaybackQueue()
            return
        }

        for (var i = 0; i < model.count; ++i) {
            var entry = model.get(i)
            if (!entry || entry.isDirectory === true || entry.isDirectory === 1) {
                continue
            }
            if (!(entry.isVideo === true || entry.isVideo === 1 || smbBackend.isLikelyVideoFile(entry.name))) {
                continue
            }

            var rawPath = cleanedValue(entry.path)
            var cleanPath = normalizedQueuePath(rawPath)
            var item = {
                kind: "smb",
                host: cleanedValue(host),
                port: port > 0 ? port : 445,
                share: cleanedValue(share),
                path: rawPath,
                domain: cleanedValue(domain),
                username: cleanedValue(username),
                password: password || "",
                guest: guest === true,
                size: entry.size > 0 ? entry.size : 0,
                title: cleanedValue(entry.name)
            }

            if (cleanPath === cleanSelectedPath
                    || (selected < 0
                        && selectedLeaf.length > 0
                        && queuePathLeaf(cleanPath) === selectedLeaf)) {
                selected = queue.length
            }
            queue.push(item)
        }

        playbackQueue = queue
        playbackQueueIndex = selected
    }

    function playQueueIndex(index) {
        if (index < 0 || index >= playbackQueue.length) {
            return false
        }

        var wasCasting = videoCastActive
        var deviceName = castManager.deviceName.length > 0
                ? castManager.deviceName : castLastDeviceName
        var deviceHost = castManager.host.length > 0
                ? castManager.host : castLastHost
        var devicePort = castManager.port > 0
                ? castManager.port : castLastPort

        var item = playbackQueue[index]
        playbackQueueIndex = index
        if (item.kind === "smb") {
            if (wasCasting) {
                // Close only our sender connection. The receiver keeps the old
                // item running until the new LOAD arrives.
                castManager.detach()
                castRequested = false
                castResumeLocalAfterDisconnect = false
                lastKnownPosition = 0
            }

            var opened = openSmbFile(item.host, item.port, item.share, item.path,
                                     item.domain, item.username, item.password,
                                     item.guest, item.size, item.title, 0)
            if (opened && wasCasting && deviceHost.length > 0) {
                castTargetKind = "video"
                return startCastingToDevice(deviceName, deviceHost, devicePort)
            }
            return opened
        }

        if (item.kind === "local") {
            if (wasCasting) {
                castManager.detach()
                castRequested = false
                castResumeLocalAfterDisconnect = false
                lastKnownPosition = 0
            }

            openMedia(item.url, item.title, 0, true)
            if (wasCasting && deviceHost.length > 0) {
                castTargetKind = "video"
                return startCastingToDevice(deviceName, deviceHost, devicePort)
            }
            return true
        }

        return false
    }

    function playPreviousVideo() {
        return playQueueIndex(playbackQueueIndex - 1)
    }

    function playNextVideo() {
        return playQueueIndex(playbackQueueIndex + 1)
    }

    function storedSourceSize(url) {
        if (playbackHistory
                && typeof playbackHistory.sourceSizeForUrl === "function") {
            return playbackHistory.sourceSizeForUrl(url)
        }

        return 0
    }

    function logHistoryState(prefix) {
        console.log(prefix + ": historyUrl=" + playbackHistory.latestUrlValue()
                    + " historyTitle=" + playbackHistory.latestTitleValue()
                    + " historyPosition=" + playbackHistory.latestPositionValue()
                    + " storage=" + playbackHistory.storagePath)
    }

    function openNetworkUrl(mediaUrl, title, resumePosition) {
        clearPlaybackQueue()
        clearPictureQueue()
        clearFolderMediaQueue()
        var playbackUrl = networkSources.normalizedUrl(mediaUrl)
        if (!playbackUrl || playbackUrl.length === 0) {
            playbackError = networkSources.lastError
            playbackStatus = playbackError
            return false
        }

        pendingNetworkSourceUrl = playbackUrl
        pendingNetworkTitle = cleanedValue(title)
        pendingNetworkResumePosition = resumePosition !== undefined && resumePosition !== null
                ? Math.max(0, resumePosition)
                : playbackHistory.resumePosition(playbackUrl)

        playbackError = ""
        playbackStatus = qsTr("Downloading URL")
        playerSuspended = false
        playbackCompleted = false
        pendingResumeSeek = false
        pendingResumeAttempts = 0
        lastKnownPosition = pendingNetworkResumePosition
        currentMediaUrl = playbackUrl
        currentPlaybackUrl = ""
        currentMediaTitle = playbackHistory.titleForUrl(playbackUrl,
                                                        pendingNetworkTitle.length > 0
                                                        ? pendingNetworkTitle
                                                        : playbackUrl)
        currentUsesStreamBridge = true
        currentSourceKind = "network"

        if (!pageStack.currentPage
                || pageStack.currentPage.objectName !== "playerPage") {
            pageStack.push(Qt.resolvedUrl("pages/PlayerPage.qml"))
        }

        bindUrlDownloadCache()
        if (!hasUrlDownloadCache()) {
            playbackError = qsTr("URL playback helper is unavailable. The installed executable is older than the QML files; fully rebuild and reinstall SailVideo.")
            playbackStatus = playbackError
            console.warn("SailVideo: URL download cache helper is missing from the QML context")
            return false
        }

        if (!urlDownloadCacheObject.prepare(playbackUrl)) {
            playbackError = urlDownloadCacheObject.lastError
            playbackStatus = playbackError
            return false
        }

        return true
    }

    function playPreparedNetworkUrl(sourceUrl, cachedFileUrl) {
        if (cleanedValue(sourceUrl) !== pendingNetworkSourceUrl) {
            return false
        }

        var streamUrl = localFileStreamServer.streamUrlForLocalFile(cachedFileUrl)
        if (!streamUrl || streamUrl.length === 0) {
            playbackError = localFileStreamServer.lastError
            playbackStatus = playbackError
            return false
        }

        var title = pendingNetworkTitle.length > 0
                ? pendingNetworkTitle
                : currentMediaTitle
        var resume = pendingNetworkResumePosition
        pendingNetworkSourceUrl = ""
        pendingNetworkTitle = ""
        pendingNetworkResumePosition = 0

        playSource(sourceUrl, streamUrl, title, resume, true, "network")
        return true
    }

    function openHistoryEntry(mediaUrl, title, resumePosition) {
        clearPlaybackQueue()
        clearPictureQueue()
        clearFolderMediaQueue()
        if (isNetworkUrl(mediaUrl)) {
            openNetworkUrl(mediaUrl, title, resumePosition)
        } else if (isSmbUrl(mediaUrl)) {
            openRememberedSmbFromUrl(mediaUrl, title, resumePosition, true)
        } else {
            openMedia(mediaUrl, title, resumePosition)
        }
    }

    function openRememberedSmbFromUrl(mediaUrl, title, resumePosition, showBrowserWhenPasswordMissing) {
        var parsed = smbBackend.parseSmbUrl(mediaUrl)
        if (!parsed.valid) {
            playbackError = qsTr("The remembered SMB video location is invalid.")
            playbackStatus = playbackError
            return false
        }

        var sourceIndex = nasSources.indexForLocation(parsed.host, parsed.port, parsed.share)
        var sourceTitle = sourceIndex >= 0 ? nasSources.nameAt(sourceIndex) : parsed.host + "/" + parsed.share
        var sourceDomain = sourceIndex >= 0 ? nasSources.domainAt(sourceIndex) : ""
        var sourceUsername = sourceIndex >= 0 ? nasSources.usernameAt(sourceIndex) : ""
        var sourceGuest = sourceIndex >= 0 ? nasSources.guestAt(sourceIndex) : false
        var sourcePassword = sourceGuest ? "" : smbCredentialStore.passwordFor(parsed.host,
                                                                                 parsed.port,
                                                                                 parsed.share,
                                                                                 sourceDomain,
                                                                                 sourceUsername,
                                                                                 sourceGuest)

        if (!sourceGuest && sourcePassword.length === 0) {
            playbackError = qsTr("Open the NAS source and enter the password to resume this SMB video.")
            playbackStatus = playbackError
            if (showBrowserWhenPasswordMissing === true) {
                pageStack.push(Qt.resolvedUrl("pages/NasBrowserPage.qml"),
                               { sourceTitle: sourceTitle,
                                 host: parsed.host,
                                 port: parsed.port,
                                 share: parsed.share,
                                 domain: sourceDomain,
                                 username: sourceUsername,
                                 guest: sourceGuest,
                                 currentPath: "" })
            }
            return false
        }

        return openSmbFile(parsed.host, parsed.port, parsed.share, parsed.path,
                           sourceDomain, sourceUsername, sourcePassword, sourceGuest,
                           storedSourceSize(mediaUrl),
                           title && title.length > 0 ? title : parsed.fileName, resumePosition)
    }

    function openMedia(mediaUrl, title, resumePosition, preserveQueue) {
        if (preserveQueue !== true) {
            clearPlaybackQueue()
            clearPictureQueue()
            clearFolderMediaQueue()
        }
        playSource(mediaUrl, mediaUrl, title, resumePosition, false)
    }

    function openLocalFileThroughBridge(mediaUrl, title, resumePosition) {
        clearPlaybackQueue()
        clearPictureQueue()
        clearFolderMediaQueue()
        var sourceUrl = normalizedMediaUrl(mediaUrl)
        if (sourceUrl.length === 0) {
            return
        }

        var streamUrl = localFileStreamServer.streamUrlForLocalFile(sourceUrl)
        if (!streamUrl || streamUrl.length === 0) {
            playbackError = localFileStreamServer.lastError
            playbackStatus = playbackError
            return
        }

        playSource(sourceUrl, streamUrl, title, resumePosition, true)
    }

    function openSmbPicture(host, port, share, path, domain, username, password, guest, size, title) {
        var cleanPath = cleanedValue(path)
        var pictureTitle = cleanedValue(title)
        if (pictureTitle.length === 0) {
            pictureTitle = cleanPath.length > 0 ? cleanPath.substring(cleanPath.lastIndexOf("/") + 1) : qsTr("Picture")
        }

        if (hasMedia && !playerSuspended) {
            suspendPlayback()
        }

        var streamUrl = localFileStreamServer.streamUrlForSmbFile(
                    host, port, share, cleanPath, domain, username, password, guest, size, pictureTitle)
        if (!streamUrl || streamUrl.length === 0) {
            playbackError = localFileStreamServer.lastError
            playbackStatus = playbackError
            return false
        }

        currentPictureKind = "smb"
        currentPictureSmbHost = cleanedValue(host)
        currentPictureSmbPort = port > 0 ? port : 445
        currentPictureSmbShare = cleanedValue(share)
        currentPictureSmbPath = cleanPath
        currentPictureSmbDomain = cleanedValue(domain)
        currentPictureSmbUsername = cleanedValue(username)
        currentPictureSmbPassword = password || ""
        currentPictureSmbGuest = guest === true
        currentPictureSmbSize = localFileStreamServer.lastResolvedSize > 0
                ? localFileStreamServer.lastResolvedSize
                : (size > 0 ? size : 0)
        currentPictureTitle = pictureTitle
        currentPictureSource = streamUrl
        syncSmbQueueIndices(cleanPath, "picture")

        if (!keepCastControlPageDuringQueueSwitch
                && (!pageStack.currentPage
                    || pageStack.currentPage.objectName !== "pictureViewerPage")) {
            pageStack.push(Qt.resolvedUrl("pages/PictureViewerPage.qml"))
        }

        if (castMode
                && castTargetKind === "picture"
                && castManager.connected) {
            castCurrentPictureToConnectedDevice()
        }

        return true
    }

    function openSmbFile(host, port, share, path, domain, username, password, guest, size, title, resumePosition) {
        var cleanPath = cleanedValue(path)
        var fileTitle = cleanedValue(title)
        if (fileTitle.length === 0) {
            fileTitle = cleanPath.length > 0 ? cleanPath.substring(cleanPath.lastIndexOf("/") + 1) : qsTr("SMB video")
        }

        var sourceUrl = smbBackend.smbUrlForFile(host, port, share, cleanPath)
        if (!sourceUrl || sourceUrl.length === 0) {
            playbackError = qsTr("The SMB file location is incomplete.")
            playbackStatus = playbackError
            return false
        }

        var streamUrl = localFileStreamServer.streamUrlForSmbFile(
                    host, port, share, cleanPath, domain, username, password, guest, size, fileTitle)
        if (!streamUrl || streamUrl.length === 0) {
            playbackError = localFileStreamServer.lastError
            playbackStatus = playbackError
            return false
        }

        currentSmbHost = cleanedValue(host)
        currentSmbPort = port > 0 ? port : 445
        currentSmbShare = cleanedValue(share)
        currentSmbPath = cleanPath
        currentSmbDomain = cleanedValue(domain)
        currentSmbUsername = cleanedValue(username)
        currentSmbPassword = password || ""
        currentSmbGuest = guest === true
        currentSmbSize = localFileStreamServer.lastResolvedSize > 0
                ? localFileStreamServer.lastResolvedSize
                : (size > 0 ? size : 0)
        syncSmbQueueIndices(cleanPath, "video")

        playSource(sourceUrl, streamUrl, fileTitle, resumePosition, true, "smb")
        return true
    }

    function playSource(sourceUrl, playbackUrl, title, resumePosition, usesStreamBridge, sourceKind) {
        var cleanSourceUrl = normalizedMediaUrl(sourceUrl)
        var cleanPlaybackUrl = normalizedMediaUrl(playbackUrl)
        if (cleanSourceUrl.length === 0 || cleanPlaybackUrl.length === 0) {
            return
        }

        var resume = 0
        if (resumePosition !== undefined && resumePosition !== null) {
            resume = Math.max(0, resumePosition)
        } else {
            resume = playbackHistory.resumePosition(cleanSourceUrl)
        }

        playbackError = ""
        playbackStatus = qsTr("Opening")
        playerSuspended = false
        playbackCompleted = false
        pendingResumePosition = resume
        pendingResumeSeek = resume > 5000
        pendingResumeAttempts = 0
        lastKnownPosition = resume
        currentMediaUrl = cleanSourceUrl
        currentPlaybackUrl = cleanPlaybackUrl
        currentMediaTitle = playbackHistory.titleForUrl(cleanSourceUrl, cleanedValue(title))
        currentUsesStreamBridge = usesStreamBridge === true
        currentSourceKind = sourceKind && sourceKind.length > 0
                ? sourceKind
                : (usesStreamBridge === true ? "bridge" : (isNetworkUrl(cleanSourceUrl) ? "network" : "local"))
        if (currentSourceKind !== "smb") {
            currentSmbHost = ""
            currentSmbPort = 445
            currentSmbShare = ""
            currentSmbPath = ""
            currentSmbDomain = ""
            currentSmbUsername = ""
            currentSmbPassword = ""
            currentSmbGuest = true
            currentSmbSize = 0
        }

        mediaPlayer.stop()
        if (!remoteQueueSwitch) {
            resetPlaybackAdjustmentsForNewVideo()
        }
        mediaPlayer.source = ""
        mediaPlayer.source = currentPlaybackUrl

        playbackHistory.addOrUpdate(currentMediaUrl,
                                    currentMediaTitle,
                                    0,
                                    resume)

        if (!keepCastControlPageDuringQueueSwitch
                && (!pageStack.currentPage
                    || pageStack.currentPage.objectName !== "playerPage")) {
            pageStack.push(Qt.resolvedUrl("pages/PlayerPage.qml"))
        }

        if (!remoteQueueSwitch) {
            mediaPlayer.play()
        } else {
            playerSuspended = true
            pendingResumeSeek = false
            pendingResumeAttempts = 0
        }

        if (pendingResumeSeek) {
            resumeSeekTimer.restart()
        }
    }

    function resumeCurrentMedia() {
        if (!hasMedia) {
            return
        }

        var resume = lastKnownPosition > 0
                ? lastKnownPosition
                : playbackHistory.resumePosition(currentMediaUrl)

        if (currentSourceKind === "smb") {
            if (!currentSmbGuest && currentSmbPassword.length === 0) {
                openRememberedSmbFromUrl(currentMediaUrl, currentMediaTitle, resume, true)
            } else {
                if (currentSmbSize <= 0) {
                    currentSmbSize = storedSourceSize(currentMediaUrl)
                }
                openSmbFile(currentSmbHost, currentSmbPort, currentSmbShare, currentSmbPath,
                            currentSmbDomain, currentSmbUsername, currentSmbPassword,
                            currentSmbGuest, currentSmbSize, currentMediaTitle, resume)
            }
        } else if (currentUsesStreamBridge) {
            openLocalFileThroughBridge(currentMediaUrl, currentMediaTitle, resume)
        } else if (isNetworkUrl(currentMediaUrl)) {
            openNetworkUrl(currentMediaUrl, currentMediaTitle, resume)
        } else {
            openMedia(currentMediaUrl, currentMediaTitle, resume)
        }
    }

    function restoreLastPlayedFromHistory() {
        var url = playbackHistory.latestUrlValue()
        console.log("SailVideo restoreLastPlayedFromHistory url=" + url
                    + " hasMedia=" + hasMedia)
        if (!url || url.length === 0 || hasMedia) {
            return
        }

        currentMediaUrl = url
        currentPlaybackUrl = ""
        currentMediaTitle = playbackHistory.latestTitleValue()
        lastKnownPosition = playbackHistory.latestPositionValue()
        pendingResumePosition = lastKnownPosition
        pendingResumeSeek = false
        pendingResumeAttempts = 0
        playerSuspended = true
        playbackCompleted = false
        playbackError = ""

        if (isSmbUrl(url)) {
            var parsed = smbBackend.parseSmbUrl(url)
            currentSourceKind = "smb"
            currentUsesStreamBridge = true
            currentSmbHost = parsed.host || ""
            currentSmbPort = parsed.port || 445
            currentSmbShare = parsed.share || ""
            currentSmbPath = parsed.path || ""
            currentSmbSize = 0
            var sourceIndex = nasSources.indexForLocation(currentSmbHost, currentSmbPort, currentSmbShare)
            currentSmbDomain = sourceIndex >= 0 ? nasSources.domainAt(sourceIndex) : ""
            currentSmbUsername = sourceIndex >= 0 ? nasSources.usernameAt(sourceIndex) : ""
            currentSmbGuest = sourceIndex >= 0 ? nasSources.guestAt(sourceIndex) : false
            currentSmbPassword = currentSmbGuest
                    ? ""
                    : smbCredentialStore.passwordFor(currentSmbHost, currentSmbPort, currentSmbShare,
                                                     currentSmbDomain, currentSmbUsername, currentSmbGuest)
            playbackStatus = currentSmbGuest || currentSmbPassword.length > 0
                    ? qsTr("Ready to resume")
                    : qsTr("Password needed")
        } else if (isNetworkUrl(url)) {
            currentSourceKind = "network"
            currentUsesStreamBridge = false
            playbackStatus = qsTr("Ready to resume")
        } else {
            currentSourceKind = localFileStreamServer.isLocalFile(url) ? "local" : "local"
            currentUsesStreamBridge = false
            playbackStatus = qsTr("Ready to resume")
        }
    }

    function suspendPlayback() {
        if (videoCastActive) {
            savePlaybackPosition(false)
            return
        }

        if (!hasMedia || playerSuspended) {
            return
        }

        savePlaybackPosition(false)
        if (mediaPlayer.position > 0) {
            lastKnownPosition = mediaPlayer.position
        }

        pendingResumeSeek = false
        pendingResumeAttempts = 0
        resumeSeekTimer.stop()
        playerSuspended = true

        if (mediaPlayer.playbackState !== MediaPlayer.StoppedState
                || currentPlaybackUrl.length > 0) {
            mediaPlayer.pause()
            mediaPlayer.stop()
            mediaPlayer.source = ""
        }

        if (!playbackCompleted) {
            playbackStatus = qsTr("Paused")
        }
    }

    function savePlaybackPosition(completed) {
        if (!hasMedia) {
            return
        }

        if (!completed && pendingResumeSeek && !videoCastActive) {
            return
        }

        var videoCast = videoCastActive
        var durationToSave = videoCast && castManager.duration > 0
                ? castManager.duration
                : mediaPlayer.duration
        var positionToSave = completed
                ? 0
                : (videoCast ? castManager.position : mediaPlayer.position)

        if (!completed && positionToSave <= 0 && lastKnownPosition > 0) {
            positionToSave = lastKnownPosition
        }

        if (!completed && positionToSave > 0) {
            lastKnownPosition = positionToSave
        }

        playbackHistory.updatePosition(currentMediaUrl,
                                       durationToSave,
                                       positionToSave)
    }

    function tryPendingResumeSeek() {
        if (!pendingResumeSeek || pendingResumePosition <= 0 || playerSuspended) {
            resumeSeekTimer.stop()
            return
        }

        var target = pendingResumePosition
        if (mediaPlayer.duration > 0) {
            target = Math.min(target, Math.max(0, mediaPlayer.duration - 1000))
        }

        if (Math.abs(mediaPlayer.position - target) < 1500) {
            pendingResumeSeek = false
            pendingResumeAttempts = 0
            lastKnownPosition = target
            resumeSeekTimer.stop()
            updatePlaybackStatus()
            return
        }

        ++pendingResumeAttempts
        if (pendingResumeAttempts > 24) {
            pendingResumeSeek = false
            pendingResumeAttempts = 0
            resumeSeekTimer.stop()
            updatePlaybackStatus()
            return
        }

        if (mediaPlayer.duration > 0
                || mediaPlayer.seekable
                || mediaPlayer.status === MediaPlayer.Loaded
                || mediaPlayer.status === MediaPlayer.Buffered
                || mediaPlayer.status === MediaPlayer.Buffering
                || mediaPlayer.playbackState === MediaPlayer.PlayingState) {
            mediaPlayer.seek(target)
            lastKnownPosition = target
        }

        resumeSeekTimer.restart()
    }

    function updatePlaybackStatus() {
        if (playbackError.length > 0) {
            playbackStatus = playbackError
            return
        }

        if (videoCastActive) {
            playbackStatus = castManager.statusText.length > 0
                    ? castManager.statusText
                    : qsTr("Casting to %1").arg(
                          castManager.deviceName.length > 0
                          ? castManager.deviceName
                          : (castLastDeviceName.length > 0
                             ? castLastDeviceName
                             : qsTr("Chromecast")))
            return
        }

        if (pendingResumeSeek) {
            playbackStatus = qsTr("Resuming")
            return
        }

        if (playerSuspended) {
            playbackStatus = playbackCompleted ? qsTr("Finished") : qsTr("Paused")
            return
        }

        switch (mediaPlayer.status) {
        case MediaPlayer.NoMedia:
            playbackStatus = ""
            break
        case MediaPlayer.Loading:
            playbackStatus = qsTr("Loading")
            break
        case MediaPlayer.Loaded:
            playbackStatus = mediaPlayer.playbackState === MediaPlayer.PausedState
                    ? qsTr("Paused")
                    : ""
            break
        case MediaPlayer.Buffering:
            playbackStatus = qsTr("Buffering")
            break
        case MediaPlayer.Stalled:
            playbackStatus = qsTr("Playback stalled")
            break
        case MediaPlayer.EndOfMedia:
            playbackStatus = qsTr("Finished")
            break
        case MediaPlayer.InvalidMedia:
            playbackStatus = qsTr("This video could not be played")
            break
        default:
            if (mediaPlayer.playbackState === MediaPlayer.PausedState) {
                playbackStatus = qsTr("Paused")
            } else {
                playbackStatus = ""
            }
            break
        }
    }

    Connections {
        target: castManager

        onConnectedChanged: {
            if (castManager.connected) {
                castRequested = true
                playbackError = ""
                updatePlaybackStatus()
                return
            }

            if (castDisconnectOnly && !castManager.disconnecting) {
                completeCastDisconnectOnly()
                return
            }

            if (castResumeLocalAfterDisconnect && !castManager.disconnecting) {
                completeCastReturnToPhone()
                return
            }

            if (handleCastFailureIfIdle()) {
                return
            }
        }

        onCastingChanged: {
            if (castManager.casting) {
                castRequested = true
                castRejoinPending = false
                if (castManager.mediaInfoKnown && !castManager.imageMedia) {
                    castPictureSlideshowRunning = false
                }
                castLastDeviceName = castManager.deviceName
                castLastHost = castManager.host
                castLastPort = castManager.port > 0 ? castManager.port : 8009
                appSettings.setDetachedCastSession(
                            castLastDeviceName,
                            castLastHost,
                            castLastPort)
            }
            updatePlaybackStatus()
        }

        onMediaInfoChanged: {
            if (castManager.mediaInfoKnown) {
                if (castRejoinPending) {
                    // Rejoin has no page/source intent, so the receiver is the
                    // only authoritative source for the current media type.
                    castTargetKind = castManager.imageMedia ? "picture" : "video"
                }
                if (!castManager.imageMedia) {
                    castPictureSlideshowRunning = false
                }
            }
            updatePlaybackStatus()
        }

        onPlayingChanged: updatePlaybackStatus()

        onPositionChanged: {
            if (videoCastActive
                    && castManager.position > 0) {
                lastKnownPosition = castManager.position
            }
        }

        onStatusTextChanged: {
            if (videoCastActive) {
                updatePlaybackStatus()
            }
        }

        onLastErrorChanged: {
            if (castManager.lastError.length > 0) {
                playbackError = castManager.lastError
                playbackStatus = playbackError
                handleCastFailureIfIdle()
            }
        }

        onPlaybackFinished: {
            if (castActivePicture) {
                return
            }
            playbackCompleted = true
            lastKnownPosition = 0
            savePlaybackPosition(true)
            playbackStatus = qsTr("Finished on %1").arg(
                        castManager.deviceName.length > 0
                        ? castManager.deviceName
                        : qsTr("Chromecast"))
        }
    }

    Connections {
        target: appWindow.urlDownloadCacheObject

        onDownloadProgress: {
            if (cleanedValue(sourceUrl) !== pendingNetworkSourceUrl) {
                return
            }

            if (bytesTotal > 0) {
                playbackStatus = qsTr("Downloading URL: %1 / %2")
                        .arg(formatBytes(bytesReceived))
                        .arg(formatBytes(bytesTotal))
            } else {
                playbackStatus = qsTr("Downloading URL: %1")
                        .arg(formatBytes(bytesReceived))
            }
        }

        onReady: {
            playPreparedNetworkUrl(sourceUrl, fileUrl)
        }

        onFailed: {
            if (cleanedValue(sourceUrl) === pendingNetworkSourceUrl
                    || currentSourceKind === "network") {
                playbackError = message
                playbackStatus = message
            }
        }
    }

    MediaPlayer {
        id: mediaPlayer
        autoLoad: true

        onStatusChanged: {
            if (status === MediaPlayer.EndOfMedia) {
                appWindow.playbackCompleted = true
                appWindow.pendingResumeSeek = false
                appWindow.pendingResumeAttempts = 0
                resumeSeekTimer.stop()
                appWindow.lastKnownPosition = 0
                appWindow.savePlaybackPosition(true)
            } else if (status !== MediaPlayer.NoMedia) {
                appWindow.playbackCompleted = false
                appWindow.tryPendingResumeSeek()
            }

            appWindow.updatePlaybackStatus()
        }

        onPlaybackStateChanged: {
            appWindow.updatePlaybackStatus()
            if (!appWindow.playerSuspended
                    && playbackState !== MediaPlayer.PlayingState
                    && status !== MediaPlayer.NoMedia
                    && status !== MediaPlayer.EndOfMedia) {
                appWindow.savePlaybackPosition(false)
            }
        }

        onPositionChanged: {
            if (appWindow.pendingResumeSeek
                    && Math.abs(position - appWindow.pendingResumePosition) < 1500) {
                appWindow.pendingResumeSeek = false
                appWindow.pendingResumeAttempts = 0
                resumeSeekTimer.stop()
                appWindow.updatePlaybackStatus()
            }

            if (!appWindow.playerSuspended && position > 0) {
                appWindow.lastKnownPosition = position
            }
        }

        onDurationChanged: appWindow.tryPendingResumeSeek()
        onSeekableChanged: appWindow.tryPendingResumeSeek()

        onError: {
            appWindow.playbackError = errorString && errorString.length > 0
                    ? errorString
                    : qsTr("Playback error")
            if (appWindow.playbackError.toLowerCase().indexOf("corrupt") >= 0) {
                appWindow.playbackError = appWindow.playbackError + "\n"
                        + qsTr("The decoder rejected this file. It may be damaged, incomplete, or encoded with an unsupported codec/profile.")
            }
            appWindow.updatePlaybackStatus()
        }
    }

    Timer {
        id: resumeSeekTimer
        interval: 300
        repeat: false
        onTriggered: appWindow.tryPendingResumeSeek()
    }

    Timer {
        id: seekSettleTimer
        interval: 650
        repeat: false
        onTriggered: {
            appWindow.userSeekPending = false
            appWindow.updatePlaybackStatus()
        }
    }

    Timer {
        id: adjustmentStatusTimer
        interval: 1200
        repeat: false
        onTriggered: appWindow.adjustmentStatus = ""
    }

    Timer {
        id: castPictureSlideshowTimer
        interval: Math.max(1, appSettings.pictureSlideshowSeconds) * 1000
        repeat: true
        running: appWindow.castPictureSlideshowRunning
                 && appWindow.castActivePicture
                 && appWindow.pictureQueue.length > 1
        onTriggered: appWindow.advanceCastPictureSlideshow()
    }

    Timer {
        id: castRejoinStartupTimer
        interval: 100
        repeat: false
        onTriggered: appWindow.tryRejoinDetachedCast(true)
    }

    Timer {
        id: historySaveTimer
        interval: 5000
        repeat: true
        running: appWindow.hasMedia
                 && ((appWindow.videoCastActive && castManager.playing)
                     || (!appWindow.videoCastActive
                         && !appWindow.playerSuspended
                         && mediaPlayer.playbackState === MediaPlayer.PlayingState))
        onTriggered: appWindow.savePlaybackPosition(false)
    }

    initialPage: Component {
        MainPage {}
    }

    cover: Qt.resolvedUrl("cover/CoverPage.qml")

    Component.onCompleted: {
        bindUrlDownloadCache()
        logHistoryState("SailVideo startup")
        if (initialMediaUrl && String(initialMediaUrl).length > 0) {
            if (isNetworkUrl(String(initialMediaUrl))) {
                openNetworkUrl(String(initialMediaUrl), "")
            } else if (isSmbUrl(String(initialMediaUrl))) {
                openRememberedSmbFromUrl(String(initialMediaUrl), "", undefined, false)
            } else {
                openMedia(initialMediaUrl, "")
            }
        } else {
            restoreLastPlayedFromHistory()
            if (appSettings.hasDetachedCastSession) {
                castRejoinStartupTimer.start()
            }
        }
    }

    Component.onDestruction: {
        if (castMode
                && !castManager.mediaStopped
                && (castManager.connected || castManager.casting)
                && (castManager.host.length > 0 || castLastHost.length > 0)) {
            appSettings.setDetachedCastSession(
                        castManager.deviceName.length > 0 ? castManager.deviceName : castLastDeviceName,
                        castManager.host.length > 0 ? castManager.host : castLastHost,
                        castManager.port > 0 ? castManager.port : castLastPort)
        } else if (castMode && castManager.mediaStopped) {
            appSettings.clearDetachedCastSession()
        }
        castDeviceModel.stopDiscovery()
        suspendPlayback()
    }
}
