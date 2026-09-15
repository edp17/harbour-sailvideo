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

    property int sourceIndex: -1
    property string initialName: ""
    property string initialUrl: ""
    property bool playOnly: false

    allowedOrientations: Orientation.All

    function showError(message) {
        errorLabel.text = message && message.length > 0
                ? message
                : qsTr("The URL could not be opened.")
    }

    function playUrl() {
        if (appWindow.openNetworkUrl(urlField.text, nameField.text)) {
            return
        }

        showError(networkSources.lastError.length > 0
                  ? networkSources.lastError
                  : appWindow.playbackStatus)
    }

    function saveSource(playAfterSaving) {
        var ok = false
        if (sourceIndex >= 0) {
            ok = networkSources.setAt(sourceIndex, nameField.text, urlField.text)
        } else {
            ok = networkSources.addOrUpdate(nameField.text, urlField.text)
        }

        if (!ok) {
            showError(networkSources.lastError)
            return
        }

        if (playAfterSaving) {
            appWindow.openNetworkUrl(urlField.text, nameField.text)
        } else {
            pageStack.pop()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                visible: !playOnly
                text: sourceIndex >= 0 ? qsTr("Save") : qsTr("Add source")
                onClicked: saveSource(false)
            }

            MenuItem {
                text: playOnly ? qsTr("Play") : qsTr("Save and play")
                onClicked: playOnly ? playUrl() : saveSource(true)
            }
        }

        Column {
            id: contentColumn

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: playOnly
                       ? qsTr("Open network URL")
                       : (sourceIndex >= 0 ? qsTr("Edit source") : qsTr("Add source"))
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Enter a direct HTTP or HTTPS media URL, for example a video file served by a NAS web server. SMB share browsing will be implemented separately.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            TextField {
                id: nameField

                width: parent.width
                label: qsTr("Name")
                placeholderText: qsTr("Optional display name")
                text: initialName
                inputMethodHints: Qt.ImhNoPredictiveText
            }

            TextField {
                id: urlField

                width: parent.width
                label: qsTr("URL")
                placeholderText: qsTr("https://server/path/video.mp4")
                text: initialUrl
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText

                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: playOnly ? playUrl() : saveSource(false)

                onTextChanged: {
                    if (nameField.text.length === 0) {
                        var suggestion = networkSources.suggestedNameForUrl(text)
                        if (suggestion.length > 0) {
                            nameField.placeholderText = suggestion
                        }
                    }
                }
            }

            Label {
                id: errorLabel

                visible: text.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.errorColor
                wrapMode: Text.Wrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: playOnly ? qsTr("Play") : qsTr("Save")
                onClicked: playOnly ? playUrl() : saveSource(false)
            }

            Button {
                visible: !playOnly
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Save and play")
                onClicked: saveSource(true)
            }
        }

        VerticalScrollDecorator {}
    }
}
