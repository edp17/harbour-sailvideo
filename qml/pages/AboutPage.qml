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

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: aboutColumn.height + Theme.paddingLarge

        Column {
            id: aboutColumn

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("About SailVideo")
            }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "/usr/share/icons/hicolor/172x172/apps/harbour-sailvideo.png"
                width: Theme.itemSizeLarge * 2
                height: Theme.itemSizeLarge * 2
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("SailVideo")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeExtraLarge
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Native local and network video player for Sailfish OS")
                color: Theme.primaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Version 1.1.0.14")
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
            }

            SectionHeader {
                text: qsTr("About")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("SailVideo is a native Sailfish OS media player focused on local videos, home-network media and Chromecast. It uses the Sailfish QtMultimedia/GStreamer playback stack for playback on the phone and a native Cast V2 sender for Google Chromecast devices.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            SectionHeader {
                text: qsTr("Features")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("• Local video library grouped by folder, with Sailfish video thumbnails\n"
                           + "• Local video playback with resume positions and recent history\n"
                           + "• SMB2/SMB3 NAS browsing and seekable playback\n"
                           + "• Saved NAS sources with passwords protected by Sailfish Secrets\n"
                           + "• HTTP/HTTPS video sources with persistent source list\n"
                           + "• Native Google Chromecast discovery with remembered devices and in-player controls\n"
                           + "• Video and NAS picture display on Chromecast\n"
                           + "• Direct URL casting and LAN Range bridge for local/SMB media\n"
                           + "• AVI casting with lossless remuxing and VP8/Vorbis transcode fallback\n"
                           + "• Remote play/pause, seek, skip, stop/continue, volume and disconnect controls\n"
                           + "• Rejoin an ongoing Cast session after restarting SailVideo where the media source remains reachable\n"
                           + "• Fit, Crop and Stretch video scaling\n"
                           + "• Swipe controls for playback volume and video brightness\n"
                           + "• Configurable new-video volume, brightness and skip interval\n"
                           + "• Keep-display-on support during playback\n"
                           + "• NAS picture browsing with previous/next navigation and configurable slideshow\n"
                           + "• Cover play/pause controls")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            SectionHeader {
                text: qsTr("Developer")
            }

            DetailItem {
                label: qsTr("Developer")
                value: "edp17"
            }

            DetailItem {
                label: qsTr("Package")
                value: "harbour-sailvideo"
            }

            DetailItem {
                label: qsTr("Source")
                value: "github.com/edp17/harbour-sailvideo"
            }

            SectionHeader {
                text: qsTr("Licensing")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Original SailVideo code is licensed under GPL-3.0-or-later. The bundled libsmb2 library is licensed under LGPL-2.1-or-later. See Third-party licences for details.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Third-party licences")
                onClicked: pageStack.push(Qt.resolvedUrl("LegalPage.qml"))
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: "Copyright © 2026 edp17"
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                horizontalAlignment: Text.AlignHCenter
            }
        }

        VerticalScrollDecorator {}
    }
}
