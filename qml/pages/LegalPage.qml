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

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: legalColumn.height + Theme.paddingLarge

        Column {
            id: legalColumn

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Third-party licences")
            }

            SectionHeader {
                text: qsTr("libsmb2")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("SailVideo uses libsmb2 for SMB2/SMB3 network-share access. The bundled libsmb2 library is licensed under LGPL-2.1-or-later. The complete licence text and upstream notices are distributed with the project and package.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            SectionHeader {
                text: qsTr("LLs Video Player")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("SailVideo 1.1.0.5 contains no source code copied, adapted or rewritten from LLs Video Player. Permission and BSD 3-Clause licence material are retained in the source tree for possible future reuse. Any future reuse will retain the original copyright notice and be recorded in the project's code-reuse ledger.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("The presence of the LLs Video Player licence material does not imply endorsement of SailVideo.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
            }
        }

        VerticalScrollDecorator {}
    }
}
