// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Gxde
import QtQuick.Layouts

/*
 * The floating panel: 传送门 + 暂存区 on top, 内容工厂 below.  While a portal
 * jump is open the window grows to the left and shows the window grid.
 */
Window {
  id: root

  required property PortalController controller

  readonly property bool gridShown: controller.session.active
  readonly property int gridGap: 10

  /* applied by the controller, which keeps the panel in its corner */
  readonly property size wantedSize: gridShown
                                     ? Qt.size(grid.width + gridGap + panel.width,
                                               Math.max(panel.height, grid.height))
                                     : Qt.size(panel.width, panel.height)

  visible: false
  color: "transparent"

  onWantedSizeChanged: controller.resizeWindow(wantedSize.width, wantedSize.height)
  Component.onCompleted: controller.resizeWindow(wantedSize.width, wantedSize.height)

  function updateBlur() {
    const areas = [Qt.rect(panel.x, panel.y, panel.width, panel.height)]
    if (gridShown)
      areas.push(Qt.rect(grid.x, grid.y, grid.width, grid.height))
    controller.setBlurAreas(areas, panelBackground.radius)
  }

  onWidthChanged: Qt.callLater(updateBlur)
  onHeightChanged: Qt.callLater(updateBlur)
  onVisibleChanged: if (visible) Qt.callLater(updateBlur)

  /* like the DTK2 original the panel is always dark, whatever the theme */
  Binding {
    target: GxdeTheme
    property: "isDark"
    value: true
  }

  /* the original's flat translucent button (QPushButton style sheet) */
  component FlatButton: Rectangle {
    id: flat

    property alias text: flatLabel.text

    signal clicked()

    implicitWidth: flatLabel.implicitWidth + 20
    implicitHeight: flatLabel.implicitHeight + 8
    radius: 6
    color: Qt.rgba(1, 1, 1, flatMouse.containsMouse ? 0.16 : 0.08)

    Text {
      id: flatLabel

      anchors.centerIn: parent
      color: Qt.rgba(1, 1, 1, 0.78)
      font.pixelSize: 12
    }

    MouseArea {
      id: flatMouse

      anchors.fill: parent
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor
      onClicked: flat.clicked()
    }
  }

  WindowGrid {
    id: grid

    anchors.left: parent.left
    anchors.bottom: parent.bottom
    visible: root.gridShown
    controller: root.controller
    /* catch the drag in the gap too, so crossing it does not end the jump */
    dropMargin: root.gridGap
    maximumHeight: root.controller.screenHeight - 32
  }

  Item {
    id: panel

    readonly property int margin: 12

    anchors.right: parent.right
    anchors.bottom: parent.bottom
    width: root.controller.stagingOnly ? 360 : 500
    height: margin * 2 + body.implicitHeight

    Rectangle {
      id: panelBackground

      anchors.fill: parent
      radius: 16
      /* DBlurEffectWidget's mask colour in the original */
      color: Qt.rgba(24 / 255, 24 / 255, 28 / 255, 190 / 255)
    }

    /* empty space moves the panel (unless pinned) and opens the menu */
    MouseArea {
      anchors.fill: parent
      acceptedButtons: Qt.LeftButton | Qt.RightButton
      cursorShape: root.controller.fixedPosition ? Qt.ArrowCursor : Qt.SizeAllCursor

      onPressed: (mouse) => {
        if (mouse.button === Qt.RightButton) {
          panelMenu.popup()
        } else {
          const p = mapToItem(null, mouse.x, mouse.y)
          root.controller.beginMove(p.x, p.y)
        }
      }
      onPositionChanged: (mouse) => {
        const p = mapToItem(null, mouse.x, mouse.y)
        root.controller.updateMoveResize(p.x, p.y)
      }
      onReleased: root.controller.endMoveResize()
      onCanceled: root.controller.endMoveResize()
    }

    /* the top edge resizes the list */
    MouseArea {
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.top: parent.top
      height: 10
      enabled: !root.controller.fixedSize
      cursorShape: Qt.SizeVerCursor

      onPressed: (mouse) => {
        const p = mapToItem(null, mouse.x, mouse.y)
        root.controller.beginResize(p.x, p.y)
      }
      onPositionChanged: (mouse) => {
        const p = mapToItem(null, mouse.x, mouse.y)
        root.controller.updateMoveResize(p.x, p.y)
      }
      onReleased: root.controller.endMoveResize()
      onCanceled: root.controller.endMoveResize()
    }

    ColumnLayout {
      id: body

      anchors.fill: parent
      anchors.margins: panel.margin
      spacing: 8

      RowLayout {
        Layout.fillWidth: true
        spacing: 10

        PortalZone {
          Layout.preferredWidth: 176
          Layout.fillHeight: true
          Layout.minimumHeight: 190
          visible: !root.controller.stagingOnly
          controller: root.controller
        }

        ColumnLayout {
          Layout.fillWidth: true
          Layout.alignment: Qt.AlignTop
          spacing: 6

          RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            Layout.leftMargin: 12
            Layout.rightMargin: 12

            Label {
              text: qsTr("暂存区")
              color: Qt.rgba(1, 1, 1, 0.88)
              font.pixelSize: 13
              font.weight: Font.DemiBold
            }
            Item { Layout.fillWidth: true }
            Label {
              text: qsTr("%1 项").arg(root.controller.model.count)
              color: Qt.rgba(1, 1, 1, 0.45)
              font.pixelSize: 11
            }
          }

          RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            visible: root.controller.anyChecked
            spacing: 6

            FlatButton {
              text: qsTr("全选")
              onClicked: root.controller.selectAll()
            }
            FlatButton {
              text: qsTr("反选")
              onClicked: root.controller.invertSelection()
            }
            FlatButton {
              text: qsTr("删除选中")
              onClicked: root.controller.deleteChecked()
            }
            Item { Layout.fillWidth: true }
          }

          StagingList {
            Layout.fillWidth: true
            Layout.preferredHeight: root.controller.listHeight
            controller: root.controller
          }
        }
      }

      ContentFactory {
        Layout.fillWidth: true
        Layout.preferredHeight: 148
        visible: !root.controller.stagingOnly
        controller: root.controller
      }
    }

    Rectangle {
      id: toast

      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: parent.bottom
      anchors.bottomMargin: 14
      width: toastLabel.implicitWidth + 24
      height: toastLabel.implicitHeight + 12
      radius: 8
      color: Qt.rgba(0, 0, 0, 0.72)
      visible: toastTimer.running

      Label {
        id: toastLabel

        anchors.centerIn: parent
        color: "white"
        font.pixelSize: 12
      }

      Timer {
        id: toastTimer

        interval: 2600
      }

      Connections {
        target: root.controller

        function onToast(text) {
          toastLabel.text = text
          toastTimer.restart()
        }
      }
    }
  }

  Menu {
    id: panelMenu

    /* a native popup is placed from the window's screen position, which Qt
     * does not know for a layer-shell surface: keep it inside the panel */
    popupType: Popup.Item

    MenuItem {
      text: qsTr("固定位置")
      checkable: true
      checked: root.controller.fixedPosition
      onTriggered: root.controller.fixedPosition = checked
    }
    MenuItem {
      text: qsTr("恢复默认位置")
      onTriggered: root.controller.resetPosition()
    }
    MenuItem {
      text: qsTr("固定大小")
      checkable: true
      checked: root.controller.fixedSize
      onTriggered: root.controller.fixedSize = checked
    }
    MenuItem {
      text: qsTr("全屏时隐藏")
      checkable: true
      checked: root.controller.hideOnFullscreen
      onTriggered: root.controller.hideOnFullscreen = checked
    }
    MenuSeparator { }
    MenuItem {
      text: qsTr("开机自启与后台保活")
      checkable: true
      checked: root.controller.autostart
      onTriggered: root.controller.autostart = checked
    }
    MenuSeparator { }
    MenuItem {
      text: qsTr("清空暂存区")
      onTriggered: root.controller.clearAll()
    }
    MenuItem {
      text: qsTr("隐藏")
      onTriggered: root.controller.requestHide()
    }
    MenuItem {
      text: qsTr("关于")
      onTriggered: root.controller.showAbout()
    }
    MenuItem {
      text: qsTr("退出")
      onTriggered: root.controller.quit()
    }
  }
}
