/*
 * Copyright (C) 2026 edp17
 * GPL-3.0-or-later
 */
import QtQuick 2.0
import Sailfish.Silica 1.0

PullDownMenu {
    id: root
    property string sourceKind: "video"

    MenuItem {
        visible: appWindow.videoCastActive
        text: qsTr("Leave playing on TV")
        onClicked: appWindow.detachCastKeepPlaying()
    }

    Repeater {
        model: castDeviceModel
        delegate: MenuItem {
            property bool sameDevice: castManager.connected
                                      && String(host) === String(castManager.host)
                                      && (port > 0 ? port : 8009)
                                         === (castManager.port > 0 ? castManager.port : 8009)
            property bool sameMedia: sameDevice
                                     && ((root.sourceKind === "picture")
                                         === appWindow.castActivePicture)

            text: sameMedia
                  ? qsTr("Connected: %1").arg(name)
                  : (sameDevice
                     ? (root.sourceKind === "picture"
                        ? qsTr("Cast current picture to %1").arg(name)
                        : qsTr("Cast current video to %1").arg(name))
                     : qsTr("Cast to %1").arg(name))
            enabled: !castManager.disconnecting
                     && !sameMedia
                     && (!appWindow.castMode || sameDevice)
            onClicked: appWindow.castFromMediaPage(root.sourceKind, name, host, port)
        }
    }

    MenuItem {
        text: castDeviceModel.discovering
              ? qsTr("Stop Chromecast scan")
              : qsTr("Scan for Chromecast")
        onClicked: {
            // On a fresh install prepareCastTarget() starts discovery itself.
            // Calling it here made the old toggle immediately stop that scan.
            appWindow.castTargetKind = root.sourceKind === "picture"
                    ? "picture"
                    : "video"

            if (castDeviceModel.discovering) {
                castDeviceModel.stopDiscovery()
                appWindow.showAdjustmentStatus(qsTr("Chromecast scan stopped"))
            } else {
                castDeviceModel.startDiscovery()
                if (castDeviceModel.discovering) {
                    appWindow.showAdjustmentStatus(qsTr("Scanning for Chromecast…"))
                } else if (castDeviceModel.lastError.length > 0) {
                    appWindow.showAdjustmentStatus(castDeviceModel.lastError)
                }
            }
        }
    }

    MenuItem {
        text: qsTr("Settings")
        onClicked: pageStack.push(Qt.resolvedUrl("../pages/SettingsPage.qml"))
    }
}
