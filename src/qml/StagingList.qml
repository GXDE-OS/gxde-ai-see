// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Gxde

/* 暂存区: drop to keep, drag an entry out to use it (pinned ones stay). */
Item {
  id: list

  required property PortalController controller

  function typeLabel(type, text) {
    switch (type) {
    case 0:
      return qsTranslate("QObject", "文件")
    case 2:
      return qsTranslate("QObject", "图片")
    case 1:
      return qsTranslate("QObject", "文字 · %1 字").arg(text.length)
    }
    return ""
  }

  ListView {
    id: view

    /* the row under a drag, painted as hovered */
    property int dragRow: -1

    anchors.fill: parent
    clip: true
    model: list.controller.model
    boundsBehavior: Flickable.StopAtBounds

    ScrollBar.vertical: ScrollBar { }

    delegate: StagingRowItem {
      id: row

      required property int index
      required property int type
      required property string text
      required name
      required icon
      required checked
      required pinned

      width: view.width
      height: 56
      typeLabel: list.typeLabel(type, text)
      highlighted: view.dragRow === index
      foreground: "white"

      onCheckClicked: list.controller.toggleChecked(index)
      onPinClicked: list.controller.togglePinned(index)
      onCloseClicked: {
        if (!removeAnimation.running)
          removeAnimation.start()
      }
      onDragRequested: list.controller.startItemDrag(index)

      NumberAnimation {
        id: removeAnimation

        target: row
        property: "removeProgress"
        from: 0
        to: 1
        duration: 200
        easing.type: Easing.OutCubic
        onFinished: list.controller.removeItem(row.index)
      }
    }
  }

  Label {
    anchors.fill: parent
    anchors.margins: 12
    visible: view.count === 0
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    wrapMode: Text.WordWrap
    text: qsTranslate("StagingView", "把文件、文字或图片拖到这里")
    font.pixelSize: 12
    color: Qt.rgba(1, 1, 1, 105 / 255)
  }

  /* on top of the rows; it takes no mouse events */
  MimeDropArea {
    anchors.fill: parent

    onEntered: (mime) => list.controller.noteDragContent(mime)
    onMoved: (mime, x, y) => view.dragRow = view.indexAt(x, y + view.contentY)
    onExited: {
      view.dragRow = -1
      list.controller.noteDragFinished()
    }
    onDropped: (mime, x, y) => {
      view.dragRow = -1
      list.controller.dropOnStaging(mime, view.indexAt(x, y + view.contentY))
    }
  }
}
