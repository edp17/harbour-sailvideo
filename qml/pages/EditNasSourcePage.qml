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
    property string initialHost: ""
    property int initialPort: 445
    property string initialShare: ""
    property string initialPath: ""
    property string initialDomain: ""
    property string initialUsername: ""
    property bool initialGuest: true
    property string loadedPassword: ""

    allowedOrientations: Orientation.All

    function clean(value) {
        return appWindow.cleanedValue(value).replace(/\\/g, "/")
    }

    function portValue() {
        var parsed = parseInt(portField.text, 10)
        return isNaN(parsed) || parsed <= 0 ? 445 : parsed
    }

    function initialSmbPathText() {
        var share = clean(initialShare)
        var path = normalizedFolder(initialPath)
        if (share.length === 0 && path.length === 0) {
            return ""
        }
        return "/" + share + (path.length > 0 ? "/" + path : "")
    }

    function locationWithoutScheme() {
        var text = clean(locationField.text)
        if (text.toLowerCase().indexOf("smb://") === 0) {
            text = text.substring(6)
        } else if (text.toLowerCase().indexOf("smb2://") === 0) {
            text = text.substring(7)
        }

        while (text.indexOf("//") === 0) {
            text = text.substring(2)
        }

        var host = clean(hostField.text)
        if (host.length > 0
                && text.toLowerCase().indexOf(host.toLowerCase() + "/") === 0) {
            text = text.substring(host.length + 1)
        }

        while (text.charAt(0) === "/") {
            text = text.substring(1)
        }

        return text
    }

    function normalizedFolder(value) {
        var text = clean(value)
        while (text.charAt(0) === "/") text = text.substring(1)
        while (text.length > 0 && text.charAt(text.length - 1) === "/") {
            text = text.substring(0, text.length - 1)
        }

        var result = []
        var parts = text.split("/")
        for (var i = 0; i < parts.length; ++i) {
            var part = appWindow.cleanedValue(parts[i])
            if (part.length === 0 || part === ".") {
                continue
            }
            if (part === "..") {
                if (result.length > 0) {
                    result.pop()
                }
                continue
            }
            result.push(part)
        }
        return result.join("/")
    }

    function shareValue() {
        var text = locationWithoutScheme()
        var slash = text.indexOf("/")
        var share = slash >= 0 ? text.substring(0, slash) : text
        return clean(share)
    }

    function pathValue() {
        var text = locationWithoutScheme()
        var slash = text.indexOf("/")
        if (slash < 0) {
            return ""
        }
        return normalizedFolder(text.substring(slash + 1))
    }

    function hostValue() {
        var host = clean(hostField.text)
        if (host.length > 0) {
            return host
        }

        var text = clean(locationField.text)
        if (text.toLowerCase().indexOf("smb://") === 0) {
            text = text.substring(6)
        } else if (text.toLowerCase().indexOf("smb2://") === 0) {
            text = text.substring(7)
        } else {
            while (text.indexOf("//") === 0) {
                text = text.substring(2)
            }
        }

        var slash = text.indexOf("/")
        return slash > 0 ? text.substring(0, slash) : host
    }

    function domainValue() {
        // Keep an existing hidden domain/workgroup value for backwards
        // compatibility, but do not show it in the normal UI.
        return clean(initialDomain)
    }

    function usernameValue() {
        return clean(usernameField.text)
    }

    function browserTitle() {
        var title = clean(nameField.text)
        if (title.length > 0) return title

        var share = shareValue()
        var path = pathValue()
        return hostValue() + "/" + share + (path.length > 0 ? "/" + path : "")
    }

    function showError(message) {
        errorLabel.text = message && message.length > 0
                ? message
                : qsTr("The NAS source could not be saved.")
    }

    function loadSavedPassword() {
        if (initialGuest || initialHost.length === 0 || initialShare.length === 0) {
            return
        }

        var saved = smbCredentialStore.passwordFor(initialHost,
                                                   initialPort > 0 ? initialPort : 445,
                                                   initialShare,
                                                   initialDomain,
                                                   initialUsername,
                                                   initialGuest)
        if (saved && saved.length > 0) {
            loadedPassword = saved
            passwordField.text = saved
        }
    }

    function savePasswordForCurrentSource() {
        if (guestSwitch.checked) {
            return true
        }

        if (passwordField.text.length === 0) {
            return true
        }

        if (!smbCredentialStore.storePasswordFor(hostValue(),
                                                 portValue(),
                                                 shareValue(),
                                                 domainValue(),
                                                 usernameValue(),
                                                 false,
                                                 passwordField.text)) {
            showError(smbCredentialStore.lastError)
            return false
        }

        return true
    }

    function deleteOldPasswordIfIdentityChanged() {
        if (sourceIndex < 0) {
            return
        }

        var credentialsChanged = clean(initialHost) !== hostValue()
                || initialPort !== portValue()
                || clean(initialShare) !== shareValue()
                || clean(initialDomain) !== domainValue()
                || clean(initialUsername) !== usernameValue()
                || initialGuest !== guestSwitch.checked

        if (credentialsChanged) {
            smbCredentialStore.deletePasswordFor(initialHost, initialPort, initialShare,
                                                 initialDomain, initialUsername, initialGuest)
        }
    }

    function saveSource(openAfterSaving) {
        if (hostValue().length === 0) {
            showError(qsTr("Enter a NAS server name or IP address."))
            return
        }

        if (shareValue().length === 0) {
            showError(qsTr("Enter an SMB path including the share name, for example /public or /public/Shared Videos."))
            return
        }

        if (!guestSwitch.checked && usernameValue().length === 0) {
            showError(qsTr("Enter a username or enable guest access."))
            return
        }

        var ok = false
        if (sourceIndex >= 0) {
            ok = nasSources.setAt(sourceIndex,
                                  nameField.text,
                                  hostValue(),
                                  portValue(),
                                  shareValue(),
                                  pathValue(),
                                  domainValue(),
                                  usernameValue(),
                                  guestSwitch.checked)
            if (ok) {
                deleteOldPasswordIfIdentityChanged()
            }
        } else {
            ok = nasSources.addOrUpdate(nameField.text,
                                        hostValue(),
                                        portValue(),
                                        shareValue(),
                                        pathValue(),
                                        domainValue(),
                                        usernameValue(),
                                        guestSwitch.checked)
        }

        if (!ok) {
            showError(nasSources.lastError)
            return
        }

        if (!savePasswordForCurrentSource()) {
            return
        }

        if (openAfterSaving) {
            pageStack.replace(Qt.resolvedUrl("NasBrowserPage.qml"),
                              { sourceTitle: browserTitle(),
                                host: hostValue(),
                                port: portValue(),
                                share: shareValue(),
                                domain: domainValue(),
                                username: usernameValue(),
                                guest: guestSwitch.checked,
                                currentPath: pathValue(),
                                sourceRootPath: pathValue(),
                                sourceRootLocked: true,
                                sessionPassword: guestSwitch.checked ? "" : passwordField.text })
        } else {
            pageStack.pop()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: sourceIndex >= 0 ? qsTr("Save") : qsTr("Add source")
                onClicked: saveSource(false)
            }

            MenuItem {
                text: qsTr("Save and browse")
                onClicked: saveSource(true)
            }
        }

        Column {
            id: contentColumn

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: sourceIndex >= 0 ? qsTr("Edit SMB source") : qsTr("Add SMB source")
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
                id: hostField
                width: parent.width
                label: qsTr("Server")
                placeholderText: qsTr("nas.local or 192.168.1.10")
                text: initialHost
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            }

            TextField {
                id: portField
                width: parent.width
                label: qsTr("Port")
                placeholderText: "445"
                text: initialPort > 0 ? String(initialPort) : "445"
                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhNoPredictiveText
            }

            TextField {
                id: locationField
                width: parent.width
                label: qsTr("SMB path")
                placeholderText: qsTr("/public/Shared Videos")
                text: initialSmbPathText()
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            }

            TextSwitch {
                id: guestSwitch
                text: qsTr("Guest access")
                description: checked
                             ? qsTr("Connect as guest; username and password are not used.")
                             : qsTr("Use username and password for this SMB source.")
                checked: initialGuest
            }

            TextField {
                id: usernameField
                visible: !guestSwitch.checked
                width: parent.width
                label: qsTr("Username")
                placeholderText: qsTr("NAS username")
                text: initialUsername
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            }
            PasswordField {
                id: passwordField
                visible: !guestSwitch.checked
                width: parent.width
                label: qsTr("Password")
                placeholderText: qsTr("NAS password")
                inputMethodHints: Qt.ImhNoPredictiveText
            }

Label {
                visible: !guestSwitch.checked
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("The password is saved in Sailfish Secrets when this field is filled.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            Label {
                id: errorLabel
                visible: text.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.errorColor
                wrapMode: Text.Wrap
            }
}

        VerticalScrollDecorator {}
    }

    Component.onCompleted: loadSavedPassword()
}
