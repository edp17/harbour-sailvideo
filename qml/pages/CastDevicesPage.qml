/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    objectName: "castDevicesPage"
    allowedOrientations: Orientation.All

    property string lastAttemptName: ""
    property string lastAttemptHost: ""
    property int lastAttemptPort: 8009
    property bool activePictureCast: appWindow.castActivePicture
    property bool requestedPictureCast: appWindow.castTargetKind === "picture"
    property bool mediaTypeMismatch: appWindow.castMode
                                     && castManager.mediaInfoKnown
                                     && activePictureCast !== requestedPictureCast

    function attemptDevice(name, host, port) {
        page.lastAttemptName = name || ""
        page.lastAttemptHost = host || ""
        page.lastAttemptPort = port > 0 ? port : 8009
        appWindow.startCastingToDevice(page.lastAttemptName,
                                       page.lastAttemptHost,
                                       page.lastAttemptPort)
    }

    function retryLastConnection() {
        if (page.lastAttemptHost.length > 0) {
            page.attemptDevice(page.lastAttemptName,
                               page.lastAttemptHost,
                               page.lastAttemptPort)
        } else if (appSettings.hasDetachedCastSession) {
            appWindow.tryRejoinDetachedCast(false)
        }
    }

    SilicaListView {
        id: deviceList

        anchors.fill: parent
        model: castDeviceModel
        clip: true

        PullDownMenu {
            MenuItem {
                text: castDeviceModel.discovering
                      ? qsTr("Stop Chromecast scan")
                      : qsTr("Scan for Chromecast devices")
                onClicked: {
                    if (castDeviceModel.discovering) {
                        castDeviceModel.stopDiscovery()
                    } else {
                        castDeviceModel.startDiscovery()
                    }
                }
            }
        }

        header: Column {
            width: deviceList.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Chromecast")
            }

            Column {
                visible: appWindow.castMode
                width: parent.width
                spacing: Theme.paddingMedium

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: castManager.rejoining
                          ? qsTr("Reconnecting to %1").arg(
                                castManager.deviceName.length > 0
                                ? castManager.deviceName
                                : qsTr("Chromecast"))
                          : qsTr("%1 %2")
                                .arg(page.activePictureCast
                                     ? qsTr("Displaying on")
                                     : qsTr("Casting to"))
                                .arg(castManager.deviceName.length > 0
                                     ? castManager.deviceName
                                     : qsTr("Chromecast"))
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeLarge
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: castManager.statusText
                    visible: text.length > 0
                    color: Theme.secondaryColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: page.activePictureCast
                          ? (appWindow.currentPictureTitle.length > 0
                             ? appWindow.currentPictureTitle
                             : qsTr("Picture"))
                          : (appWindow.currentMediaTitle.length > 0
                             ? appWindow.currentMediaTitle
                             : qsTr("Video"))
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                // Keep this navigation row above every media-specific
                // control. Picture <-> video switches must never move
                // Previous/Next underneath the user's finger.
                Row {
                    visible: appWindow.castMode
                             && (appWindow.hasPreviousVideoControl
                                 || appWindow.hasNextVideoControl
                                 || appWindow.hasPreviousPictureControl
                                 || appWindow.hasNextPictureControl)
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.paddingMedium

                    Button {
                        text: qsTr("Previous")
                        enabled: page.activePictureCast
                                 ? appWindow.hasPreviousPictureControl
                                 : appWindow.hasPreviousVideoControl
                        onClicked: appWindow.showPreviousCastMediaFromControlPage()
                    }

                    Button {
                        text: qsTr("Next")
                        enabled: page.activePictureCast
                                 ? appWindow.hasNextPictureControl
                                 : appWindow.hasNextVideoControl
                        onClicked: appWindow.showNextCastMediaFromControlPage()
                    }
                }

                Button {
                    visible: page.mediaTypeMismatch
                             && (page.requestedPictureCast
                                 ? appWindow.currentPictureSource.length > 0
                                 : appWindow.hasMedia)
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: page.requestedPictureCast
                          ? qsTr("Cast current picture")
                          : qsTr("Cast current video")
                    enabled: castManager.connected && !castManager.disconnecting
                    onClicked: appWindow.castRequestedMediaToConnectedDevice(false)
                }

                Button {
                    visible: page.requestedPictureCast
                             && appWindow.pictureQueue.length > 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: appWindow.castPictureSlideshowRunning
                          ? qsTr("Stop slideshow")
                          : qsTr("Start slideshow")
                    enabled: castManager.connected && !castManager.disconnecting
                    onClicked: appWindow.toggleCastPictureSlideshowFromControlPage()
                }

                Row {
                    visible: !page.activePictureCast
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.paddingMedium

                    Button {
                        text: castManager.muted ? qsTr("Unmute") : qsTr("Mute")
                        enabled: castManager.connected
                        onClicked: castManager.setMuted(!castManager.muted)
                    }

                    Button {
                        text: castManager.mediaStopped
                              ? qsTr("Start / continue")
                              : qsTr("Stop media")
                        enabled: castManager.connected
                                 && (castManager.mediaStopped || castManager.casting)
                        onClicked: {
                            if (castManager.mediaStopped) {
                                castManager.continueMedia()
                            } else {
                                castManager.stopMedia()
                            }
                        }
                    }
                }

                Button {
                    visible: appWindow.castMode
                             && ((page.requestedPictureCast
                                  && appWindow.currentPictureSource.length > 0)
                                 || (!page.requestedPictureCast && appWindow.hasMedia))
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: page.requestedPictureCast
                          ? qsTr("Return to picture")
                          : qsTr("Return to player")
                    onClicked: appWindow.returnToActiveCastMedia()
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: page.activePictureCast
                          ? qsTr("Disconnect Chromecast")
                          : qsTr("Disconnect and resume on phone")
                    enabled: !castManager.disconnecting
                    onClicked: appWindow.disconnectCastAndResume()
                }

                Button {
                    visible: !page.activePictureCast
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Leave playing on TV")
                    enabled: !castManager.disconnecting && !castManager.mediaStopped
                    onClicked: appWindow.detachCastKeepPlaying()
                }

                Label {
                    visible: !page.activePictureCast
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("Leaving playback on the TV closes only SailVideo's sender connection. Local and NAS media still depends on SailVideo staying open because the phone is serving it over the LAN.")
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    visible: page.activePictureCast
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("Picture display remains active when you leave this page. Use Return to picture to continue browsing, or start a slideshow to advance pictures automatically.")
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Separator {
                    width: parent.width
                    color: Theme.highlightColor
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: appWindow.castMode
                      ? qsTr("The active Chromecast is connected.")
                      : (castDeviceModel.count > 0
                         ? qsTr("Remembered Chromecast devices")
                         : (castDeviceModel.discovering
                            ? qsTr("Searching for Chromecast devices…")
                            : qsTr("No remembered Chromecast devices")))
                color: castDeviceModel.discovering
                       ? Theme.highlightColor
                       : Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Label {
                visible: !appWindow.castMode && castDeviceModel.count > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Tap a remembered device to connect. Use the pulley menu only when you want to refresh its address or discover another Chromecast.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: castDeviceModel.discovering
                visible: running
                size: BusyIndicatorSize.Small
            }

            Label {
                visible: castDeviceModel.lastError.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: castDeviceModel.lastError
                color: Theme.errorColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Label {
                visible: !appWindow.castMode && castManager.lastError.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: castManager.lastError
                color: Theme.errorColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Button {
                visible: !appWindow.castMode
                         && castManager.lastError.length > 0
                         && (page.lastAttemptHost.length > 0
                             || appSettings.hasDetachedCastSession)
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Retry connection")
                onClicked: page.retryLastConnection()
            }
        }

        delegate: ListItem {
            id: deviceItem

            width: deviceList.width
            contentHeight: Theme.itemSizeLarge
            enabled: !appWindow.castMode
            opacity: enabled ? 1.0 : 0.45

            onClicked: page.attemptDevice(name, host, port)

            Column {
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                spacing: Theme.paddingSmall / 2

                Label {
                    width: parent.width
                    text: name
                    color: deviceItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeMedium
                    truncationMode: TruncationMode.Fade
                }

                Label {
                    width: parent.width
                    text: modelName.length > 0
                          ? qsTr("%1 · %2:%3").arg(modelName).arg(host).arg(port)
                          : qsTr("%1:%2").arg(host).arg(port)
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    truncationMode: TruncationMode.Fade
                }
            }
        }

        VerticalScrollDecorator {}
    }

    Component.onCompleted: {
        if (!appWindow.castMode
                && castDeviceModel.count === 0
                && !castDeviceModel.discovering) {
            castDeviceModel.startDiscovery()
        }
    }

    Component.onDestruction: castDeviceModel.stopDiscovery()
}
