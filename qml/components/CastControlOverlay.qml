/*
 * Copyright (C) 2026 edp17
 * GPL-3.0-or-later
 */
import QtQuick 2.0
import Sailfish.Silica 1.0

Rectangle {
    id: root
    property string sourceKind: "video"
    property bool controlsVisible: true
    property bool menuActive: false
    property bool pageIsPicture: sourceKind === "picture"
    property bool activeMatchesPage: appWindow.castMode
                                     && (pageIsPicture
                                         ? appWindow.castActivePicture
                                         : appWindow.videoCastActive)

    visible: appWindow.castMode && controlsVisible
    opacity: menuActive ? 0.15 : 1.0
    enabled: !menuActive

    Behavior on opacity {
        NumberAnimation { duration: 120 }
    }

    width: Math.min(parent ? parent.width - 2 * Theme.horizontalPageMargin : 0,
                    Math.max(Theme.itemSizeLarge * 2,
                             controlColumn.implicitWidth + 2 * Theme.paddingMedium))
    height: controlColumn.height + 2 * Theme.paddingSmall
    radius: Theme.paddingSmall
    color: "#cc000000"

    function receiverName() {
        return castManager.deviceName.length > 0
                ? castManager.deviceName
                : (appWindow.castLastDeviceName.length > 0
                   ? appWindow.castLastDeviceName
                   : qsTr("Chromecast"))
    }

    Column {
        id: controlColumn
        anchors.centerIn: parent
        spacing: Theme.paddingSmall

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: appWindow.castActivePicture
                  ? qsTr("Displaying on %1").arg(root.receiverName())
                  : qsTr("Casting to %1").arg(root.receiverName())
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.paddingSmall

            Button {
                visible: !root.activeMatchesPage
                         && castManager.connected
                         && !castManager.disconnecting
                text: root.pageIsPicture ? qsTr("Cast picture") : qsTr("Cast video")
                onClicked: {
                    appWindow.castTargetKind = root.pageIsPicture ? "picture" : "video"
                    appWindow.castRequestedMediaToConnectedDevice(false)
                }
            }

            Button {
                visible: root.activeMatchesPage && !root.pageIsPicture
                text: castManager.muted ? qsTr("Unmute") : qsTr("Mute")
                enabled: castManager.connected
                onClicked: castManager.setMuted(!castManager.muted)
            }

            Button {
                visible: root.activeMatchesPage && !root.pageIsPicture
                text: castManager.mediaStopped ? qsTr("Continue") : qsTr("Stop")
                enabled: castManager.connected
                         && !appWindow.aviCastProgressiveSession
                         && (castManager.mediaStopped || castManager.casting)
                onClicked: {
                    if (castManager.mediaStopped) castManager.continueMedia()
                    else castManager.stopMedia()
                }
            }

            Button {
                visible: root.activeMatchesPage
                         && root.pageIsPicture
                         && appWindow.pictureQueue.length > 1
                text: appWindow.castPictureSlideshowRunning
                      ? qsTr("Stop slideshow")
                      : qsTr("Slideshow")
                enabled: castManager.connected
                onClicked: {
                    appWindow.castTargetKind = "picture"
                    appWindow.toggleCastPictureSlideshowFromControlPage()
                }
            }
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Disconnect")
            enabled: !castManager.disconnecting
            onClicked: appWindow.disconnectCastAndResume()
        }

        Label {
            visible: appWindow.playbackError.length > 0
            width: parent.parent.width - 2 * Theme.paddingMedium
            text: appWindow.playbackError
            color: Theme.errorColor
            font.pixelSize: Theme.fontSizeTiny
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
}
