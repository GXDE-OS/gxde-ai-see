// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Gxde

/*
 * A content factory tile: click it, or release a drag onto it.  Looks like
 * the original ActionButton / AskTile style sheets.
 */
MimeDropArea {
  id: target

  required property PortalController controller

  property alias text: label.text
  property string iconName
  property string hint
  /* the blue "问问小埃同学" tile instead of a plain action */
  property bool ask: false

  readonly property bool hovered: mouse.containsMouse
  readonly property bool lit: enabled && (hovered || containsDrag)

  signal clicked()
  signal droppedOn(QtObject mime)

  implicitWidth: content.implicitWidth + 16
  implicitHeight: 32
  acceptDrop: enabled

  onEntered: (mime) => controller.noteDragContent(mime)
  onExited: controller.noteDragFinished()
  onDropped: (mime) => {
    if (enabled)
      droppedOn(mime)
  }

  Rectangle {
    anchors.fill: parent
    radius: target.ask ? 9 : 7
    color: {
      if (target.ask)
        return Qt.rgba(74 / 255, 144 / 255, 1, target.containsDrag ? 0.45 : target.hovered ? 0.32 : 0.16)
      if (!target.enabled)
        return Qt.rgba(1, 1, 1, 0.03)
      if (target.containsDrag)
        return Qt.rgba(120 / 255, 180 / 255, 1, 0.45)
      return target.hovered ? Qt.rgba(120 / 255, 180 / 255, 1, 0.30) : Qt.rgba(1, 1, 1, 0.07)
    }
    border.width: target.ask || target.containsDrag ? 1 : 0
    border.color: {
      if (target.ask)
        return target.containsDrag ? Qt.rgba(180 / 255, 215 / 255, 1, 0.95)
             : target.hovered ? Qt.rgba(160 / 255, 205 / 255, 1, 0.7)
             : Qt.rgba(120 / 255, 180 / 255, 1, 0.28)
      return Qt.rgba(160 / 255, 205 / 255, 1, 0.9)
    }
  }

  Row {
    id: content

    anchors.centerIn: parent
    spacing: 4

    IconImage {
      anchors.verticalCenter: parent.verticalCenter
      width: 16
      height: 16
      sourceSize: Qt.size(16, 16)
      name: target.iconName
      visible: target.iconName !== "" && status === Image.Ready
      opacity: target.enabled ? 1 : 0.4
    }

    Text {
      id: label

      anchors.verticalCenter: parent.verticalCenter
      horizontalAlignment: Text.AlignHCenter
      font.pixelSize: 12
      color: !target.enabled ? Qt.rgba(1, 1, 1, 0.32)
           : target.lit ? "white"
           : Qt.rgba(1, 1, 1, target.ask ? 0.9 : 0.86)
    }
  }

  MouseArea {
    id: mouse

    anchors.fill: parent
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    onClicked: if (target.enabled) target.clicked()
  }

  ToolTip.visible: hovered && hint !== ""
  ToolTip.text: hint
  ToolTip.delay: 600
}
