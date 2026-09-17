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

CoverBackground {
    Label {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Theme.paddingLarge
        }
        text: qsTr("SailVideo")
        horizontalAlignment: Text.AlignHCenter
        color: Theme.highlightColor
        font.pixelSize: Theme.fontSizeLarge
    }

    Label {
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            margins: Theme.paddingLarge
        }
        text: appWindow.castActivePicture
              ? (appWindow.currentPictureTitle.length > 0
                 ? appWindow.currentPictureTitle
                 : qsTr("Picture"))
              : (appWindow.hasMedia
                 ? appWindow.currentMediaTitle
                 : qsTr("No video selected"))
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        maximumLineCount: 4
        truncationMode: TruncationMode.Fade
    }

    Label {
        visible: appWindow.castMode
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            bottomMargin: Theme.paddingLarge * 3
            margins: Theme.paddingLarge
        }
        text: appWindow.castActivePicture
              ? qsTr("Displaying picture on %1").arg(
                    castManager.deviceName.length > 0
                    ? castManager.deviceName
                    : appWindow.castLastDeviceName)
              : qsTr("Casting to %1").arg(
                    castManager.deviceName.length > 0
                    ? castManager.deviceName
                    : appWindow.castLastDeviceName)
        horizontalAlignment: Text.AlignHCenter
        color: Theme.highlightColor
        font.pixelSize: Theme.fontSizeExtraSmall
        truncationMode: TruncationMode.Fade
    }

    CoverActionList {
        enabled: appWindow.hasMedia && !appWindow.castActivePicture

        CoverAction {
            iconSource: appWindow.playbackIsPlaying()
                        ? "image://theme/icon-cover-pause"
                        : "image://theme/icon-cover-play"

            onTriggered: appWindow.togglePlayback()
        }
    }
}
