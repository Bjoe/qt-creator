// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

import QtQuick 2.15
import HelperWidgets 2.0
import QtQuick.Layouts 1.15
import StudioControls 1.0 as StudioControls
import StudioTheme 1.0 as StudioTheme

Column {
    anchors.left: parent.left
    anchors.right: parent.right

    Section {
        caption: qsTr("Color Animation")

        anchors.left: parent.left
        anchors.right: parent.right

        SectionLayout {
            PropertyLabel { text: qsTr("From color") }

            ColorEditor {
                backendValue: backendValues.from
                supportGradient: false
            }

            PropertyLabel { text: qsTr("To color") }

            ColorEditor {
                backendValue: backendValues.to
                supportGradient: false
            }
        }
    }

    AnimationTargetSection {}

    AnimationSection { showEasingCurve: true }
}

