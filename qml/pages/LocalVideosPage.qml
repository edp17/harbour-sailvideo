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
import Sailfish.Pickers 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    SilicaGridView {
        id: categoryGrid

        anchors.fill: parent
        model: localVideoCategoryModel
        cellWidth: width / (width > height ? 3 : 2)
        cellHeight: Theme.itemSizeLarge * 2.0
        clip: true

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: localVideoModel.refresh()
            }

            MenuItem {
                text: qsTr("System video picker")
                onClicked: pageStack.push(videoPickerComponent)
            }
        }

        header: Column {
            width: categoryGrid.width
            spacing: Theme.paddingSmall

            PageHeader {
                title: qsTr("Local videos")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: localVideoModel.scanning
                      ? qsTr("Loading videos from Sailfish media index…")
                      : qsTr("%1 videos in %2 folders")
                            .arg(localVideoModel.count)
                            .arg(localVideoCategoryModel.count)
                color: localVideoModel.scanning
                       ? Theme.highlightColor
                       : Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Label {
                visible: !localVideoModel.scanning
                         && localVideoModel.count === 0
                         && localVideoModel.lastError.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: localVideoModel.lastError
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }

        delegate: BackgroundItem {
            id: categoryItem

            width: categoryGrid.cellWidth
            height: categoryGrid.cellHeight

            onClicked: pageStack.push(Qt.resolvedUrl("LocalVideoCategoryPage.qml"),
                                      { categoryName: name,
                                        categoryPath: path })

            Rectangle {
                anchors {
                    fill: parent
                    margins: Theme.paddingSmall
                }
                radius: Theme.paddingMedium
                color: categoryItem.highlighted
                       ? Theme.rgba(Theme.highlightColor, 0.22)
                       : Theme.rgba(Theme.primaryColor, 0.07)
                border.color: Theme.rgba(Theme.highlightColor, 0.18)
                border.width: 1
            }

            Column {
                anchors {
                    fill: parent
                    margins: Theme.paddingMedium
                }
                spacing: Theme.paddingSmall

                Item {
                    width: parent.width
                    height: Theme.iconSizeLarge + Theme.paddingMedium

                    Image {
                        anchors.centerIn: parent
                        width: Theme.iconSizeLarge
                        height: Theme.iconSizeLarge
                        source: "image://theme/icon-m-folder"
                        fillMode: Image.PreserveAspectFit
                        opacity: categoryItem.highlighted ? 1.0 : 0.85
                    }
                }

                Label {
                    width: parent.width
                    text: name
                    color: categoryItem.highlighted
                           ? Theme.highlightColor
                           : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeMedium
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    truncationMode: TruncationMode.Fade
                }

                Label {
                    width: parent.width
                    text: videoCount === 1
                          ? qsTr("1 video")
                          : qsTr("%1 videos").arg(videoCount)
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: parent.width
                    text: subtitle
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeTiny
                    horizontalAlignment: Text.AlignHCenter
                    truncationMode: TruncationMode.Fade
                }
            }
        }

        VerticalScrollDecorator {}
    }

    Component.onCompleted: {
        if (localVideoModel.count === 0 && !localVideoModel.scanning) {
            localVideoModel.refresh()
        }
    }

    Component {
        id: videoPickerComponent

        VideoPickerPage {
            title: qsTr("System video picker")

            onSelectedContentPropertiesChanged: {
                var mediaUrl = appWindow.mediaUrlFromPickerProperties(selectedContentProperties)
                if (mediaUrl.length === 0) {
                    return
                }

                appWindow.openMedia(mediaUrl,
                                    appWindow.titleFromPickerProperties(selectedContentProperties))
            }
        }
    }
}
