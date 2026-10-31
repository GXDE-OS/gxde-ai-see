// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Gxde

/* Hover a drag here: the other windows get out of the way (see WindowGrid). */
MimeDropArea {
  id: zone

  required property PortalController controller

  /* only the compositor overview forwards a drop made here */
  acceptDrop: controller.portalOpen && !controller.session.active

  onEntered: (mime) => {
    controller.noteDragContent(mime)
    controller.portalEntered()
  }
  onExited: {
    controller.portalLeft()
    controller.noteDragFinished()
  }
  onDropped: {
    controller.noteDragFinished()
    controller.portalDropped()
  }

  PortalZoneItem {
    anchors.fill: parent
    active: zone.containsDrag
    foreground: "white"
    title: qsTranslate("PortalZone", "传送门")
    subtitle: qsTranslate("PortalZone", "拖到这里\n选择窗口跃迁")
  }

  Label {
    id: hint

    x: 12
    y: parent.height - 84
    width: parent.width - 24
    height: 36
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    wrapMode: Text.WordWrap
    font.pixelSize: 11
    color: Qt.rgba(1, 1, 1, 0.62)
    visible: hintTimer.running

    Timer {
      id: hintTimer

      interval: 2600
    }

    Connections {
      target: zone.controller

      function onZoneMessage(text) {
        hint.text = text
        hintTimer.restart()
      }
    }
  }
}
