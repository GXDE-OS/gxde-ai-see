// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Gxde
import QtQuick.Layouts

/*
 * 内容工厂: type aware actions for the dragged payload.  Every action is a
 * drop target itself, so the payload can be released straight onto it.
 *
 * 空闲时只有一行（提示 + 「问问小埃同学」），面板不会因为这块空区域变高；
 * 拖拽内容经过时（controller.hasContent）才展开成动作网格。
 */
MimeDropArea {
  id: factory

  required property PortalController controller

  /* 一行动作 32px + 行距 6px；列数固定 4，动作多时也只多两行 */
  readonly property int actionRows: Math.max(1, Math.ceil(controller.actions.length / 4))
  implicitHeight: controller.hasContent ? 44 + actionRows * 38 : 44

  onEntered: (mime) => controller.noteDragContent(mime)
  onExited: controller.noteDragFinished()

  Rectangle {
    anchors.fill: parent
    radius: 10
    color: Qt.rgba(1, 1, 1, factory.controller.hasContent ? 14 / 255 : 10 / 255)
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, factory.controller.hasContent ? 42 / 255 : 34 / 255)
  }

  ColumnLayout {
    anchors.fill: parent
    anchors.leftMargin: 10
    anchors.rightMargin: 8
    anchors.topMargin: 6
    anchors.bottomMargin: 6
    spacing: 6

    RowLayout {
      Layout.fillWidth: true
      Layout.preferredHeight: 32
      spacing: 8

      Label {
        text: "🔧 " + qsTranslate("ContentFactory", "内容工厂")
        color: Qt.rgba(1, 1, 1, 0.88)
        font.pixelSize: 12
        font.weight: Font.DemiBold
      }

      /* 空闲：一行说明撑满中间，不占额外高度 */
      Label {
        Layout.fillWidth: !factory.controller.hasContent
        visible: !factory.controller.hasContent
        text: qsTranslate("ContentFactory", "拖到这里，按类型处理")
        elide: Text.ElideRight
        color: Qt.rgba(1, 1, 1, 0.42)
        font.pixelSize: 11
      }

      Label {
        Layout.fillWidth: true
        visible: factory.controller.hasContent
        text: factory.controller.contentSummary
        elide: Text.ElideRight
        color: Qt.rgba(1, 1, 1, 0.45)
        font.pixelSize: 11
      }

      Label {
        visible: factory.controller.hasContent
        text: qsTranslate("ContentFactory", "拖到动作上松手")
        color: Qt.rgba(1, 1, 1, 0.32)
        font.pixelSize: 11
      }

      DropButton {
        Layout.preferredWidth: 92
        Layout.preferredHeight: 32
        controller: factory.controller
        ask: true
        text: qsTranslate("AskTile", "问问小埃同学")
        hint: qsTranslate("AskTile", "把内容放到这里：新开一个对话并填入内容")
        onClicked: factory.controller.openChat()
        onDroppedOn: (mime) => factory.controller.dropOnAsk(mime)
      }
    }

    GridLayout {
      Layout.fillWidth: true
      visible: factory.controller.hasContent
      columns: 4
      rowSpacing: 6
      columnSpacing: 6

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

    Item {
      Layout.fillHeight: true
      visible: factory.controller.hasContent
    }
  }
}
