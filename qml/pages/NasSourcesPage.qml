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

    function sameCredentials(row, host, port, share, domain, username, guest) {
        return appWindow.cleanedValue(nasSources.hostAt(row)).toLowerCase() === appWindow.cleanedValue(host).toLowerCase()
                && nasSources.portAt(row) === port
                && appWindow.cleanedValue(nasSources.shareAt(row)).toLowerCase() === appWindow.cleanedValue(share).toLowerCase()
                && appWindow.cleanedValue(nasSources.domainAt(row)).toLowerCase() === appWindow.cleanedValue(domain).toLowerCase()
                && appWindow.cleanedValue(nasSources.usernameAt(row)).toLowerCase() === appWindow.cleanedValue(username).toLowerCase()
                && nasSources.guestAt(row) === guest
    }

    function otherSourceUsesCredentials(skipRow, host, port, share, domain, username, guest) {
        for (var i = 0; i < nasSources.count; ++i) {
            if (i !== skipRow && sameCredentials(i, host, port, share, domain, username, guest)) {
                return true
            }
        }
        return false
    }

    function removeSourceAndMaybePassword(row, host, port, share, domain, username, guest) {
        if (!otherSourceUsesCredentials(row, host, port, share, domain, username, guest)) {
            smbCredentialStore.deletePasswordFor(host, port, share, domain, username, guest)
        }
        nasSources.removeAt(row)
    }

    function forgetSavedPasswords() {
        for (var i = nasSources.count - 1; i >= 0; --i) {
            smbCredentialStore.deletePasswordFor(nasSources.hostAt(i),
                                                 nasSources.portAt(i),
                                                 nasSources.shareAt(i),
                                                 nasSources.domainAt(i),
                                                 nasSources.usernameAt(i),
                                                 nasSources.guestAt(i))
        }
    }

    function clearSourcesAndPasswords() {
        forgetSavedPasswords()
        nasSources.clear()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                visible: nasSources.count > 0
                text: qsTr("Clear NAS sources")
                onClicked: remorsePopup.execute(qsTr("Clearing NAS sources"),
                                                function() { clearSourcesAndPasswords() })
            }

            MenuItem {
                visible: nasSources.count > 0
                text: qsTr("Forget saved NAS passwords")
                onClicked: remorsePopup.execute(qsTr("Forgetting NAS passwords"),
                                                function() { forgetSavedPasswords() })
            }

            MenuItem {
                text: qsTr("Search local network")
                onClicked: pageStack.push(Qt.resolvedUrl("NetworkDiscoveryPage.qml"))
            }

            MenuItem {
                text: qsTr("Add SMB source")
                onClicked: pageStack.push(Qt.resolvedUrl("EditNasSourcePage.qml"))
            }
        }

        Column {
            id: contentColumn

            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("▣ NAS sources")
            }

            Label {
                visible: !smbBackend.backendAvailable()
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: smbBackend.backendStatus
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            Label {
                visible: nasSources.count === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("No NAS sources saved yet. Pull down to add one or search the local network.")
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Repeater {
                model: nasSources

                delegate: ListItem {
                    id: sourceItem

                    width: contentColumn.width
                    contentHeight: Theme.itemSizeLarge + Theme.paddingMedium

                    menu: ContextMenu {
                        MenuItem {
                            text: qsTr("Edit")
                            onClicked: pageStack.push(Qt.resolvedUrl("EditNasSourcePage.qml"),
                                                      { sourceIndex: index,
                                                        initialName: name,
                                                        initialHost: host,
                                                        initialPort: port,
                                                        initialShare: share,
                                                        initialPath: path,
                                                        initialDomain: domain,
                                                        initialUsername: username,
                                                        initialGuest: guest })
                        }

                        MenuItem {
                            visible: !guest
                            text: qsTr("Forget password")
                            onClicked: remorsePopup.execute(qsTr("Forgetting password"),
                                                            function() { smbCredentialStore.deletePasswordFor(host, port, share, domain, username, guest) })
                        }

                        MenuItem {
                            text: qsTr("Remove")
                            onClicked: remorsePopup.execute(qsTr("Removing NAS source"),
                                                            function() { removeSourceAndMaybePassword(index, host, port, share, domain, username, guest) })
                        }
                    }

                    onClicked: pageStack.push(Qt.resolvedUrl("NasBrowserPage.qml"),
                                              { sourceTitle: name,
                                                host: host,
                                                port: port,
                                                share: share,
                                                domain: domain,
                                                username: username,
                                                guest: guest,
                                                currentPath: path,
                                                sourceRootPath: path,
                                                sourceRootLocked: true })

                    Rectangle {
                        anchors {
                            fill: parent
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            topMargin: Theme.paddingSmall / 2
                            bottomMargin: Theme.paddingSmall / 2
                        }
                        radius: Theme.paddingMedium
                        color: sourceItem.highlighted ? "#33209fd6" : "#11000000"
                    }

                    Row {
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.horizontalPageMargin + Theme.paddingMedium
                            rightMargin: Theme.horizontalPageMargin + Theme.paddingMedium
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: Theme.paddingMedium

                        Label {
                            width: Theme.iconSizeLarge
                            text: "▣"
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeExtraLarge
                            horizontalAlignment: Text.AlignHCenter
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            width: parent.width - Theme.iconSizeLarge - Theme.paddingMedium
                            spacing: Theme.paddingSmall / 2
                            anchors.verticalCenter: parent.verticalCenter

                            Label {
                                width: parent.width
                                text: name && name.length > 0 ? name : qsTr("SMB location")
                                color: sourceItem.highlighted
                                       ? Theme.highlightColor
                                       : Theme.primaryColor
                                font.pixelSize: Theme.fontSizeMedium
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
