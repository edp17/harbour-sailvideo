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
import SailVideo 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    // Prefer the application-wide C++ discovery model. A local model keeps
    // discovery available if the root-context object is unavailable.
    property var discoveryModel: fallbackNetworkDiscovery
    property bool usingFallbackModel: true

    NetworkDiscoveryModel {
        id: fallbackNetworkDiscovery
    }

    function bindDiscoveryModel() {
        if (typeof networkDiscovery !== "undefined" && networkDiscovery !== null) {
            discoveryModel = networkDiscovery
            usingFallbackModel = false
        } else {
            discoveryModel = fallbackNetworkDiscovery
            usingFallbackModel = true
        }
    }

    function addDiscoveredHost(host, port) {
        pageStack.push(Qt.resolvedUrl("EditNasSourcePage.qml"),
                       { initialName: host,
                         initialHost: host,
                         initialPort: port > 0 ? port : 445,
                         initialGuest: false })
    }

    function scanNow() {
        bindDiscoveryModel()
        if (!discoveryModel.scanning) {
            discoveryModel.scan()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Add manually")
                onClicked: pageStack.push(Qt.resolvedUrl("EditNasSourcePage.qml"))
            }

            MenuItem {
                text: qsTr("Search again")
                onClicked: page.scanNow()
            }
        }

        Column {
            id: contentColumn

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("⌕ Find NAS")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Searches the local IPv4 /24 network for devices with SMB port 445 open. This detects possible NAS servers, but it cannot discover share names yet.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: discoveryModel.scanning ? qsTr("Searching...") : qsTr("Search local network")
                enabled: !discoveryModel.scanning
                onClicked: page.scanNow()
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: discoveryModel.scanning
                visible: running
                size: BusyIndicatorSize.Medium
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: discoveryModel.status
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            SectionHeader {
                visible: discoveryModel.count > 0
                text: qsTr("Possible SMB servers")
            }

            Repeater {
                model: discoveryModel

                delegate: BackgroundItem {
                    width: contentColumn.width
                    height: Theme.itemSizeLarge

                    onClicked: addDiscoveredHost(host, port)

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
                            color: parent.parent.highlighted
                                   ? Theme.highlightColor
                                   : Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                        }

                        Label {
                            width: parent.width
                            text: subtitle
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            truncationMode: TruncationMode.Fade
                        }
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Add NAS manually")
                onClicked: pageStack.push(Qt.resolvedUrl("EditNasSourcePage.qml"))
            }
        }

        VerticalScrollDecorator {}
    }

    Component.onCompleted: {
        bindDiscoveryModel()
        if (discoveryModel.count === 0 && !discoveryModel.scanning) {
            discoveryModel.scan()
        }
    }
}
