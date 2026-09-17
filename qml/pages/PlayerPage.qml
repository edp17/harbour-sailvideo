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
import "../components"

Page {
    id: page

    objectName: "playerPage"
    allowedOrientations: Orientation.All

    property bool controlsVisible: true
    property bool leavingPage: false
    property real gestureStartX: 0
    property real gestureLastY: 0
    property bool gestureActive: false
    property real videoAspectRatio: 16.0 / 9.0
    property int aspectRefreshAttempts: 0

    function leavePlayer() {
        if (leavingPage) {
            return
        }
        leavingPage = true
        appWindow.leavePlayerView()
    }

    function updateVideoAspectRatio() {
        var w = 0
        var h = 0

        try {
            if (videoOutput && videoOutput.sourceRect
                    && videoOutput.sourceRect.width > 0
                    && videoOutput.sourceRect.height > 0) {
                w = videoOutput.sourceRect.width
                h = videoOutput.sourceRect.height
            }
        } catch (ignoredSourceRectError) {
        }

        if (w <= 0 || h <= 0) {
            try {
                var md = appWindow.player.metaData
                var res = md ? (md.resolution || md.videoResolution || md.Resolution) : null
                if (res && res.width > 0 && res.height > 0) {
                    w = res.width
                    h = res.height
                }
            } catch (ignoredMetadataError) {
            }
        }

        if (w > 0 && h > 0) {
            var ratio = w / h
            if (ratio > 0.2 && ratio < 6.0) {
                videoAspectRatio = ratio
            }
        }

        refreshVideoGeometry()
    }

    function fitVideoWidth() {
        if (page.width <= 0 || page.height <= 0 || videoAspectRatio <= 0) {
            return page.width
        }

        var pageRatio = page.width / Math.max(1, page.height)
        return pageRatio > videoAspectRatio ? page.height * videoAspectRatio : page.width
    }

    function fitVideoHeight() {
        if (page.width <= 0 || page.height <= 0 || videoAspectRatio <= 0) {
            return page.height
        }

        var pageRatio = page.width / Math.max(1, page.height)
        return pageRatio > videoAspectRatio ? page.height : page.width / videoAspectRatio
    }

    function cropScale() {
        var baseWidth = Math.max(1, fitVideoWidth())
        var baseHeight = Math.max(1, fitVideoHeight())
        return Math.max(page.width / baseWidth, page.height / baseHeight)
    }

    function surfaceScaleXForMode() {
        if (appSettings.videoFillMode === "stretch") {
            return page.width / Math.max(1, fitVideoWidth())
        }
        if (appSettings.videoFillMode === "crop") {
            return cropScale()
        }
        return 1.0
    }

    function surfaceScaleYForMode() {
        if (appSettings.videoFillMode === "stretch") {
            return page.height / Math.max(1, fitVideoHeight())
        }
        if (appSettings.videoFillMode === "crop") {
            return cropScale()
        }
        return 1.0
    }

    function refreshVideoGeometry() {
        if (!videoSurface || !videoOutput) {
            return
        }

        // Keep the native VideoOutput itself stable. We only resize/transform
        // the ordinary QML wrapper item around it. This avoids the r8d black
        // surface problem while keeping r8c's working Crop behaviour.
        videoOutput.fillMode = VideoOutput.Stretch
    }

    function seekBack() {
        appWindow.requestSeek(Math.max(0, appWindow.playbackPosition() - appWindow.skipMilliseconds()))
        controlsHideTimer.restart()
    }

    function seekForward() {
        var duration = appWindow.playbackDuration()
        if (duration > 0) {
            appWindow.requestSeek(Math.min(duration,
                                           appWindow.playbackPosition() + appWindow.skipMilliseconds()))
        }
        controlsHideTimer.restart()
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Item {
        id: videoFrame

        anchors.fill: parent
        clip: true

        Item {
            id: videoSurface

            anchors.centerIn: parent
            width: Math.max(1, page.fitVideoWidth())
            height: Math.max(1, page.fitVideoHeight())

            transform: Scale {
                origin.x: videoSurface.width / 2
                origin.y: videoSurface.height / 2
                xScale: page.surfaceScaleXForMode()
                yScale: page.surfaceScaleYForMode()
            }

            VideoOutput {
                id: videoOutput

                anchors.fill: parent
                source: appWindow.player
                fillMode: VideoOutput.Stretch
            }
        }
    }

    Connections {
        target: appSettings
        onVideoFillModeChanged: page.refreshVideoGeometry()
    }

    Rectangle {
        anchors.fill: parent
        visible: opacity > 0
        color: "black"
        opacity: appWindow.videoDimming
    }

    MouseArea {
        anchors.fill: parent

        onPressed: {
            page.gestureStartX = mouse.x
            page.gestureLastY = mouse.y
            page.gestureActive = false
        }

        onPositionChanged: {
            if (!pressed) {
                return
            }

            var deltaY = page.gestureLastY - mouse.y
            if (Math.abs(deltaY) < 18) {
                return
            }

            page.gestureActive = true
            page.controlsVisible = true
            page.gestureLastY = mouse.y

            if (page.gestureStartX < width / 2) {
                if (!appWindow.videoCastActive) {
                    appWindow.adjustVideoBrightness(deltaY > 0 ? 5 : -5)
                }
            } else {
                appWindow.adjustVolume(deltaY > 0 ? 5 : -5)
            }
            controlsHideTimer.restart()
        }

        onReleased: {
            if (!page.gestureActive) {
                page.controlsVisible = !page.controlsVisible
                if (page.controlsVisible) {
                    controlsHideTimer.restart()
                }
            }
            page.gestureActive = false
        }
    }

    Item {
        id: gestureHints

        visible: page.controlsVisible
        opacity: page.gestureActive ? 0.95 : 0.75
        anchors.fill: parent

        Rectangle {
            id: brightnessHint

            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            visible: !appWindow.videoCastActive
            width: parent.width / 2
            color: "#30209fd6"

            Column {
                anchors.centerIn: parent
                spacing: Theme.paddingMedium

                Label {
                    width: brightnessHint.width
                    text: "↑"
                    color: "white"
                    opacity: 0.9
                    font.pixelSize: Theme.fontSizeExtraLarge
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: brightnessHint.width
                    text: qsTr("Brightness")
                    color: "white"
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: brightnessHint.width
                    text: "↓"
                    color: "white"
                    opacity: 0.9
                    font.pixelSize: Theme.fontSizeExtraLarge
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        Rectangle {
            id: volumeHint

            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            width: parent.width / 2
            color: "#3049a078"

            Column {
                anchors.centerIn: parent
                spacing: Theme.paddingMedium

                Label {
                    width: volumeHint.width
                    text: "↑"
                    color: "white"
                    opacity: 0.9
                    font.pixelSize: Theme.fontSizeExtraLarge
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: volumeHint.width
                    text: appWindow.videoCastActive ? qsTr("Cast volume") : qsTr("Volume")
                    color: "white"
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: volumeHint.width
                    text: "↓"
                    color: "white"
                    opacity: 0.9
                    font.pixelSize: Theme.fontSizeExtraLarge
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    SilicaFlickable {
        id: topMenuFlickable
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: Theme.itemSizeLarge
        contentWidth: width
        contentHeight: height
        flickableDirection: Flickable.VerticalFlick
        clip: false
        z: 30

        CastPulleyMenu {
            sourceKind: "video"
        }

        Rectangle {
            id: topPanel
            width: topMenuFlickable.width
            height: topMenuFlickable.height
            visible: page.controlsVisible
            color: "#99000000"

            IconButton {
                id: backButton
                anchors {
                    left: parent.left
                    leftMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                icon.source: "image://theme/icon-m-back"
                onClicked: {
                    page.leavePlayer()
                    pageStack.pop()
                }
            }

            Label {
                anchors {
                    left: backButton.right
                    right: parent.right
                    leftMargin: Theme.paddingSmall
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                text: appWindow.currentMediaTitle
                color: "white"
                truncationMode: TruncationMode.Fade
            }
        }
    }

    CastControlOverlay {
        sourceKind: "video"
        controlsVisible: page.controlsVisible
        anchors {
            top: topMenuFlickable.bottom
            horizontalCenter: parent.horizontalCenter
            topMargin: Theme.paddingSmall
        }
        z: 29
    }

    Rectangle {
        id: statusPanel

        visible: appWindow.playbackStatus.length > 0
                 && (page.controlsVisible
                     || appWindow.player.status === MediaPlayer.Buffering
                     || appWindow.player.status === MediaPlayer.Stalled
                     || appWindow.playbackError.length > 0
                     || appWindow.videoCastActive)
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.horizontalPageMargin,
                        statusLabel.implicitWidth + 2 * Theme.paddingLarge)
        height: statusLabel.implicitHeight + 2 * Theme.paddingMedium
        radius: Theme.paddingMedium
        color: "#aa000000"

        Label {
            id: statusLabel

            anchors.centerIn: parent
            width: Math.min(implicitWidth,
                            statusPanel.width - 2 * Theme.paddingLarge)
            text: appWindow.playbackStatus
            color: appWindow.playbackError.length > 0
                   ? Theme.errorColor
                   : "white"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    Rectangle {
        id: adjustmentPanel

        visible: appWindow.adjustmentStatus.length > 0
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: bottomPanel.visible ? bottomPanel.top : parent.bottom
            bottomMargin: Theme.paddingLarge
        }
        width: Math.min(parent.width - 2 * Theme.horizontalPageMargin,
                        adjustmentLabel.implicitWidth + 2 * Theme.paddingLarge)
        height: adjustmentLabel.implicitHeight + 2 * Theme.paddingMedium
        radius: Theme.paddingMedium
        color: "#cc000000"

        Label {
            id: adjustmentLabel
            anchors.centerIn: parent
            text: appWindow.adjustmentStatus
            color: "white"
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Rectangle {
        id: bottomPanel

        visible: page.controlsVisible
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: controlsColumn.height + 2 * Theme.paddingMedium
        color: "#aa000000"

        Column {
            id: controlsColumn

            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                margins: Theme.paddingMedium
            }
            spacing: Theme.paddingSmall

            Item {
                width: parent.width
                height: Theme.itemSizeMedium

                Row {
                    anchors.centerIn: parent
                    spacing: Theme.paddingSmall

                    IconButton {
                        id: previousVideoButton

                        enabled: appWindow.hasPreviousVideoControl
                        opacity: enabled ? 1.0 : 0.35
                        icon.source: "image://theme/icon-m-previous"

                        onClicked: {
                            appWindow.showPreviousVideoControl()
                            controlsHideTimer.restart()
                        }
                    }

                    BackgroundItem {
                        id: skipBackButton

                        width: Theme.itemSizeMedium
                        height: Theme.itemSizeMedium

                        onClicked: page.seekBack()

                        Label {
                            anchors.centerIn: parent
                            text: qsTr("-%1s").arg(appSettings.skipSeconds)
                            color: skipBackButton.highlighted ? Theme.highlightColor : "white"
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }
                    }

                    IconButton {
                        id: restartButton

                        icon.source: "image://theme/icon-m-refresh"

                        onClicked: {
                            appWindow.restartCurrentMedia()
                            controlsHideTimer.restart()
                        }
                    }

                    IconButton {
                        id: playPauseButton

                        icon.source: appWindow.playbackIsPlaying()
                                     ? "image://theme/icon-m-pause"
                                     : "image://theme/icon-m-play"

                        onClicked: {
                            appWindow.togglePlayback()
                            controlsHideTimer.restart()
                        }
                    }

                    BackgroundItem {
                        id: fillModeButton

                        width: Theme.itemSizeMedium
                        height: Theme.itemSizeMedium
                        enabled: !appWindow.videoCastActive
                        opacity: enabled ? 1.0 : 0.35

                        onClicked: {
                            appWindow.cycleVideoFillMode()
                            page.refreshVideoGeometry()
                            controlsHideTimer.restart()
                        }

                        Label {
                            anchors.centerIn: parent
                            text: appWindow.fillModeShortLabel()
                            color: fillModeButton.highlighted ? Theme.highlightColor : "white"
                            font.pixelSize: Theme.fontSizeTiny
                        }
                    }

                    BackgroundItem {
                        id: skipForwardButton

                        width: Theme.itemSizeMedium
                        height: Theme.itemSizeMedium

                        onClicked: page.seekForward()

                        Label {
                            anchors.centerIn: parent
                            text: qsTr("+%1s").arg(appSettings.skipSeconds)
                            color: skipForwardButton.highlighted ? Theme.highlightColor : "white"
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }
                    }

                    IconButton {
                        id: nextVideoButton

                        enabled: appWindow.hasNextVideoControl
                        opacity: enabled ? 1.0 : 0.35
                        icon.source: "image://theme/icon-m-next"

                        onClicked: {
                            appWindow.showNextVideoControl()
                            controlsHideTimer.restart()
                        }
                    }
                }
            }

            Label {
                width: parent.width
                text: appWindow.videoCastActive
                      ? qsTr("Right swipe: Cast volume · Remote controls active")
                      : qsTr("Left swipe: video brightness · Right swipe: media volume · Scaling: %1")
                            .arg(appWindow.fillModeShortLabel())
                color: "white"
                opacity: 0.7
                font.pixelSize: Theme.fontSizeTiny
                horizontalAlignment: Text.AlignHCenter
                truncationMode: TruncationMode.Fade
            }

            Item {
                width: parent.width
                height: Theme.paddingLarge

                Rectangle {
                    id: seekBackground

                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    height: Math.max(Theme.paddingSmall / 2, 3)
                    radius: height / 2
                    color: "#66ffffff"

                    Rectangle {
                        width: appWindow.playbackDuration() > 0
                               ? parent.width * appWindow.playbackPosition()
                                 / appWindow.playbackDuration()
                               : 0
                        height: parent.height
                        radius: height / 2
                        color: Theme.highlightColor
                    }

                    MouseArea {
                        anchors {
                            fill: parent
                            topMargin: -Theme.paddingLarge
                            bottomMargin: -Theme.paddingLarge
                        }

                        function seekToPointer(mouseX) {
                            if (appWindow.playbackDuration() <= 0) {
                                return
                            }

                            var boundedX = Math.max(0, Math.min(width, mouseX))
                            var target = Math.round(
                                appWindow.playbackDuration() * boundedX / width)
                            appWindow.requestSeek(target)
                        }

                        onReleased: seekToPointer(mouse.x)
                    }
                }
            }

            Item {
                width: parent.width
                height: Theme.fontSizeSmall

                Label {
                    anchors {
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                    }
                    text: appWindow.formatTime(appWindow.playbackPosition())
                    color: "white"
                    font.pixelSize: Theme.fontSizeExtraSmall
                }

                Label {
                    anchors.centerIn: parent
                    visible: (!appWindow.videoCastActive && appWindow.pendingResumeSeek)
                             || appWindow.userSeekPending
                    text: appWindow.userSeekPending ? qsTr("Seeking") : qsTr("Resuming")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }

                Label {
                    anchors {
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    text: appWindow.formatTime(appWindow.playbackDuration())
                    color: "white"
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }

            Label {
                visible: appWindow.playbackError.length > 0
                width: parent.width
                text: appWindow.playbackError
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Timer {
        id: controlsHideTimer

        interval: 4000
        repeat: false
        running: page.controlsVisible
        onTriggered: {
            if (appWindow.playbackIsPlaying()) {
                page.controlsVisible = false
            }
        }
    }

    Connections {
        target: appWindow.player

        onPlaybackStateChanged: {
            if (appWindow.player.playbackState !== MediaPlayer.PlayingState) {
                page.controlsVisible = true
                controlsHideTimer.stop()
            }
        }

        onStatusChanged: {
            aspectRefreshAttempts = 0
            aspectRefreshTimer.restart()
            page.updateVideoAspectRatio()
        }
    }

    Connections {
        target: castManager

        onPlayingChanged: {
            if (!appWindow.playbackIsPlaying()) {
                page.controlsVisible = true
                controlsHideTimer.stop()
            } else if (page.controlsVisible) {
                controlsHideTimer.restart()
            }
        }
    }

    Timer {
        id: aspectRefreshTimer
        interval: 350
        repeat: true
        onTriggered: {
            page.updateVideoAspectRatio()
            aspectRefreshAttempts += 1
            if (aspectRefreshAttempts >= 10) {
                stop()
            }
        }
    }

    onWidthChanged: page.refreshVideoGeometry()
    onHeightChanged: page.refreshVideoGeometry()

    Component.onCompleted: {
        aspectRefreshAttempts = 0
        aspectRefreshTimer.restart()
        page.updateVideoAspectRatio()
        controlsHideTimer.restart()
    }
    Component.onDestruction: page.leavePlayer()
}
