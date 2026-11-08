// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Thorsten Becker

// Layout of the monitors and saved windows of a profile. The selected window
// has a knob at each corner to change its size; dragging a window moves it.
// The context property "layoutBridge" is a LayoutBridge.

import QtQuick

Rectangle {
    id: root

    SystemPalette { id: sysPalette; colorGroup: SystemPalette.Active }
    color: sysPalette.base

    // Screen area that is shown, recalculated when the profile changes but
    // not while a window is dragged, so that the scale stays the same
    property rect bounds: Qt.rect(0, 0, 1920, 1080)
    // Area enclosing the work areas saved in the profile: windows can only be
    // moved and resized within it, so they do not cover the taskbar
    property rect monitorBounds: Qt.rect(0, 0, 0, 0)
    // Snap lines for the corner knobs (screen pixels): the edges of the work
    // areas and 1/4, 1/3, 1/2, 2/3 and 3/4 of each monitor
    property var snapXs: []
    property var snapYs: []
    readonly property var snapFractions: [1 / 4, 1 / 3, 1 / 2, 2 / 3, 3 / 4]
    // Distance in view pixels within which an edge snaps
    readonly property real snapDistance: 8
    // Snap line in use while a knob is dragged, NaN if none
    property real guideX: NaN
    property real guideY: NaN
    readonly property real margin: 12
    readonly property real scaleFactor: Math.max(0.0001,
        Math.min((width - 2 * margin) / bounds.width, (height - 2 * margin) / bounds.height))
    readonly property real offsetX: (width - bounds.width * scaleFactor) / 2
    readonly property real offsetY: (height - bounds.height * scaleFactor) / 2
    // Smallest window size in screen pixels when resizing
    readonly property int minimumSize: 100

    function toViewX(x) { return offsetX + (x - bounds.x) * scaleFactor }
    function hasMonitors() { return monitorBounds.width > 0 && monitorBounds.height > 0 }
    // Nearest snap line within snapDistance, or NaN
    function snapLine(value, lines) {
        let best = NaN
        let bestDistance = snapDistance / scaleFactor
        for (const line of lines) {
            const distance = Math.abs(value - line)
            if (distance <= bestDistance) {
                best = line
                bestDistance = distance
            }
        }
        return best
    }
    // Position so that a window of the given size stays within the monitors
    function clampPosition(value, size, start, length) {
        if (!hasMonitors())
            return value
        return Math.max(start, Math.min(value, start + length - size))
    }
    function toViewY(y) { return offsetY + (y - bounds.y) * scaleFactor }

    // Calculates the movement limit and the snap lines from the monitors
    function updateLimits() {
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
        const xs = [], ys = []
        for (const monitor of layoutBridge.monitors) {
            const workRight = monitor.workX + monitor.workWidth
            const workBottom = monitor.workY + monitor.workHeight
            left = Math.min(left, monitor.workX)
            top = Math.min(top, monitor.workY)
            right = Math.max(right, workRight)
            bottom = Math.max(bottom, workBottom)
            xs.push(monitor.workX, workRight)
            ys.push(monitor.workY, workBottom)
            for (const f of snapFractions) {
                xs.push(Math.round(monitor.x + monitor.width * f))
                ys.push(Math.round(monitor.y + monitor.height * f))
            }
        }
        monitorBounds = left === Infinity ? Qt.rect(0, 0, 0, 0)
                                          : Qt.rect(left, top, right - left, bottom - top)
        snapXs = xs
        snapYs = ys
    }

    function updateBounds() {
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
        const add = function(r) {
            left = Math.min(left, r.x)
            top = Math.min(top, r.y)
            right = Math.max(right, r.x + r.width)
            bottom = Math.max(bottom, r.y + r.height)
        }
        for (const monitor of layoutBridge.monitors)
            add(monitor)
        updateLimits()
        // Minimized windows without a restored position are far off screen
        for (const window of layoutBridge.windows) {
            if (window.width > 0 && window.height > 0 && window.x > -30000 && window.y > -30000)
                add(window)
        }
        if (left === Infinity)
            bounds = Qt.rect(0, 0, 1920, 1080)
        else
            bounds = Qt.rect(left, top, Math.max(1, right - left), Math.max(1, bottom - top))
    }

    Connections {
        target: layoutBridge
        function onLayoutChanged() { root.updateBounds() }
    }
    Component.onCompleted: updateBounds()

    // Click on the background: no selection
    MouseArea {
        anchors.fill: parent
        onPressed: layoutBridge.select(-1, -1)
    }

    Repeater {
        model: layoutBridge.monitors

        delegate: Rectangle {
            required property var modelData

            x: root.toViewX(modelData.x)
            y: root.toViewY(modelData.y)
            width: modelData.width * root.scaleFactor
            height: modelData.height * root.scaleFactor
            color: sysPalette.window
            border.color: sysPalette.mid
            border.width: modelData.primary ? 3 : 2

        }
    }


    Repeater {
        model: layoutBridge.windows

        delegate: Rectangle {
            id: win

            required property var modelData

            // Geometry in screen pixels, changed directly while dragging
            property real gx: modelData.x
            property real gy: modelData.y
            property real gw: modelData.width
            property real gh: modelData.height
            readonly property color baseColor: modelData.color
            readonly property bool selected: layoutBridge.selectedApplication === modelData.application
                                             && layoutBridge.selectedInstance === modelData.instance

            function commit() {
                layoutBridge.setWindowRect(modelData.application, modelData.instance,
                                     Math.round(gx), Math.round(gy), Math.round(gw), Math.round(gh))
            }

            x: root.toViewX(gx)
            y: root.toViewY(gy)
            width: gw * root.scaleFactor
            height: gh * root.scaleFactor
            z: selected ? 2 : 1
            color: Qt.rgba(baseColor.r, baseColor.g, baseColor.b, selected ? 0.55 : 0.35)
            border.color: baseColor
            border.width: selected ? 3 : 1.5

            Text {
                anchors.fill: parent
                anchors.margins: 4
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                text: win.modelData.name + " [" + win.modelData.number + "]"
                color: sysPalette.windowText
                font.pixelSize: 11
            }

            // Moving the whole window
            MouseArea {
                id: moveArea

                property point start
                property real startX
                property real startY

                anchors.fill: parent
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                preventStealing: true

                onPressed: (mouse) => {
                    layoutBridge.select(win.modelData.application, win.modelData.instance)
                    start = mapToItem(root, mouse.x, mouse.y)
                    startX = win.gx
                    startY = win.gy
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const p = mapToItem(root, mouse.x, mouse.y)
                    const mb = root.monitorBounds
                    // A window larger than the work area would cover the taskbar
                    if (root.hasMonitors()) {
                        win.gw = Math.min(win.gw, mb.width)
                        win.gh = Math.min(win.gh, mb.height)
                    }
                    win.gx = root.clampPosition(startX + (p.x - start.x) / root.scaleFactor,
                                                win.gw, mb.x, mb.width)
                    win.gy = root.clampPosition(startY + (p.y - start.y) / root.scaleFactor,
                                                win.gh, mb.y, mb.height)
                    win.commit()
                }
                onReleased: layoutBridge.finishEdit()
            }

            // Knobs at the corners for resizing, only for the selected window
            Repeater {
                model: [ { h: -1, v: -1 }, { h: 1, v: -1 }, { h: -1, v: 1 }, { h: 1, v: 1 } ]

                delegate: Rectangle {
                    id: knob

                    required property var modelData

                    visible: win.selected
                    width: 12
                    height: 12
                    radius: 6
                    x: (modelData.h < 0 ? 0 : win.width) - width / 2
                    y: (modelData.v < 0 ? 0 : win.height) - height / 2
                    color: win.baseColor
                    border.color: sysPalette.base
                    border.width: 2

                    MouseArea {
                        property point start
                        property rect startGeometry

                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: knob.modelData.h * knob.modelData.v > 0 ? Qt.SizeFDiagCursor
                                                                             : Qt.SizeBDiagCursor
                        preventStealing: true

                        onPressed: (mouse) => {
                            start = mapToItem(root, mouse.x, mouse.y)
                            startGeometry = Qt.rect(win.gx, win.gy, win.gw, win.gh)
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return
                            const p = mapToItem(root, mouse.x, mouse.y)
                            const dx = (p.x - start.x) / root.scaleFactor
                            const dy = (p.y - start.y) / root.scaleFactor
                            const g = startGeometry
                            let left = g.x, top = g.y, right = g.x + g.width, bottom = g.y + g.height
                            // All edges stay within the work areas
                            const mb = root.monitorBounds
                            const limit = root.hasMonitors()
                            let ddx = dx, ddy = dy
                            if (limit) {
                                left = Math.max(left, mb.x)
                                top = Math.max(top, mb.y)
                                right = Math.min(right, mb.x + mb.width)
                                bottom = Math.min(bottom, mb.y + mb.height)
                                if (knob.modelData.h < 0)
                                    ddx = Math.max(dx, mb.x - left)
                                else
                                    ddx = Math.min(dx, mb.x + mb.width - right)
                                if (knob.modelData.v < 0)
                                    ddy = Math.max(dy, mb.y - top)
                                else
                                    ddy = Math.min(dy, mb.y + mb.height - bottom)
                            }
                            if (knob.modelData.h < 0)
                                left = Math.min(left + ddx, right - root.minimumSize)
                            else
                                right = Math.max(right + ddx, left + root.minimumSize)
                            if (knob.modelData.v < 0)
                                top = Math.min(top + ddy, bottom - root.minimumSize)
                            else
                                bottom = Math.max(bottom + ddy, top + root.minimumSize)

                            // Snap the moved edges, as long as the minimum size stays
                            const sx = root.snapLine(knob.modelData.h < 0 ? left : right, root.snapXs)
                            if (!isNaN(sx) && (knob.modelData.h < 0 ? right - sx : sx - left) >= root.minimumSize) {
                                if (knob.modelData.h < 0)
                                    left = sx
                                else
                                    right = sx
                                root.guideX = sx
                            } else {
                                root.guideX = NaN
                            }
                            const sy = root.snapLine(knob.modelData.v < 0 ? top : bottom, root.snapYs)
                            if (!isNaN(sy) && (knob.modelData.v < 0 ? bottom - sy : sy - top) >= root.minimumSize) {
                                if (knob.modelData.v < 0)
                                    top = sy
                                else
                                    bottom = sy
                                root.guideY = sy
                            } else {
                                root.guideY = NaN
                            }

                            win.gx = left
                            win.gy = top
                            win.gw = right - left
                            win.gh = bottom - top
                            win.commit()
                        }
                        onReleased: {
                            root.guideX = NaN
                            root.guideY = NaN
                            layoutBridge.finishEdit()
                        }
                    }
                }
            }
        }
    }

    // Snap lines while a corner is dragged
    Rectangle {
        z: 4
        visible: !isNaN(root.guideX)
        x: root.toViewX(isNaN(root.guideX) ? 0 : root.guideX) - width / 2
        y: root.toViewY(root.monitorBounds.y)
        width: 1
        height: root.monitorBounds.height * root.scaleFactor
        color: sysPalette.highlight
    }
    Rectangle {
        z: 4
        visible: !isNaN(root.guideY)
        x: root.toViewX(root.monitorBounds.x)
        y: root.toViewY(isNaN(root.guideY) ? 0 : root.guideY) - height / 2
        width: root.monitorBounds.width * root.scaleFactor
        height: 1
        color: sysPalette.highlight
    }

    // Names and resolutions of the monitors above the windows
    Repeater {
        model: layoutBridge.monitors

        delegate: Text {
            required property var modelData

            z: 3
            x: root.toViewX(modelData.x + modelData.width) - width - 6
            y: root.toViewY(modelData.y + modelData.height) - height - 4
            horizontalAlignment: Text.AlignRight
            text: modelData.name + "\n" + modelData.width + " × " + modelData.height
            color: sysPalette.windowText
            style: Text.Outline
            styleColor: sysPalette.base
            font.pixelSize: 12
        }
    }
}