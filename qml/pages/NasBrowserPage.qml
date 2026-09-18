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

    property string sourceTitle: ""
    property string host: ""
    property int port: 445
    property string share: ""
    property string domain: ""
    property string username: ""
    property bool guest: true
    property string currentPath: ""
    property string sourceRootPath: ""
    property bool sourceRootLocked: false
    property var folderScrollPositions: ({})
    property real pendingRestoreContentY: 0
    property string requestId: ""
    property string sessionPassword: ""
    property bool rememberPassword: true
    property bool loadedOnce: false
    property int lastReturnedCount: 0
    property int lastVisibleCount: 0
    property int lastFilteredCount: 0
    property string pendingBrowsePath: ""
    property string sortMode: "name"
    property bool sortAscending: true
    property string pendingMediaKind: ""
    property string pendingMediaName: ""
    property string pendingMediaPath: ""
    property var pendingMediaSize: 0
    property bool pendingMediaIsImage: false

    allowedOrientations: Orientation.All

    ListModel {
        id: entryModel
    }

    Timer {
        id: scrollRestoreTimer
        interval: 50
        repeat: false
        onTriggered: {
            var maxY = Math.max(0, browserFlickable.contentHeight - browserFlickable.height)
            browserFlickable.contentY = Math.max(
                        0, Math.min(page.pendingRestoreContentY, maxY))
        }
    }

    Timer {
        id: pendingActionTimer
        interval: 120
        repeat: true
        running: page.pendingBrowsePath.length > 0 || page.pendingMediaKind.length > 0
        onTriggered: {
            if (smbBackend.busy) {
                return
            }

            if (page.pendingMediaKind.length > 0) {
                var kind = page.pendingMediaKind
                var name = page.pendingMediaName
                var path = page.pendingMediaPath
                var size = page.pendingMediaSize
                var isImage = page.pendingMediaIsImage
                page.clearPendingMediaAction()

                if (kind === "picture") {
                    page.viewPicture(name, path, size)
                } else if (kind === "video") {
                    page.playEntry(name, path, size, isImage)
                }
                return
            }

            if (page.pendingBrowsePath.length > 0) {
                var browsePath = page.pendingBrowsePath
                page.pendingBrowsePath = ""
                page.browse(browsePath)
            }
        }
    }

    function clean(value) {
        return appWindow.cleanedValue(value).replace(/\\/g, "/")
    }

    function pathParent(path) {
        var cleanPath = clean(path)
        while (cleanPath.length > 0 && cleanPath.charAt(cleanPath.length - 1) === "/") {
            cleanPath = cleanPath.substring(0, cleanPath.length - 1)
        }
        var slash = cleanPath.lastIndexOf("/")
        return slash > 0 ? cleanPath.substring(0, slash) : ""
    }

    function pathLeaf(path) {
        var cleanPath = clean(path)
        while (cleanPath.length > 0 && cleanPath.charAt(cleanPath.length - 1) === "/") {
            cleanPath = cleanPath.substring(0, cleanPath.length - 1)
        }
        var slash = cleanPath.lastIndexOf("/")
        return slash >= 0 ? cleanPath.substring(slash + 1) : cleanPath
    }

    function displayPathFor(path) {
        var cleanPath = clean(path)
        return "//" + host + "/" + share
                + (smbBackend.displayPath(cleanPath) === "/" ? "" : smbBackend.displayPath(cleanPath))
    }

    function atSourceRoot() {
        return clean(currentPath) === clean(sourceRootPath)
    }

    function pathInsideSourceRoot(path) {
        var candidate = clean(path)
        var root = clean(sourceRootPath)

        if (root.length === 0) {
            return true
        }

        return candidate === root || candidate.indexOf(root + "/") === 0
    }

    function boundedBrowsePath(path) {
        var candidate = clean(path)
        return pathInsideSourceRoot(candidate) ? candidate : clean(sourceRootPath)
    }

    function relativeBrowsePath() {
        var path = clean(currentPath)
        var root = clean(sourceRootPath)

        if (path === root) {
            return ""
        }

        if (root.length === 0) {
            return path
        }

        if (path.indexOf(root + "/") === 0) {
            return path.substring(root.length + 1)
        }

        return ""
    }

    function breadcrumbText() {
        var relativePath = relativeBrowsePath()
        return relativePath.length > 0 ? ".." + relativePath : ""
    }

    function rememberScrollPosition(path) {
        var key = clean(path)
        folderScrollPositions[key] = Math.max(0, browserFlickable.contentY)
    }

    function restoreScrollPosition(path) {
        var key = clean(path)
        var saved = folderScrollPositions[key]
        pendingRestoreContentY = saved !== undefined && saved !== null
                ? Math.max(0, Number(saved))
                : 0
        scrollRestoreTimer.restart()
    }

    function sourceNameForFolder(folderPath) {
        var leaf = pathLeaf(folderPath)
        if (leaf.length > 0) {
            return leaf
        }
        return sourceTitle && sourceTitle.length > 0
                ? sourceTitle
                : host + "/" + share
    }

    function passwordText() {
        return guest ? "" : sessionPassword
    }

    function credentialStatus(message, error) {
        if (error) {
            errorLabel.text = message || ""
        }
    }

    function loadRememberedPassword() {
        if (guest) {
            return
        }

        var savedPassword = smbCredentialStore.passwordFor(host, port, share, domain, username, guest)
        if (savedPassword && savedPassword.length > 0) {
            sessionPassword = savedPassword
            credentialStatus("", false)
        } else if (smbCredentialStore.lastError.length > 0) {
            credentialStatus(smbCredentialStore.lastError, true)
        } else {
            credentialStatus("", false)
        }
    }

    function saveRememberedPasswordIfNeeded() {
        if (guest || sessionPassword.length === 0) {
            return
        }

        if (rememberPassword) {
            if (smbCredentialStore.storePasswordFor(host, port, share, domain, username, guest, sessionPassword)) {
                credentialStatus("", false)
            } else {
                credentialStatus(smbCredentialStore.lastError, true)
            }
        } else {
            smbCredentialStore.deletePasswordFor(host, port, share, domain, username, guest)
            credentialStatus("", false)
        }
    }

    function canBrowse() {
        return guest || sessionPassword.length > 0
    }

    function clearPendingMediaAction() {
        pendingMediaKind = ""
        pendingMediaName = ""
        pendingMediaPath = ""
        pendingMediaSize = 0
        pendingMediaIsImage = false
    }

    function queueMediaAction(kind, entryName, entryPath, entrySize, entryIsImageValue) {
        pendingBrowsePath = ""
        pendingMediaKind = kind
        pendingMediaName = entryName
        pendingMediaPath = entryPath
        pendingMediaSize = entrySize
        pendingMediaIsImage = entryIsImageValue === true
        statusLabel.text = qsTr("Waiting for the SMB folder request to finish…")
        errorLabel.text = ""
    }

    function requestPassword() {
        pageStack.push(passwordDialogComponent)
    }

    function browse(path) {
        if (!canBrowse()) {
            errorLabel.text = ""
            statusLabel.text = qsTr("Password required")
            requestPassword()
            return
        }

        if (smbBackend.busy) {
            clearPendingMediaAction()
            pendingBrowsePath = clean(path)
            errorLabel.text = ""
            return
        }

        clearPendingMediaAction()

        var nextPath = boundedBrowsePath(path)
        rememberScrollPosition(currentPath)
        currentPath = nextPath

        requestId = String(Date.now()) + "-" + Math.floor(Math.random() * 100000)
        errorLabel.text = ""
        smbBackend.listDirectory(requestId,
                                 host,
                                 port,
                                 share,
                                 currentPath,
                                 domain,
                                 username,
                                 passwordText(),
                                 guest)
    }

    function browseParent() {
        if (!atSourceRoot() && currentPath.length > 0) {
            browse(pathParent(currentPath))
        }
    }

    function retryBrowse() {
        pendingBrowsePath = ""
        clearPendingMediaAction()
        browse(currentPath)
    }

    function saveFolderAsSource(folderName, folderPath) {
        if (!canBrowse()) {
            requestPassword()
            return
        }

        saveRememberedPasswordIfNeeded()

        var name = sourceNameForFolder(folderPath)
        var ok = nasSources.addOrUpdate(name,
                                        host,
                                        port,
                                        share,
                                        folderPath,
                                        domain,
                                        username,
                                        guest)
        if (ok) {
            errorLabel.text = ""
            statusLabel.text = qsTr("Saved as NAS source: %1").arg(name)
        } else {
            errorLabel.text = nasSources.lastError
        }
    }

    function editFolderAsSource(folderName, folderPath) {
        pageStack.push(Qt.resolvedUrl("EditNasSourcePage.qml"),
                       { initialName: sourceNameForFolder(folderPath),
                         initialHost: host,
                         initialPort: port,
                         initialShare: share,
                         initialPath: folderPath,
                         initialDomain: domain,
                         initialUsername: username,
                         initialGuest: guest })
    }

    function playEntry(entryName, entryPath, entrySize, entryIsImageValue) {
        if (entryIsImageValue === true || smbBackend.isLikelyImageFile(entryName)) {
            viewPicture(entryName, entryPath, entrySize)
            return
        }

        if (!canBrowse()) {
            requestPassword()
            return
        }

        if (smbBackend.busy) {
            queueMediaAction("video", entryName, entryPath, entrySize, entryIsImageValue)
            return
        }

        pendingBrowsePath = ""
        appWindow.setSmbPlaybackQueueFromModel(entryModel,
                                               entryPath,
                                               host,
                                               port,
                                               share,
                                               domain,
                                               username,
                                               passwordText(),
                                               guest)
        appWindow.setSmbPictureQueueFromModel(entryModel,
                                              entryPath,
                                              host,
                                              port,
                                              share,
                                              domain,
                                              username,
                                              passwordText(),
                                              guest)
        appWindow.setSmbFolderMediaQueueFromModel(entryModel,
                                                  entryPath,
                                                  host,
                                                  port,
                                                  share,
                                                  domain,
                                                  username,
                                                  passwordText(),
                                                  guest)

        var ok = appWindow.openSmbFile(host,
                                       port,
                                       share,
                                       entryPath,
                                       domain,
                                       username,
                                       passwordText(),
                                       guest,
                                       entrySize,
                                       entryName)
        if (!ok) {
            errorLabel.text = appWindow.playbackStatus
        }
    }

    function viewPicture(entryName, entryPath, entrySize) {
        if (!canBrowse()) {
            requestPassword()
            return
        }

        if (smbBackend.busy) {
            queueMediaAction("picture", entryName, entryPath, entrySize, true)
            return
        }

        pendingBrowsePath = ""
        appWindow.setSmbPictureQueueFromModel(entryModel,
                                              entryPath,
                                              host,
                                              port,
                                              share,
                                              domain,
                                              username,
                                              passwordText(),
                                              guest)
        appWindow.setSmbPlaybackQueueFromModel(entryModel,
                                               entryPath,
                                               host,
                                               port,
                                               share,
                                               domain,
                                               username,
                                               passwordText(),
                                               guest)
        appWindow.setSmbFolderMediaQueueFromModel(entryModel,
                                                  entryPath,
                                                  host,
                                                  port,
                                                  share,
                                                  domain,
                                                  username,
                                                  passwordText(),
                                                  guest)

        var ok = appWindow.openSmbPicture(host,
                                          port,
                                          share,
                                          entryPath,
                                          domain,
                                          username,
                                          passwordText(),
                                          guest,
                                          entrySize,
                                          entryName)
        if (!ok) {
            errorLabel.text = appWindow.playbackStatus
        }
    }

    function entryName(entry) {
        return entry && entry.name ? String(entry.name) : ""
    }

    function entryIsDirectory(entry) {
        return entry && (entry.isDirectory === true || entry.isDirectory === 1)
    }

    function entryIsVideo(entry) {
        var name = entryName(entry)
        if (name.length === 0) {
            return false
        }

        return entry.isVideo === true
                || entry.isVideo === 1
                || smbBackend.isLikelyVideoFile(name)
    }

    function entryIsImage(entry) {
        var name = entryName(entry)
        if (name.length === 0) {
            return false
        }

        return entry.isImage === true
                || entry.isImage === 1
                || smbBackend.isLikelyImageFile(name)
    }

    function isHiddenEntry(entry) {
        var name = entryName(entry)
        return name.length > 0 && name.charAt(0) === "."
    }

    function passesBaseFilter(entry) {
        if (!entry) return false
        if (!appSettings.nasShowHiddenFiles && isHiddenEntry(entry)) return false
        return true
    }

    function passesMediaFilter(entry) {
        if (!passesBaseFilter(entry)) return false
        if (entryIsDirectory(entry)) return true
        return entryIsVideo(entry) || entryIsImage(entry)
    }

    function appendEntry(entry) {
        if (entry) {
            entryModel.append(entry)
        }
    }

    function appendParentEntryIfNeeded() {
        if (currentPath.length === 0 || atSourceRoot()) {
            return
        }

        entryModel.append({
                              "name": "..",
                              "path": pathParent(currentPath),
                              "isDirectory": true,
                              "isVideo": false,
                              "isImage": false,
                              "size": 0,
                              "subtitle": qsTr("Parent folder")
                          })
    }

    function compareEntryNames(a, b) {
        var an = entryName(a).toLowerCase()
        var bn = entryName(b).toLowerCase()
        if (an < bn) return -1
        if (an > bn) return 1
        return 0
    }

    function compareEntries(a, b) {
        var aDir = entryIsDirectory(a)
        var bDir = entryIsDirectory(b)
        if (aDir && !bDir) return -1
        if (!aDir && bDir) return 1

        var result = 0
        if (sortMode === "size" && !aDir && !bDir) {
            var asize = a && a.size ? a.size : 0
            var bsize = b && b.size ? b.size : 0
            if (asize < bsize) result = -1
            else if (asize > bsize) result = 1
            else result = compareEntryNames(a, b)
        } else if (sortMode === "type") {
            var at = entryIsImage(a) ? "1-image" : (entryIsVideo(a) ? "2-video" : "3-other")
            var bt = entryIsImage(b) ? "1-image" : (entryIsVideo(b) ? "2-video" : "3-other")
            if (at < bt) result = -1
            else if (at > bt) result = 1
            else result = compareEntryNames(a, b)
        } else {
            result = compareEntryNames(a, b)
        }

        return sortAscending ? result : -result
    }

    function sortEntries(entries) {
        return entries.sort(function(a, b) { return compareEntries(a, b) })
    }

    function sortDescription() {
        var base = sortMode === "size" ? qsTr("size") : (sortMode === "type" ? qsTr("type") : qsTr("name"))
        return qsTr("%1, %2").arg(base).arg(sortAscending ? qsTr("ascending") : qsTr("descending"))
    }

    function sortModeLabel() {
        if (sortMode === "size") return qsTr("Size")
        if (sortMode === "type") return qsTr("Type")
        return qsTr("Name")
    }

    function cycleSortMode() {
        if (sortMode === "name") sortMode = "type"
        else if (sortMode === "type") sortMode = "size"
        else sortMode = "name"
        if (loadedOnce) browse(page.currentPath)
    }

    function rebuildList(entries) {
        entryModel.clear()
        appendParentEntryIfNeeded()

        for (var j = 0; j < entries.length; ++j) {
            appendEntry(entries[j])
        }
    }

    function iconForEntry(entryNameValue, directoryValue, imageValue, videoValue) {
        if (entryNameValue === "..") {
            return "image://theme/icon-m-back"
        }
        if (directoryValue) {
            return "image://theme/icon-m-file-folder"
        }
        if (imageValue) {
            return "image://theme/icon-m-file-image"
        }
        if (videoValue) {
            return "image://theme/icon-m-media-video"
        }
        return "image://theme/icon-m-document"
    }

    Connections {
        target: smbBackend

        onDirectoryReady: {
            if (finishedRequestId !== page.requestId) {
                return
            }

            page.loadedOnce = true

            if (errorString && errorString.length > 0) {
                entryModel.clear()
                if (String(errorString).indexOf("Another SMB request is still running") >= 0) {
                    pendingBrowsePath = currentPath
                    statusLabel.text = qsTr("Retrying SMB folder request…")
                    errorLabel.text = ""
                    return
                }
                statusLabel.text = ""
                errorLabel.text = errorString
                return
            }

            appSettings.setLastNasSource(sourceTitle, host, port, share, currentPath,
                                         domain, username, guest)

            var totalCount = entries.length
            var baseEntries = []
            var mediaEntries = []

            for (var i = 0; i < entries.length; ++i) {
                var entry = entries[i]
                if (passesBaseFilter(entry)) {
                    baseEntries.push(entry)
                    if (passesMediaFilter(entry)) {
                        mediaEntries.push(entry)
                    }
                }
            }

            var selectedEntries = baseEntries
            var usedMediaFilter = false
            if (appSettings.nasShowOnlyVideos && mediaEntries.length > 0) {
                selectedEntries = mediaEntries
                usedMediaFilter = true
            }

            selectedEntries = sortEntries(selectedEntries)
            rebuildList(selectedEntries)
            restoreScrollPosition(currentPath)

            page.lastReturnedCount = totalCount
            page.lastVisibleCount = baseEntries.length
            page.lastFilteredCount = mediaEntries.length

            saveRememberedPasswordIfNeeded()

            if (entryModel.count > 0) {
                if (appSettings.nasShowOnlyVideos && !usedMediaFilter && baseEntries.length > 0) {
                    statusLabel.text = qsTr("No media-only matches; showing all %1 visible item(s)").arg(baseEntries.length)
                } else if (baseEntries.length === totalCount) {
                    statusLabel.text = qsTr("%1 item(s) · %2").arg(baseEntries.length).arg(sortDescription())
                } else {
                    statusLabel.text = qsTr("%1 of %2 item(s) · %3").arg(baseEntries.length).arg(totalCount).arg(sortDescription())
                }
            } else {
                statusLabel.text = totalCount > 0
                        ? qsTr("No visible files in this folder. Check hidden-file filtering.")
                        : qsTr("This folder is empty")
            }
        }
    }

    Component {
        id: passwordDialogComponent

        Dialog {
            id: passwordDialog

            allowedOrientations: Orientation.All
            canAccept: dialogPasswordField.text.length > 0

            SilicaFlickable {
                anchors.fill: parent
                contentHeight: dialogColumn.height + Theme.paddingLarge

                Column {
                    id: dialogColumn

                    width: parent.width
                    spacing: Theme.paddingLarge

                    DialogHeader {
                        acceptText: qsTr("Browse")
                        cancelText: qsTr("Cancel")
                    }

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        text: qsTr("Enter the password for %1.").arg(displayPathFor(currentPath))
                        color: Theme.secondaryColor
                        wrapMode: Text.Wrap
                    }

                    TextField {
                        id: dialogPasswordField
                        width: parent.width
                        label: qsTr("Password")
                        placeholderText: qsTr("NAS password")
                        echoMode: TextInput.Password
                        inputMethodHints: Qt.ImhNoPredictiveText
                        EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                        EnterKey.onClicked: if (passwordDialog.canAccept) passwordDialog.accept()
                    }

                    TextSwitch {
                        id: dialogRememberSwitch
                        text: qsTr("Remember password")
                        description: qsTr("Store this NAS password in Sailfish Secrets after a successful browse.")
                        checked: page.rememberPassword
                    }
                }
            }

            onAccepted: {
                page.sessionPassword = dialogPasswordField.text
                page.rememberPassword = dialogRememberSwitch.checked
                page.browse(page.currentPath)
            }
        }
    }

    SilicaFlickable {
        id: browserFlickable
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: browse(page.currentPath)
            }
        }

        Column {
            id: contentColumn

            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                id: nasPageHeader
                title: sourceTitle && sourceTitle.length > 0 ? sourceTitle : qsTr("SMB share")
                description: " "

                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        leftMargin: nasPageHeader.leftMargin
                        rightMargin: nasPageHeader.rightMargin
                        bottomMargin: Theme.paddingSmall
                    }
                    visible: page.breadcrumbText().length > 0
                    text: page.breadcrumbText()
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    horizontalAlignment: Text.AlignRight
                    wrapMode: Text.NoWrap
                    elide: Text.ElideLeft
                }

                extraContent.children: [
                    BusyIndicator {
                        anchors {
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                        }
                        running: smbBackend.busy
                        size: BusyIndicatorSize.Medium
                    }
                ]
            }

            Row {
                visible: loadedOnce
                width: parent.width
                spacing: Theme.paddingSmall

                BackgroundItem {
                    width: Math.floor((parent.width - 2 * Theme.paddingSmall) / 3)
                    height: Theme.itemSizeMedium
                    onClicked: {
                        appSettings.nasShowOnlyVideos = !appSettings.nasShowOnlyVideos
                        if (loadedOnce) browse(page.currentPath)
                    }

                    Row {
                        anchors.centerIn: parent
                        height: filterText.height
                        spacing: Theme.paddingSmall

                        Label {
                            id: filterIcon
                            anchors.verticalCenter: filterText.verticalCenter
                            width: Theme.iconSizeExtraSmall
                            height: filterText.height
                            text: "◉"
                            color: parent.parent.highlighted ? Theme.highlightColor : Theme.highlightColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        Label {
                            id: filterText
                            text: appSettings.nasShowOnlyVideos ? qsTr("Media") : qsTr("All")
                            color: parent.parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                BackgroundItem {
                    width: Math.floor((parent.width - 2 * Theme.paddingSmall) / 3)
                    height: Theme.itemSizeMedium
                    onClicked: cycleSortMode()

                    Row {
                        anchors.centerIn: parent
                        height: sortText.height
                        spacing: Theme.paddingSmall

                        Label {
                            id: sortIcon
                            anchors.verticalCenter: sortText.verticalCenter
                            width: Theme.iconSizeExtraSmall
                            height: sortText.height
                            text: "↕"
                            color: parent.parent.highlighted ? Theme.highlightColor : Theme.highlightColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        Label {
                            id: sortText
                            text: sortModeLabel()
                            color: parent.parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                BackgroundItem {
                    width: Math.floor((parent.width - 2 * Theme.paddingSmall) / 3)
                    height: Theme.itemSizeMedium
                    onClicked: {
                        sortAscending = !sortAscending
                        if (loadedOnce) browse(page.currentPath)
                    }

                    Row {
                        anchors.centerIn: parent
                        height: orderText.height
                        spacing: Theme.paddingSmall

                        Label {
                            id: orderIcon
                            anchors.verticalCenter: orderText.verticalCenter
                            width: Theme.iconSizeExtraSmall
                            height: orderText.height
                            text: "A"
                            color: parent.parent.highlighted ? Theme.highlightColor : Theme.highlightColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        Label {
                            id: orderText
                            text: sortAscending ? qsTr("A→Z") : qsTr("Z→A")
                            color: parent.parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            Label {
                visible: !smbBackend.backendAvailable()
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("This build cannot browse SMB until libsmb2 is bundled into the RPM.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            Button {
                visible: !guest && !canBrowse()
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Enter password")
                onClicked: requestPassword()
            }

            Button {
                visible: !loadedOnce && canBrowse()
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Browse")
                onClicked: browse(currentPath)
            }

            Label {
                id: statusLabel
                visible: text.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: guest ? qsTr("Ready") : qsTr("Password required")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.NoWrap
                truncationMode: TruncationMode.Fade
            }

            Label {
                id: errorLabel
                visible: text.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.errorColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Button {
                visible: errorLabel.text.length > 0 && canBrowse()
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Retry")
                onClicked: retryBrowse()
            }

            Button {
                visible: loadedOnce && entryModel.count === 0
                         && (appSettings.nasShowOnlyVideos || !appSettings.nasShowHiddenFiles)
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Show all files")
                onClicked: {
                    appSettings.nasShowOnlyVideos = false
                    appSettings.nasShowHiddenFiles = true
                    browse(currentPath)
                }
            }


            Repeater {
                model: entryModel

                delegate: ListItem {
                    id: entryItem

                    width: contentColumn.width
                    contentHeight: Theme.itemSizeLarge

                    property bool entryImage: page.entryIsImage({"name": name, "isImage": isImage})
                    property bool entryVideo: page.entryIsVideo({"name": name, "isVideo": isVideo})

                    menu: ContextMenu {
                        MenuItem {
                            visible: isDirectory && name !== ".."
                            text: qsTr("Open folder")
                            onClicked: browse(path)
                        }

                        MenuItem {
                            visible: isDirectory && name !== ".."
                            text: qsTr("Save folder as source")
                            onClicked: saveFolderAsSource(name, path)
                        }

                        MenuItem {
                            visible: isDirectory && name !== ".."
                            text: qsTr("Edit as source")
                            onClicked: editFolderAsSource(name, path)
                        }

                        MenuItem {
                            visible: !isDirectory && entryItem.entryImage
                            text: qsTr("View picture")
                            onClicked: viewPicture(name, path, size)
                        }

                        MenuItem {
                            visible: !isDirectory && !entryItem.entryImage
                            text: qsTr("Play")
                            onClicked: playEntry(name, path, size, false)
                        }
                    }

                    onClicked: {
                        if (isDirectory) {
                            browse(path)
                        } else if (entryItem.entryImage) {
                            viewPicture(name, path, size)
                        } else {
                            playEntry(name, path, size, entryItem.entryImage)
                        }
                    }

                    Row {
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: Theme.paddingMedium

                        Item {
                            width: Theme.iconSizeMedium
                            height: Theme.iconSizeMedium
                            anchors.verticalCenter: parent.verticalCenter

                            Image {
                                id: typeIcon
                                anchors.centerIn: parent
                                width: Theme.iconSizeMedium
                                height: Theme.iconSizeMedium
                                source: page.iconForEntry(name, isDirectory, entryItem.entryImage, entryItem.entryVideo)
                                opacity: entryItem.highlighted ? 1.0 : 0.75
                            }

                            Label {
                                visible: typeIcon.status === Image.Error
                                anchors.centerIn: parent
                                text: name === ".." ? ".." : (isDirectory ? qsTr("DIR") : (entryItem.entryImage ? qsTr("IMG") : (entryItem.entryVideo ? qsTr("VID") : "•")))
                                color: entryItem.highlighted ? Theme.highlightColor : Theme.secondaryColor
                                font.pixelSize: Theme.fontSizeTiny
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        Column {
                            width: parent.width - Theme.iconSizeMedium - Theme.paddingMedium
                            spacing: Theme.paddingSmall / 2
                            anchors.verticalCenter: parent.verticalCenter

                            Label {
                                width: parent.width
                                text: name
                                color: entryItem.highlighted
                                       ? Theme.highlightColor
                                       : Theme.primaryColor
                                truncationMode: TruncationMode.Fade
                            }

                            Label {
                                width: parent.width
                                text: name === ".."
                                      ? qsTr("Parent folder")
                                      : (isDirectory
                                         ? qsTr("Folder · long-tap to save")
                                         : (entryItem.entryImage
                                            ? qsTr("Picture · %1").arg(subtitle)
                                            : (entryItem.entryVideo ? qsTr("Video · %1").arg(subtitle) : subtitle)))
                                color: Theme.secondaryColor
                                font.pixelSize: Theme.fontSizeExtraSmall
                                truncationMode: TruncationMode.Fade
                            }
                        }
                    }
                }
            }

            Label {
                visible: loadedOnce && entryModel.count === 0 && errorLabel.text.length === 0 && !smbBackend.busy
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: page.lastReturnedCount > 0
                      ? qsTr("This SMB folder returned %1 item(s), but none are visible with the current filter.").arg(page.lastReturnedCount)
                      : qsTr("No files or folders were returned by this SMB folder.")
                color: Theme.secondaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }

        VerticalScrollDecorator {}
    }

    Component.onCompleted: {
        sourceRootPath = sourceRootLocked
                ? clean(sourceRootPath)
                : clean(currentPath)
        currentPath = boundedBrowsePath(currentPath)

        if (guest) {
            browse(currentPath)
        } else {
            loadRememberedPassword()
            if (sessionPassword.length > 0) {
                browse(currentPath)
            }
        }
    }
}
