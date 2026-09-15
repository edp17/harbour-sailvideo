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
        slideshowRunning = false
        if (appWindow.showPreviousPicture()) {
            fitToScreen()
        }
    }

    function goNext() {
        if (appWindow.showNextPicture()) {
            fitToScreen()
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
                visible: appWindow.hasNextPicture || appWindow.hasPreviousPicture
                text: slideshowRunning ? qsTr("Stop slideshow") : qsTr("Start slideshow")
                onClicked: slideshowRunning = !slideshowRunning
            }
            MenuItem {
                visible: appWindow.hasNextPicture
                text: qsTr("Next picture")
                onClicked: page.goNext()
            }
            MenuItem {
                visible: appWindow.hasPreviousPicture
                text: qsTr("Previous picture")
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

        Label {
            anchors {
                left: backButton.right
                right: parent.right
                leftMargin: Theme.paddingSmall
                rightMargin: Theme.horizontalPageMargin
                verticalCenter: parent.verticalCenter
            }
            text: page.pictureTitle.length > 0 ? page.pictureTitle : qsTr("Picture")
            color: "white"
            truncationMode: TruncationMode.Fade
        }
    }

    Rectangle {
        visible: page.controlsVisible && (appWindow.hasPreviousPicture || appWindow.hasNextPicture)
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
                enabled: appWindow.hasPreviousPicture
                opacity: enabled ? 1.0 : 0.35
                icon.source: "image://theme/icon-m-previous"
                onClicked: page.goPrevious()
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                font.pixelSize: Theme.fontSizeExtraSmall
                text: appWindow.pictureQueueIndex >= 0
                      ? qsTr("%1 / %2").arg(appWindow.pictureQueueIndex + 1).arg(appWindow.pictureQueue.length)
                      : qsTr("Picture")
            }

            IconButton {
                enabled: appWindow.hasNextPicture
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
        interval: 5000
        repeat: true
        running: page.slideshowRunning
        onTriggered: page.goNext()
    }
}
