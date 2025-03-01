// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

import QtQuick 2.15
import QtQuick.Layouts 1.15
import HelperWidgets 2.0
import StudioControls 1.0 as StudioControls
import StudioTheme 1.0 as StudioTheme

StudioControls.ComboBox {
    id: targetComboBox

    property string targetName: anchorBackend.topTarget

    actionIndicatorVisible: false
    model: anchorBackend.possibleTargetItems

    onTargetNameChanged: {
        targetComboBox.currentIndex =
                anchorBackend.indexOfPossibleTargetItem(targetComboBox.targetName)
    }

    Connections {
        target: anchorBackend
        function onInvalidated() {
            targetComboBox.currentIndex =
                    anchorBackend.indexOfPossibleTargetItem(targetComboBox.targetName)
        }
    }
}
