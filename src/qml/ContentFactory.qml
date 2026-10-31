// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Gxde
import QtQuick.Layouts

/*
 * 内容工厂: type aware actions for the dragged payload.  Every action is a
 * drop target itself, so the payload can be released straight onto it.
 */
MimeDropArea {
  id: factory

  required property PortalController controller

  onEntered: (mime) => controller.noteDragContent(mime)
  onExited: controller.noteDragFinished()

  Rectangle {
    anchors.fill: parent
    radius: 12
    color: Qt.rgba(1, 1, 1, (factory.controller.hasContent ? 14 : 8) / 255)
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 42 / 255)
  }

  RowLayout {
    anchors.fill: parent
    anchors.leftMargin: 12
    anchors.rightMargin: 12
    anchors.topMargin: 7
    anchors.bottomMargin: 7
    spacing: 10

    ColumnLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: 6

      RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label {
          text: "🔧 内容工厂"
          color: Qt.rgba(1, 1, 1, 0.88)
          font.pixelSize: 12
          font.weight: Font.DemiBold
        }
        Label {
          Layout.fillWidth: true
          text: factory.controller.hasContent
                ? factory.controller.contentSummary
                : qsTranslate("ContentFactory", "拖到这里，按类型处理")
          elide: Text.ElideRight
          color: Qt.rgba(1, 1, 1, 0.45)
          font.pixelSize: 11
        }
        Label {
          text: qsTranslate("ContentFactory", "拖到动作上松手")
          color: Qt.rgba(1, 1, 1, 0.32)
          font.pixelSize: 11
        }
      }

      GridLayout {
        Layout.fillWidth: true
        columns: 3
        rowSpacing: 6
        columnSpacing: 6
        visible: factory.controller.hasContent

        Repeater {
          model: factory.controller.actions

          DropButton {
            required property var modelData

            Layout.fillWidth: true
            Layout.preferredHeight: 32
            controller: factory.controller
            text: modelData.label
            iconName: modelData.icon
            hint: modelData.hint
            enabled: modelData.enabled
            onClicked: factory.controller.runAction(modelData.id)
            onDroppedOn: (mime) => factory.controller.dropOnAction(modelData.id, mime)
          }
        }
      }

      Item { Layout.fillHeight: true }
    }

    DropButton {
      Layout.preferredWidth: 96
      Layout.fillHeight: true
      controller: factory.controller
      ask: true
      text: qsTranslate("AskTile", "问问\n小埃同学")
      hint: qsTranslate("AskTile", "把内容放到这里：新开一个对话并填入内容")
      onClicked: factory.controller.openChat()
      onDroppedOn: (mime) => factory.controller.dropOnAsk(mime)
    }
  }
}
