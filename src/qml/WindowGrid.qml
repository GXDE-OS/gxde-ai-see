// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Gxde

/*
 * The portal's window grid, left of the panel.  Every window is minimized
 * while it is shown; hovering a tile with the drag brings that window back
 * (the drag then continues into it), leaving restores everything.
 */
Item {
  id: grid

  required property PortalController controller
  property int dropMargin: 0
  property int maximumHeight: 800

  readonly property int cellWidth: 116
  readonly property int cellHeight: 108
  readonly property int padding: 12
  readonly property int headerHeight: 30
  readonly property int columns: Math.max(1, Math.min(4, controller.session.count))
  readonly property int rows: Math.ceil(controller.session.count / columns)
  /* how long a tile has to be hovered before its window comes up */
  readonly property int dwell: 350

  width: columns * cellWidth + padding * 2
  height: Math.min(maximumHeight, headerHeight + rows * cellHeight + padding * 2)

  Rectangle {
    anchors.fill: parent
    radius: 16
    /* same glass as the panel */
    color: Qt.rgba(24 / 255, 24 / 255, 28 / 255, 190 / 255)
  }

  /* the grid and the gap to the portal count as "still in the portal" */
  MimeDropArea {
    anchors.fill: parent
    anchors.rightMargin: -grid.dropMargin
    acceptDrop: false

    onEntered: grid.controller.portalEntered()
    onExited: grid.controller.portalLeft()
    onDropped: grid.controller.portalDropped()
  }

  Label {
    x: grid.padding + 4
    y: grid.padding
    height: grid.headerHeight - 6
    verticalAlignment: Text.AlignVCenter
    text: qsTr("拖到窗口上停留片刻，切换到该窗口")
    font.pixelSize: 12
    opacity: 0.6
  }

  GridView {
    id: view

    x: grid.padding
    y: grid.padding + grid.headerHeight
    width: grid.columns * grid.cellWidth
    height: grid.height - y - grid.padding
    clip: true
    interactive: false
    cellWidth: grid.cellWidth
    cellHeight: grid.cellHeight
    model: grid.controller.session

    delegate: MimeDropArea {
      id: tile

      required property string windowId
      required property string title
      required property string iconSource

      width: grid.cellWidth
      height: grid.cellHeight
      /* the drop belongs to the window behind, never to the tile */
      acceptDrop: false

      onEntered: grid.controller.portalEntered()
      onExited: grid.controller.portalLeft()
      onDropped: grid.controller.jumpTo(tile.windowId)

      Timer {
        interval: grid.dwell
        running: tile.containsDrag
        onTriggered: grid.controller.jumpTo(tile.windowId)
      }

      Rectangle {
        anchors.fill: parent
        anchors.margins: 4
        radius: 10
        color: tile.containsDrag ? Qt.rgba(74 / 255, 144 / 255, 1, 62 / 255) : "transparent"
        border.width: tile.containsDrag ? 1 : 0
        border.color: Qt.rgba(120 / 255, 180 / 255, 1, 230 / 255)

        /* fills up while hovering: the window comes up when it is full */
        Rectangle {
          anchors.left: parent.left
          anchors.bottom: parent.bottom
          anchors.margins: 6
          height: 3
          radius: 1.5
          color: Qt.rgba(120 / 255, 180 / 255, 1, 230 / 255)
          width: 0

          NumberAnimation on width {
            running: tile.containsDrag
            from: 0
            to: tile.width - 20
            duration: grid.dwell
          }
        }
      }

      Image {
        id: icon

        anchors.horizontalCenter: parent.horizontalCenter
        y: 14
        width: 48
        height: 48
        sourceSize: Qt.size(64, 64)
        fillMode: Image.PreserveAspectFit
        cache: false
        smooth: true
        source: tile.iconSource
        scale: tile.containsDrag ? 1.1 : 1.0

        Behavior on scale {
          NumberAnimation { duration: 120 }
        }
      }

      Label {
        anchors.top: icon.bottom
        anchors.topMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 16
        horizontalAlignment: Text.AlignHCenter
        text: tile.title
        elide: Text.ElideRight
        maximumLineCount: 2
        wrapMode: Text.Wrap
        font.pixelSize: 11
      }
    }
  }
}
