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

    property string currentPictureTitle: ""
    property string currentPictureSource: ""
    property real videoDimming: 0.0
    property int playbackVolumePercent: 30
    property string pendingNetworkSourceUrl: ""
    property string pendingNetworkTitle: ""
    property int pendingNetworkResumePosition: 0
    property var urlDownloadCacheObject: null
    property string adjustmentStatus: ""
    property bool userSeekPending: false


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
        return playbackVolumePercent
    }

    function setVolumePercent(value) {
        var bounded = Math.max(0, Math.min(100, Math.round(value)))
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

    function requestSeek(targetPosition) {
        if (!hasMedia || mediaPlayer.duration <= 0) {
            return
        }

        if (userSeekPending) {
            return
        }

        var safeTarget = Math.max(0, Math.min(mediaPlayer.duration, Math.round(targetPosition)))
        if (mediaPlayer.duration > 5000) {
            safeTarget = Math.min(safeTarget, mediaPlayer.duration - 1000)
        }

        pendingResumeSeek = false
        pendingResumeAttempts = 0
        resumeSeekTimer.stop()
        userSeekPending = true
        playbackStatus = qsTr("Seeking")
        lastKnownPosition = safeTarget
        playbackHistory.updatePosition(currentMediaUrl, mediaPlayer.duration, safeTarget)
        mediaPlayer.seek(safeTarget)
        seekSettleTimer.restart()
    }

    function restartCurrentMedia() {
        if (!hasMedia) {
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

    function setSmbPictureQueueFromModel(model, selectedPath, host, port, share, domain, username, password, guest) {
        var queue = []
        var selected = -1
        var cleanSelectedPath = cleanedValue(selectedPath)

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

            var cleanPath = cleanedValue(entry.path)
            var item = {
                kind: "smb",
                host: cleanedValue(host),
                port: port > 0 ? port : 445,
                share: cleanedValue(share),
                path: cleanPath,
                domain: cleanedValue(domain),
                username: cleanedValue(username),
                password: password || "",
                guest: guest === true,
                size: entry.size > 0 ? entry.size : 0,
                title: cleanedValue(entry.name)
            }

            if (cleanPath === cleanSelectedPath) {
                selected = queue.length
            }
            queue.push(item)
        }

        pictureQueue = queue
        pictureQueueIndex = selected
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
        var cleanSelectedPath = cleanedValue(selectedPath)

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

            var cleanPath = cleanedValue(entry.path)
            var item = {
                kind: "smb",
                host: cleanedValue(host),
                port: port > 0 ? port : 445,
                share: cleanedValue(share),
                path: cleanPath,
                domain: cleanedValue(domain),
                username: cleanedValue(username),
                password: password || "",
                guest: guest === true,
                size: entry.size > 0 ? entry.size : 0,
                title: cleanedValue(entry.name)
            }

            if (cleanPath === cleanSelectedPath) {
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

        var item = playbackQueue[index]
        playbackQueueIndex = index
        if (item.kind === "smb") {
            return openSmbFile(item.host, item.port, item.share, item.path,
                               item.domain, item.username, item.password,
                               item.guest, item.size, item.title, 0)
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
        var playbackUrl = networkSources.normalizedUrl(mediaUrl)
        if (!playbackUrl || playbackUrl.length === 0) {
            playbackError = networkSources.lastError
            playbackStatus = playbackError
            return false
        }

        clearPlaybackQueue()
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

    function openMedia(mediaUrl, title, resumePosition) {
        clearPlaybackQueue()
        playSource(mediaUrl, mediaUrl, title, resumePosition, false)
    }

    function openLocalFileThroughBridge(mediaUrl, title, resumePosition) {
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

        clearPlaybackQueue()
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

        currentPictureTitle = pictureTitle
        currentPictureSource = streamUrl

        if (!pageStack.currentPage
                || pageStack.currentPage.objectName !== "pictureViewerPage") {
            pageStack.push(Qt.resolvedUrl("pages/PictureViewerPage.qml"))
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
        resetPlaybackAdjustmentsForNewVideo()
        mediaPlayer.source = ""
        mediaPlayer.source = currentPlaybackUrl

        playbackHistory.addOrUpdate(currentMediaUrl,
                                    currentMediaTitle,
                                    0,
                                    resume)

        if (!pageStack.currentPage
                || pageStack.currentPage.objectName !== "playerPage") {
            pageStack.push(Qt.resolvedUrl("pages/PlayerPage.qml"))
        }

        mediaPlayer.play()

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

        if (!completed && pendingResumeSeek) {
            return
        }

        var positionToSave = completed ? 0 : mediaPlayer.position
        if (!completed && positionToSave <= 0 && lastKnownPosition > 0) {
            positionToSave = lastKnownPosition
        }

        if (!completed && positionToSave > 0) {
            lastKnownPosition = positionToSave
        }

        playbackHistory.updatePosition(currentMediaUrl,
                                       mediaPlayer.duration,
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
        id: historySaveTimer
        interval: 5000
        repeat: true
        running: appWindow.hasMedia
                 && !appWindow.playerSuspended
                 && mediaPlayer.playbackState === MediaPlayer.PlayingState
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
        }
    }

    Component.onDestruction: suspendPlayback()
}
