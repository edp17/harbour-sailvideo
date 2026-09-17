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

    function nextSkipSeconds(value) {
        if (value <= 5) return 10
        if (value <= 10) return 30
        if (value <= 30) return 60
        if (value <= 60) return 120
        return 5
    }

    function skipLabel(seconds) {
        if (seconds >= 120) return qsTr("2 minutes")
        if (seconds >= 60) return qsTr("1 minute")
        return qsTr("%1 seconds").arg(seconds)
    }

    function nextSlideshowSeconds(value) {
        if (value <= 3) return 5
        if (value <= 5) return 10
        if (value <= 10) return 15
        if (value <= 15) return 30
        return 3
    }

    function nextPercent(value, minimum) {
        var next = Math.round(value / 10) * 10 + 10
        return next > 100 ? minimum : next
    }

    function percentSetting(name, fallback, minimum) {
        try {
            var value = appSettings ? appSettings[name] : undefined
            if (value === undefined || value === null || isNaN(value)) {
                return fallback
            }
            value = Math.round(value)
            if (value < minimum || value > 100) {
                return fallback
            }
            return value
        } catch (ignoredSettingError) {
            return fallback
        }
    }

    function defaultVolumePercent() {
        return percentSetting("defaultVolumePercent", 30, 0)
    }

    function defaultBrightnessPercent() {
        return percentSetting("defaultBrightnessPercent", 50, 20)
    }

    function setDefaultVolumePercent(value) {
        try {
            if (appSettings["defaultVolumePercent"] !== undefined) {
                appSettings["defaultVolumePercent"] = value
            }
        } catch (ignoredVolumeSettingError) {
        }
    }

    function setDefaultBrightnessPercent(value) {
        try {
            if (appSettings["defaultBrightnessPercent"] !== undefined) {
                appSettings["defaultBrightnessPercent"] = value
            }
        } catch (ignoredBrightnessSettingError) {
        }
    }

    function cleanPercent(value, fallback, minimum) {
        if (value === undefined || value === null || isNaN(value)) {
            return fallback
        }
        value = Math.round(value)
        if (value < minimum || value > 100) {
            return fallback
        }
        return value
    }

    function percentLabel(value, fallback, minimum) {
        return cleanPercent(value, fallback, minimum).toString() + "%"
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        Column {
            id: contentColumn

            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Settings")
            }

            SectionHeader {
                text: qsTr("Playback")
            }

            ValueButton {
                label: qsTr("Skip interval")
                value: skipLabel(appSettings.skipSeconds)
                description: qsTr("Used by the player skip-back and skip-forward buttons.")
                onClicked: appSettings.skipSeconds = nextSkipSeconds(appSettings.skipSeconds)
            }

            ValueButton {
                label: qsTr("Picture slideshow interval")
                value: skipLabel(appSettings.pictureSlideshowSeconds)
                description: qsTr("Waiting time between pictures in local and Chromecast slideshows.")
                onClicked: appSettings.pictureSlideshowSeconds =
                           nextSlideshowSeconds(appSettings.pictureSlideshowSeconds)
            }

            ValueButton {
                label: qsTr("New video volume")
                value: percentLabel(defaultVolumePercent(), 30, 0)
                description: qsTr("Applied whenever a new video starts. Tap to cycle by 10%.")
                onClicked: setDefaultVolumePercent(nextPercent(defaultVolumePercent(), 0))
            }

            ValueButton {
                label: qsTr("New video brightness")
                value: percentLabel(defaultBrightnessPercent(), 50, 20)
                description: qsTr("Applied whenever a new video starts. Tap to cycle by 10%.")
                onClicked: setDefaultBrightnessPercent(nextPercent(defaultBrightnessPercent(), 20))
            }

            TextSwitch {
                text: qsTr("Keep display on")
                description: qsTr("Prevent display blanking while a video is playing.")
                checked: appSettings.keepDisplayOn
                onClicked: appSettings.keepDisplayOn = checked
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Video scaling is changed directly from the player screen.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            SectionHeader {
                text: qsTr("NAS browser")
            }

            TextSwitch {
                text: qsTr("Prefer media files")
                description: qsTr("Prefer recognised videos and pictures. If none are detected, show all visible files.")
                checked: appSettings.nasShowOnlyVideos
                onClicked: appSettings.nasShowOnlyVideos = checked
            }

            TextSwitch {
                text: qsTr("Show hidden files")
                description: qsTr("Show dot-prefixed entries such as .hidden folders, .nomedia files and sidecar metadata.")
                checked: appSettings.nasShowHiddenFiles
                onClicked: appSettings.nasShowHiddenFiles = checked
            }

            SectionHeader {
                text: qsTr("Playback history")
            }

            DetailItem {
                label: qsTr("Recently played")
                value: playbackHistory.count
            }

            Button {
                visible: playbackHistory.count > 0
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Clear recent videos")
                onClicked: remorsePopup.execute(qsTr("Clearing recent videos"), function() {
                    playbackHistory.clear()
                    appWindow.currentMediaUrl = ""
                    appWindow.currentPlaybackUrl = ""
                    appWindow.currentMediaTitle = ""
                    appWindow.playbackStatus = ""
                    appWindow.playbackError = ""
                })
            }

            SectionHeader {
                text: qsTr("Storage")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("History, NAS sources and settings are stored in SailVideo's Sailjail app-data directory. SMB passwords are managed from the NAS sources page and stored through Sailfish Secrets.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("History: %1\nNAS sources: %2\nURL sources: %3\nSettings: %4")
                      .arg(playbackHistory.storagePath)
                      .arg(nasSources.storagePath)
                      .arg(networkSources.storagePath)
                      .arg(appSettings.storagePath)
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.WrapAnywhere
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Reset restores playback/browser preferences: Fit scaling, 10 second skip interval, 5 second picture slideshow interval, new video volume 30%, new video brightness 50%, keep-display-on enabled, media-file preference off, hidden files off, and clears the last browsed NAS shortcut. It does not delete NAS sources, passwords or playback history.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Reset playback/browser settings")
                onClicked: remorsePopup.execute(qsTr("Resetting settings"), function() {
                    appSettings.resetToDefaults()
                })
            }
        }

        VerticalScrollDecorator {}
    }
}
