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

    RemorsePopup {
        id: remorsePopup
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                visible: networkSources.count > 0
                text: qsTr("Clear network sources")
                onClicked: remorsePopup.execute(qsTr("Clearing network sources"),
                                                function() { networkSources.clear() })
            }

            MenuItem {
                text: qsTr("Open URL")
                onClicked: pageStack.push(Qt.resolvedUrl("EditNetworkSourcePage.qml"),
                                          { playOnly: true })
            }

            MenuItem {
                text: qsTr("Add source")
                onClicked: pageStack.push(Qt.resolvedUrl("EditNetworkSourcePage.qml"))
            }
        }

        Column {
            id: contentColumn

            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("↗ URL sources")
            }

            Label {
                visible: networkSources.count === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("No HTTP/HTTPS sources saved yet. Pull down to add one or open a URL without saving.")
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Repeater {
                model: networkSources

                delegate: ListItem {
                    id: sourceItem

                    width: contentColumn.width
                    contentHeight: Theme.itemSizeLarge

                    menu: ContextMenu {
                        MenuItem {
                            text: qsTr("Edit")
                            onClicked: pageStack.push(Qt.resolvedUrl("EditNetworkSourcePage.qml"),
                                                      { sourceIndex: index,
                                                        initialName: name,
                                                        initialUrl: url })
                        }

                        MenuItem {
                            text: qsTr("Remove")
                            onClicked: remorsePopup.execute(qsTr("Removing source"),
                                                            function() { networkSources.removeAt(index) })
                        }
                    }

                    onClicked: appWindow.openNetworkUrl(url, name)

                    Row {
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: Theme.paddingMedium

                        Label {
                            width: Theme.iconSizeMedium
                            text: "↗"
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeLarge
                            horizontalAlignment: Text.AlignHCenter
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            width: parent.width - Theme.iconSizeMedium - Theme.paddingMedium
                            spacing: Theme.paddingSmall / 2
                            anchors.verticalCenter: parent.verticalCenter

                            Label {
                                width: parent.width
                                text: name && name.length > 0 ? name : qsTr("Network video")
                                color: sourceItem.highlighted
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
            }
        }

        VerticalScrollDecorator {}
    }
}
