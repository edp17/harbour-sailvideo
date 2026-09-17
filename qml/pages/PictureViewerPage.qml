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
import Sailfish.Silica 1.0

Page {
    id: page

    objectName: "pictureViewerPage"
    allowedOrientations: Orientation.All

    property string pictureTitle: appWindow.currentPictureTitle
    property string pictureSource: appWindow.currentPictureSource
    property real zoom: 1.0
    property bool controlsVisible: true
    property bool slideshowRunning: false
    property bool activePictureCast: appWindow.castActivePicture

    function slideshowIsRunning() {
        return activePictureCast
                ? appWindow.castPictureSlideshowRunning
                : slideshowRunning
    }

    function toggleSlideshow() {
        if (activePictureCast) {
            appWindow.castPictureSlideshowRunning = !appWindow.castPictureSlideshowRunning
        } else {
            slideshowRunning = !slideshowRunning
        }
    }

    function zoomIn() {
        zoom = Math.min(4.0, zoom * 1.25)
    }

    function zoomOut() {
        zoom = Math.max(0.25, zoom / 1.25)
    }

    function fitToScreen() {
        zoom = 1.0
        flick.contentX = 0
        flick.contentY = 0
    }

    function goPrevious() {
        if (activePictureCast) {
            appWindow.castPictureSlideshowRunning = false
            if (appWindow.showPreviousPictureControl()) {
                fitToScreen()
            }
        } else {
            slideshowRunning = false
            if (appWindow.showPreviousPicture()) {
                fitToScreen()
            }
        }
    }

    function goNext() {
        var advanced = activePictureCast
                ? appWindow.showNextPictureControl()
                : appWindow.showNextPicture()
        if (advanced) {
            fitToScreen()
        } else if (activePictureCast) {
            appWindow.castPictureSlideshowRunning = false
        } else {
            slideshowRunning = false
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    SilicaFlickable {
        id: flick

        anchors.fill: parent
        clip: true
        contentWidth: Math.max(width, imageItem.width)
        contentHeight: Math.max(height, imageItem.height)

        PullDownMenu {
            MenuItem {
                visible: page.activePictureCast
                         ? appWindow.pictureQueue.length > 1
                         : (appWindow.hasNextPicture || appWindow.hasPreviousPicture)
                text: page.slideshowIsRunning() ? qsTr("Stop slideshow") : qsTr("Start slideshow")
                onClicked: page.toggleSlideshow()
            }
            MenuItem {
                visible: page.activePictureCast
                         ? appWindow.hasNextPictureControl
                         : appWindow.hasNextPicture
                text: page.activePictureCast ? qsTr("Next media") : qsTr("Next picture")
                onClicked: page.goNext()
            }
            MenuItem {
                visible: page.activePictureCast
                         ? appWindow.hasPreviousPictureControl
                         : appWindow.hasPreviousPicture
                text: page.activePictureCast ? qsTr("Previous media") : qsTr("Previous picture")
                onClicked: page.goPrevious()
            }
            MenuItem {
                text: qsTr("Fit to screen")
                onClicked: page.fitToScreen()
            }
            MenuItem {
                text: qsTr("Zoom out")
                onClicked: page.zoomOut()
            }
            MenuItem {
                text: qsTr("Zoom in")
                onClicked: page.zoomIn()
            }
        }

        Image {
            id: imageItem

            width: Math.max(flick.width, flick.width * page.zoom)
            height: Math.max(flick.height, flick.height * page.zoom)
            fillMode: Image.PreserveAspectFit
            source: page.pictureSource
            asynchronous: true
            cache: false
            smooth: true

            x: Math.max(0, (flick.width - width) / 2)
            y: Math.max(0, (flick.height - height) / 2)

            onSourceChanged: page.fitToScreen()
        }

        MouseArea {
            anchors.fill: parent
            onClicked: page.controlsVisible = !page.controlsVisible
        }
    }

    Rectangle {
        visible: page.controlsVisible
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: Theme.itemSizeLarge
        color: "#99000000"

        IconButton {
            id: backButton
            anchors {
                left: parent.left
                leftMargin: Theme.paddingSmall
                verticalCenter: parent.verticalCenter
            }
            icon.source: "image://theme/icon-m-back"
            onClicked: pageStack.pop()
        }

        BackgroundItem {
            id: castButton

            anchors {
                right: parent.right
                rightMargin: Theme.paddingSmall
                verticalCenter: parent.verticalCenter
            }
            width: Theme.itemSizeMedium
            height: Theme.itemSizeMedium

            onClicked: appWindow.openCastDevicesForPicture()

            Column {
                anchors.centerIn: parent
                width: parent.width
                spacing: 0

                Label {
                    width: parent.width
                    text: appWindow.castActivePicture ? "◉" : "◌"
                    color: appWindow.castActivePicture
                           ? Theme.highlightColor
                           : (castButton.highlighted ? Theme.highlightColor : "white")
                    font.pixelSize: Theme.fontSizeMedium
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: parent.width
                    text: qsTr("Cast")
                    color: "white"
                    font.pixelSize: Theme.fontSizeTiny
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        Label {
            anchors {
                left: backButton.right
                right: castButton.left
                leftMargin: Theme.paddingSmall
                rightMargin: Theme.paddingSmall
                verticalCenter: parent.verticalCenter
            }
            text: page.pictureTitle.length > 0 ? page.pictureTitle : qsTr("Picture")
            color: "white"
            truncationMode: TruncationMode.Fade
        }
    }

    Rectangle {
        visible: page.controlsVisible
                 && (page.activePictureCast
                     ? (appWindow.hasPreviousPictureControl || appWindow.hasNextPictureControl)
                     : (appWindow.hasPreviousPicture || appWindow.hasNextPicture))
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: Theme.itemSizeLarge
        color: "#99000000"

        Row {
            anchors.centerIn: parent
            spacing: Theme.paddingLarge

            IconButton {
                enabled: page.activePictureCast
                         ? appWindow.hasPreviousPictureControl
                         : appWindow.hasPreviousPicture
                opacity: enabled ? 1.0 : 0.35
                icon.source: "image://theme/icon-m-previous"
                onClicked: page.goPrevious()
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                font.pixelSize: Theme.fontSizeExtraSmall
                text: page.activePictureCast && appWindow.castFolderNavigationActive
                      ? qsTr("%1 / %2")
                            .arg(appWindow.folderMediaQueueIndex + 1)
                            .arg(appWindow.folderMediaQueue.length)
                      : (appWindow.pictureQueueIndex >= 0
                         ? qsTr("%1 / %2")
                               .arg(appWindow.pictureQueueIndex + 1)
                               .arg(appWindow.pictureQueue.length)
                         : qsTr("Picture"))
            }

            IconButton {
                enabled: page.activePictureCast
                         ? appWindow.hasNextPictureControl
                         : appWindow.hasNextPicture
                opacity: enabled ? 1.0 : 0.35
                icon.source: "image://theme/icon-m-next"
                onClicked: page.goNext()
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: imageItem.status === Image.Loading
        visible: running
        size: BusyIndicatorSize.Medium
    }

    Label {
        visible: imageItem.status === Image.Error
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        text: qsTr("The picture could not be loaded.")
        color: Theme.errorColor
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }

    Timer {
        interval: Math.max(1, appSettings.pictureSlideshowSeconds) * 1000
        repeat: true
        running: page.slideshowRunning && !page.activePictureCast
        onTriggered: page.goNext()
    }
}
