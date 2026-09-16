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
import Nemo.Thumbnailer 1.0
import SailVideo 1.0

Page {
    id: page

    property string categoryName: ""
    property string categoryPath: ""

    allowedOrientations: Orientation.All

    LocalVideoFolderModel {
        id: folderModel
        sourceModel: localVideoModel
        folderPath: page.categoryPath
    }

    SilicaGridView {
        id: videoGrid

        anchors.fill: parent
        model: folderModel
        cellWidth: width / (width > height ? 3 : 2)
        cellHeight: Theme.itemSizeLarge * 2.35
        clip: true

        header: Column {
            width: videoGrid.width
            spacing: Theme.paddingSmall

            PageHeader {
                title: page.categoryName
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: folderModel.count === 1
                      ? qsTr("1 video")
                      : qsTr("%1 videos").arg(folderModel.count)
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
            }
        }

        delegate: BackgroundItem {
            id: videoItem

            width: videoGrid.cellWidth
            height: videoGrid.cellHeight

            onClicked: appWindow.openMedia(url, title)

            Column {
                anchors {
                    fill: parent
                    margins: Theme.paddingSmall
                }
                spacing: Theme.paddingSmall / 2

                Rectangle {
                    width: parent.width
                    height: Theme.itemSizeLarge
                    radius: Theme.paddingMedium
                    color: videoItem.highlighted
                           ? Theme.rgba(Theme.highlightColor, 0.24)
                           : Theme.rgba(Theme.primaryColor, 0.08)
                    border.color: Theme.rgba(Theme.highlightColor, 0.15)
                    border.width: 1

                    Image {
                        id: videoThumbnail

                        anchors.centerIn: parent
                        width: Theme.iconSizeExtraLarge
                        height: Theme.iconSizeExtraLarge
                        source: "image://nemoThumbnail/" + url
                        sourceSize.width: Theme.iconSizeExtraLarge
                        sourceSize.height: Theme.iconSizeExtraLarge
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                    }

                    Label {
                        visible: videoThumbnail.status === Image.Error
                                 || videoThumbnail.status === Image.Null
                        anchors.centerIn: parent
                        text: "▶"
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeExtraLarge
                    }
                }

                Item {
                    width: parent.width
                    height: Theme.fontSizeSmall * 2.8

                    Label {
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.paddingSmall
                            rightMargin: Theme.paddingSmall
                            verticalCenter: parent.verticalCenter
                        }
                        text: title && title.length > 0
                              ? title
                              : qsTr("Untitled video")
                        color: videoItem.highlighted
                               ? Theme.highlightColor
                               : Theme.primaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        clip: true
                    }
                }

                Label {
                    width: parent.width
                    text: sizeText
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeTiny
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: parent.width
                    text: modified
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeTiny
                    horizontalAlignment: Text.AlignHCenter
                    truncationMode: TruncationMode.Fade
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
