import QtQuick

// 座舱：HUD 符号（玻璃板上的绿色显示）+ 下仪表盘
// 参考真实战机 PFD：速度带 / 高度带 / 俯仰梯 / 飞行路径标记 / 坡度尺 / 航向带
Item {
    id: hud
    anchors.fill: parent

    property real speed: 0
    property real altitude: 0
    property real fuel: 1
    property real heading: 0
    property real pitchDeg: 0
    property real rollDeg: 0
    property real verticalSpeed: 0
    property real agl: 0
    property real gLoad: 1
    property real clock: 0
    property int aam: 4
    property bool boosting: false
    property bool inverted: false

    readonly property color hudGreen: "#8dffd6"
    readonly property color hudDim: "#5aa88f"
    readonly property color hudAmber: "#ffc078"
    readonly property color hudRed: "#ff6b5a"
    readonly property color panelColor: "#16202a"
    readonly property color bezel: "#0b1116"

    property real t0: -1
    readonly property real missionTime: t0 > 0 ? clock - t0 : 0

    // 转速/EGT 这类没有真实模型的读数，按速度和加力推一个合理的假值
    readonly property real rpm: Math.min(101, 62 + speed / 10 + (boosting ? 28 : 0))
    readonly property real egt: Math.min(940, 420 + speed * 1.1 + (boosting ? 260 : 0))
    readonly property real mach: speed / 340

    Timer {
        id: ticker
        interval: 40
        repeat: true
        running: hud.visible
        property int n: 0
        onTriggered: {
            hud.clock = Date.now() / 1000
            if (hud.t0 < 0) hud.t0 = hud.clock
            n++
            symbology.requestPaint()
            if (n % 2 === 0)
                dashCanvas.requestPaint()
        }
    }

    // ══════════════ HUD 符号层 ══════════════
    Canvas {
        id: symbology
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width, h = height
            const cx = w / 2
            const cy = h / 2 - 58
            const ppd = 7.0
            const G = hud.hudGreen

            ctx.strokeStyle = G
            ctx.fillStyle = G
            ctx.lineWidth = 1.4
            ctx.font = "13px Consolas"
            ctx.textAlign = "left"
            ctx.globalAlpha = 0.92

            // ── 俯仰梯（整体按滚转旋转）──
            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(-hud.rollDeg * Math.PI / 180)

            for (let e = -60; e <= 60; e += 10) {
                const dy = (hud.pitchDeg - e) * ppd
                if (Math.abs(dy) > 285) continue
                const half = 128
                const gap = 42

                if (e === 0) {
                    // 地平线：实线 + 两端向下折
                    ctx.beginPath()
                    ctx.moveTo(-half, dy); ctx.lineTo(-gap, dy)
                    ctx.moveTo(gap, dy); ctx.lineTo(half, dy)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(-half, dy); ctx.lineTo(-half, dy + 8)
                    ctx.moveTo(half, dy); ctx.lineTo(half, dy + 8)
                    ctx.stroke()
                } else {
                    // 正俯仰向上折、负俯仰虚线
                    if (e < 0) {
                        const dash = 7
                        for (let x = -half; x < -gap; x += dash * 2) {
                            ctx.beginPath()
                            ctx.moveTo(x, dy); ctx.lineTo(Math.min(x + dash, -gap), dy)
                            ctx.stroke()
                        }
                        for (let x = gap; x < half; x += dash * 2) {
                            ctx.beginPath()
                            ctx.moveTo(x, dy); ctx.lineTo(Math.min(x + dash, half), dy)
                            ctx.stroke()
                        }
                    } else {
                        ctx.beginPath()
                        ctx.moveTo(-half, dy); ctx.lineTo(-gap, dy)
                        ctx.moveTo(gap, dy); ctx.lineTo(half, dy)
                        ctx.stroke()
                    }
                    // 两端的小折角
                    const tickDir = e > 0 ? 1 : -1
                    ctx.beginPath()
                    ctx.moveTo(-half, dy); ctx.lineTo(-half, dy + 6 * tickDir)
                    ctx.moveTo(half, dy); ctx.lineTo(half, dy + 6 * tickDir)
                    ctx.stroke()

                    // 度数标在右端外侧
                    ctx.textAlign = "left"
                    ctx.fillText(String(Math.abs(e)), half + 6, dy + 4)
                }
            }
            ctx.restore()

            // ── 机头标记（固定的炮口十字）──
            ctx.lineWidth = 1.6
            ctx.beginPath()
            ctx.moveTo(cx - 12, cy); ctx.lineTo(cx - 4, cy)
            ctx.moveTo(cx + 4, cy); ctx.lineTo(cx + 12, cy)
            ctx.moveTo(cx, cy - 12); ctx.lineTo(cx, cy - 4)
            ctx.stroke()

            // ── 飞行路径标记（速度矢量的落点，比机头略低）──
            {
                const fpmY = cy + 16
                ctx.beginPath()
                ctx.arc(cx, fpmY, 5, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(cx - 5, fpmY); ctx.lineTo(cx - 14, fpmY)
                ctx.moveTo(cx + 5, fpmY); ctx.lineTo(cx + 14, fpmY)
                ctx.moveTo(cx, fpmY - 5); ctx.lineTo(cx, fpmY - 11)
                ctx.stroke()
            }

            // ── 坡度尺（底部圆弧 + 指针）──
            {
                const by = cy + 150
                const r = 128
                ctx.lineWidth = 1.2
                ctx.beginPath()
                ctx.arc(cx, by, r, -Math.PI * 0.72, -Math.PI * 0.28)
                ctx.stroke()

                for (const a of [-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60]) {
                    const ang = -Math.PI / 2 + a * Math.PI / 180
                    const len = (a % 30 === 0) ? 10 : 6
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(ang) * r, by + Math.sin(ang) * r)
                    ctx.lineTo(cx + Math.cos(ang) * (r - len), by + Math.sin(ang) * (r - len))
                    ctx.stroke()
                }

                // 指针跟着滚转走
                const pa = -Math.PI / 2 - hud.rollDeg * Math.PI / 180
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(cx + Math.cos(pa) * (r + 2), by + Math.sin(pa) * (r + 2))
                ctx.lineTo(cx + Math.cos(pa) * (r - 16), by + Math.sin(pa) * (r - 16))
                ctx.stroke()
            }

            // ── 速度带（左）──
            {
                const x = 78, top = cy - 140, bot = cy + 140
                ctx.lineWidth = 1.3
                ctx.beginPath()
                ctx.moveTo(x, top); ctx.lineTo(x, bot)
                ctx.stroke()

                for (let v = Math.floor((hud.speed - 90) / 10) * 10; v <= hud.speed + 90; v += 10) {
                    if (v < 0) continue
                    const y = cy - (v - hud.speed) * 1.55
                    if (y < top || y > bot) continue
                    const long = v % 50 === 0
                    ctx.beginPath()
                    ctx.moveTo(x, y); ctx.lineTo(x + (long ? 16 : 9), y)
                    ctx.stroke()
                    if (long) {
                        ctx.textAlign = "right"
                        ctx.font = "12px Consolas"
                        ctx.fillText(String(v), x - 6, y + 4)
                    }
                }

                // 当前值读框
                ctx.lineWidth = 1.8
                ctx.strokeRect(x - 2, cy - 12, 62, 24)
                ctx.textAlign = "left"
                ctx.font = "bold 15px Consolas"
                ctx.fillText(String(Math.round(hud.speed)), x + 8, cy + 5)
                ctx.font = "10px Consolas"
                ctx.textAlign = "left"
                ctx.fillText("KT", x + 26, cy - 18)
            }

            // ── 高度带（右）──
            {
                const x = w - 78, top = cy - 140, bot = cy + 140
                ctx.lineWidth = 1.3
                ctx.beginPath()
                ctx.moveTo(x, top); ctx.lineTo(x, bot)
                ctx.stroke()

                for (let a = Math.floor((hud.altitude - 500) / 50) * 50; a <= hud.altitude + 500; a += 50) {
                    if (a < 0) continue
                    const y = cy - (a - hud.altitude) * 0.5
                    if (y < top || y > bot) continue
                    const long = a % 200 === 0
                    ctx.beginPath()
                    ctx.moveTo(x, y); ctx.lineTo(x - (long ? 16 : 9), y)
                    ctx.stroke()
                    if (long) {
                        ctx.textAlign = "left"
                        ctx.font = "12px Consolas"
                        ctx.fillText(String(a), x + 6, y + 4)
                    }
                }

                ctx.lineWidth = 1.8
                ctx.strokeRect(x - 76, cy - 12, 58, 24)
                ctx.textAlign = "right"
                ctx.font = "bold 15px Consolas"
                ctx.fillText(String(Math.round(hud.altitude)), x - 20, cy + 5)

                // 垂直速度条
                const vx = x + 40
                ctx.lineWidth = 1.1
                ctx.beginPath()
                ctx.moveTo(vx, top); ctx.lineTo(vx, bot)
                ctx.stroke()
                const vsClamped = Math.max(-40, Math.min(40, hud.verticalSpeed))
                const vy = cy - vsClamped * 3
                ctx.beginPath()
                ctx.moveTo(vx, cy); ctx.lineTo(vx + 6, vy)
                ctx.stroke()
                ctx.textAlign = "right"
                ctx.font = "10px Consolas"
                ctx.fillText("VS", vx - 4, top - 4)
            }

            // ── 航向带（顶）──
            {
                const y = cy - 190
                const cxw = w / 2
                ctx.textAlign = "center"
                for (let b = 0; b < 360; b += 5) {
                    const d = ((b - hud.heading + 540) % 360) - 180
                    const x = cxw + d * 6.2
                    if (Math.abs(d) > 62) continue
                    const long = b % 30 === 0
                    ctx.beginPath()
                    ctx.moveTo(x, y); ctx.lineTo(x, y + (long ? 9 : 5))
                    ctx.stroke()
                    if (long) {
                        const names = { 0: "N", 90: "E", 180: "S", 270: "W" }
                        const label = names[b] || String(b).padStart(3, "0")
                        ctx.font = "12px Consolas"
                        ctx.fillText(label, x, y - 5)
                    }
                }
                // 当前航向三角
                ctx.beginPath()
                ctx.moveTo(cxw, y + 14); ctx.lineTo(cxw - 6, y + 22); ctx.lineTo(cxw + 6, y + 22)
                ctx.closePath()
                ctx.fill()
            }

            // ── 左上角数据块 ──
            {
                ctx.textAlign = "left"
                const x = 168, y0 = cy - 176
                ctx.fillStyle = "rgba(6,20,16,0.35)"
                ctx.fillRect(x - 6, y0 - 12, 92, 50)
                ctx.fillStyle = G
                ctx.font = "12px Consolas"
                ctx.fillText("AAM " + hud.aam, x, y0)
                ctx.fillText("GUN 500", x, y0 + 16)
                ctx.fillText("M " + hud.mach.toFixed(2), x, y0 + 32)
            }

            // ── 右下角 G 值 / 雷达高度 ──
            {
                ctx.textAlign = "right"
                ctx.font = "13px Consolas"
                const x = w - 100
                ctx.fillStyle = hud.gLoad > 7 ? hud.hudAmber : G
                ctx.fillText("G " + hud.gLoad.toFixed(1), x, cy + 172)
                ctx.fillStyle = G
                if (hud.agl < 2000)
                    ctx.fillText("R " + Math.round(hud.agl / 10) * 10, x, cy + 190)
            }

            // ── 告警（闪烁）──
            {
                const blink = (hud.clock * 2) % 2 < 1.2
                ctx.textAlign = "center"
                ctx.font = "bold 17px Consolas"

                if (hud.speed < 125 && blink) {
                    ctx.fillStyle = hud.hudRed
                    ctx.fillText("STALL", cx, cy + 88)
                }
                if (hud.agl < 90 && hud.verticalSpeed < -6 && blink) {
                    ctx.fillStyle = hud.hudRed
                    ctx.fillText("PULL UP", cx, cy + 116)
                }
                if (hud.fuel < 0.2 && blink) {
                    ctx.fillStyle = hud.hudAmber
                    ctx.fillText("BINGO FUEL", cx, cy + 144)
                }
                if (hud.inverted && blink) {
                    ctx.fillStyle = hud.hudAmber
                    ctx.fillText("INVERTED", cx, cy - 150)
                }
                if (hud.boosting) {
                    ctx.fillStyle = hud.hudAmber
                    ctx.font = "13px Consolas"
                    ctx.textAlign = "left"
                    ctx.fillText("AB", 168, cy - 130)
                }
            }
        }
    }

    // ══════════════ 下仪表盘 ══════════════
    Rectangle {
        id: dash
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 236
        color: hud.panelColor

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 2
            color: "#2d4453"
        }

        Canvas {
            id: dashCanvas
            anchors.fill: parent

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const w = width, h = height
                const G = hud.hudGreen

                // 通用表盘
                function dial(cx, cy, r, value, minV, maxV, label, unit, needleColor) {
                    ctx.fillStyle = "#0d1418"
                    ctx.beginPath(); ctx.arc(cx, cy, r + 4, 0, Math.PI * 2); ctx.fill()
                    ctx.strokeStyle = "#3d5a6b"
                    ctx.lineWidth = 2
                    ctx.beginPath(); ctx.arc(cx, cy, r + 4, 0, Math.PI * 2); ctx.stroke()

                    ctx.strokeStyle = "#7fb8a4"
                    ctx.lineWidth = 1
                    ctx.globalAlpha = 0.75
                    for (let t = 0; t <= 10; t++) {
                        const a = (-220 + t * 26) * Math.PI / 180
                        const inner = t % 2 === 0 ? r - 8 : r - 4
                        ctx.beginPath()
                        ctx.moveTo(cx + Math.cos(a) * inner, cy + Math.sin(a) * inner)
                        ctx.lineTo(cx + Math.cos(a) * (r - 1), cy + Math.sin(a) * (r - 1))
                        ctx.stroke()
                    }
                    ctx.globalAlpha = 1

                    const norm = Math.max(0, Math.min(1, (value - minV) / (maxV - minV)))
                    const ang = (-220 + norm * 260) * Math.PI / 180
                    ctx.strokeStyle = needleColor
                    ctx.lineWidth = 2.4
                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    ctx.lineTo(cx + Math.cos(ang) * (r - 11), cy + Math.sin(ang) * (r - 11))
                    ctx.stroke()

                    ctx.fillStyle = "#ffd9a0"
                    ctx.beginPath(); ctx.arc(cx, cy, 2.6, 0, Math.PI * 2); ctx.fill()

                    ctx.textAlign = "center"
                    ctx.fillStyle = hud.hudDim
                    ctx.font = "9px Consolas"
                    ctx.fillText(label, cx, cy + r + 16)
                    ctx.fillStyle = G
                    ctx.font = "bold 11px Consolas"
                    ctx.fillText(String(Math.round(value)) + unit, cx, cy + r + 29)
                }

                // ── 雷达 ──
                {
                    const cx = 108, cy = h / 2 - 6, r = 66
                    ctx.fillStyle = "#12211a"
                    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.fill()

                    ctx.strokeStyle = "#5fae8c"
                    ctx.lineWidth = 1
                    ctx.globalAlpha = 0.55
                    for (const rr of [r, r * 0.66, r * 0.33]) {
                        ctx.beginPath(); ctx.arc(cx, cy, rr, 0, Math.PI * 2); ctx.stroke()
                    }
                    ctx.beginPath()
                    ctx.moveTo(cx - r, cy); ctx.lineTo(cx + r, cy)
                    ctx.moveTo(cx, cy - r); ctx.lineTo(cx, cy + r)
                    ctx.stroke()
                    ctx.globalAlpha = 1

                    const sweep = (hud.clock * 1.5) % (Math.PI * 2)
                    const grd = ctx.createLinearGradient(cx, cy, cx + Math.cos(sweep) * r, cy + Math.sin(sweep) * r)
                    grd.addColorStop(0, "rgba(157,255,202,0.85)")
                    grd.addColorStop(1, "rgba(157,255,202,0.05)")
                    ctx.strokeStyle = grd
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    ctx.lineTo(cx + Math.cos(sweep) * r, cy + Math.sin(sweep) * r)
                    ctx.stroke()

                    ctx.fillStyle = "#c8ffe0"
                    const blips = [[0.42, 0.12], [-0.24, -0.5], [0.55, -0.32], [-0.5, 0.3], [0.12, 0.62]]
                    for (const b of blips) {
                        ctx.beginPath()
                        ctx.arc(cx + b[0] * r, cy + b[1] * r, 2, 0, Math.PI * 2)
                        ctx.fill()
                    }

                    ctx.fillStyle = "#ffe9a8"
                    ctx.beginPath(); ctx.arc(cx, cy, 2.4, 0, Math.PI * 2); ctx.fill()

                    ctx.textAlign = "center"
                    ctx.fillStyle = hud.hudDim
                    ctx.font = "9px Consolas"
                    ctx.fillText("RDR  40 NM", cx, cy + r + 16)
                }

                // ── ADI 姿态仪 ──
                {
                    const cx = 320, cy = h / 2 - 6, r = 62
                    ctx.save()
                    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.clip()
                    ctx.translate(cx, cy)
                    ctx.rotate(-hud.rollDeg * Math.PI / 180)

                    const off = hud.pitchDeg * 3.2
                    ctx.fillStyle = "#2f6f9e"      // 天
                    ctx.fillRect(-r * 2, -r * 2 + off, r * 4, r * 2)
                    ctx.fillStyle = "#7a5a35"      // 地
                    ctx.fillRect(-r * 2, off, r * 4, r * 2)

                    ctx.strokeStyle = "#e8f6ff"
                    ctx.lineWidth = 1.6
                    ctx.beginPath()
                    ctx.moveTo(-r * 2, off); ctx.lineTo(r * 2, off)
                    ctx.stroke()

                    ctx.strokeStyle = "rgba(255,255,255,0.55)"
                    ctx.lineWidth = 1
                    for (let a = -30; a <= 30; a += 10) {
                        if (a === 0) continue
                        const y = off - a * 3.2
                        ctx.beginPath()
                        ctx.moveTo(-26, y); ctx.lineTo(-8, y)
                        ctx.moveTo(8, y); ctx.lineTo(26, y)
                        ctx.stroke()
                    }
                    ctx.restore()

                    // 固定机影
                    ctx.strokeStyle = "#ffcf6a"
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    ctx.moveTo(cx - 30, cy); ctx.lineTo(cx - 10, cy); ctx.lineTo(cx - 10, cy + 7)
                    ctx.moveTo(cx + 30, cy); ctx.lineTo(cx + 10, cy); ctx.lineTo(cx + 10, cy + 7)
                    ctx.stroke()

                    ctx.strokeStyle = "#3d5a6b"
                    ctx.lineWidth = 2
                    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke()

                    ctx.textAlign = "center"
                    ctx.fillStyle = hud.hudDim
                    ctx.font = "9px Consolas"
                    ctx.fillText("ADI", cx, cy + r + 16)
                }

                // ── HSI 罗盘 ──
                {
                    const cx = 530, cy = h / 2 - 6, r = 62
                    ctx.fillStyle = "#0d1418"
                    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.fill()
                    ctx.strokeStyle = "#3d5a6b"
                    ctx.lineWidth = 2
                    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke()

                    ctx.save()
                    ctx.translate(cx, cy)
                    ctx.rotate(-hud.heading * Math.PI / 180)

                    ctx.textAlign = "center"
                    for (let b = 0; b < 360; b += 10) {
                        const a = (b - 90) * Math.PI / 180
                        const long = b % 30 === 0
                        ctx.strokeStyle = "#7fb8a4"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(Math.cos(a) * (r - 2), Math.sin(a) * (r - 2))
                        ctx.lineTo(Math.cos(a) * (r - (long ? 10 : 5)), Math.sin(a) * (r - (long ? 10 : 5)))
                        ctx.stroke()

                        if (b % 90 === 0) {
                            const names = { 0: "N", 90: "E", 180: "S", 270: "W" }
                            ctx.save()
                            ctx.translate(Math.cos(a) * (r - 20), Math.sin(a) * (r - 20))
                            ctx.rotate(hud.heading * Math.PI / 180)
                            ctx.fillStyle = b === 0 ? "#ffcf6a" : G
                            ctx.font = "bold 12px Consolas"
                            ctx.fillText(names[b], 0, 4)
                            ctx.restore()
                        } else if (long) {
                            ctx.save()
                            ctx.translate(Math.cos(a) * (r - 18), Math.sin(a) * (r - 18))
                            ctx.rotate(hud.heading * Math.PI / 180)
                            ctx.fillStyle = hud.hudDim
                            ctx.font = "9px Consolas"
                            ctx.fillText(String(b / 10), 0, 3)
                            ctx.restore()
                        }
                    }
                    ctx.restore()

                    // 机头指针
                    ctx.fillStyle = "#ffcf6a"
                    ctx.beginPath()
                    ctx.moveTo(cx, cy - r + 4); ctx.lineTo(cx - 6, cy - r - 6); ctx.lineTo(cx + 6, cy - r - 6)
                    ctx.closePath()
                    ctx.fill()

                    ctx.strokeStyle = "#ffcf6a"
                    ctx.lineWidth = 1.4
                    ctx.beginPath()
                    ctx.moveTo(cx, cy - 12); ctx.lineTo(cx, cy + 12)
                    ctx.moveTo(cx - 12, cy); ctx.lineTo(cx + 12, cy)
                    ctx.stroke()

                    ctx.textAlign = "center"
                    ctx.fillStyle = hud.hudDim
                    ctx.font = "9px Consolas"
                    ctx.fillText("HSI  " + String(Math.round(hud.heading)).padStart(3, "0") + "°", cx, cy + r + 16)
                }

                // ── 发动机表：转速 / 排气温度 ──
                dial(690, h / 2 - 22, 40, hud.rpm, 0, 110, "RPM %", "", "#9dffca")
                dial(790, h / 2 - 22, 40, hud.egt, 0, 1000, "EGT °C", "", "#ffb060")

                // ── 燃油竖条 ──
                {
                    const x = w - 150, top = 34, bh = 120, bw = 20
                    ctx.fillStyle = "#0d1418"
                    ctx.fillRect(x, top, bw, bh)
                    ctx.strokeStyle = "#3d5a6b"
                    ctx.lineWidth = 1
                    ctx.strokeRect(x, top, bw, bh)

                    const fh = Math.max(0, (bh - 4) * hud.fuel)
                    ctx.fillStyle = hud.fuel > 0.55 ? "#8dffd6" : hud.fuel > 0.22 ? "#ffd166" : "#ff6b5a"
                    ctx.fillRect(x + 2, top + bh - 2 - fh, bw - 4, fh)

                    ctx.strokeStyle = "#4f9c85"
                    for (let i = 1; i < 5; i++) {
                        const y = top + (bh / 5) * i
                        ctx.beginPath(); ctx.moveTo(x + bw, y); ctx.lineTo(x + bw + 5, y); ctx.stroke()
                    }

                    ctx.textAlign = "center"
                    ctx.fillStyle = hud.boosting ? hud.hudAmber : hud.hudDim
                    ctx.font = "9px Consolas"
                    ctx.fillText(hud.boosting ? "AB" : "FUEL", x + bw / 2, top - 6)
                    ctx.fillStyle = G
                    ctx.font = "bold 11px Consolas"
                    ctx.fillText(String(Math.round(hud.fuel * 100)), x + bw / 2, top + bh + 14)
                }

                // ── 警告灯面板 ──
                {
                    const x = w - 108, y0 = 30
                    const lamps = [
                        { t: "CAUT", on: hud.fuel < 0.22, c: "#ff6b5a" },
                        { t: "FIRE", on: false, c: "#ff6b5a" },
                        { t: "HYD", on: false, c: "#ffc078" },
                        { t: "GEN", on: false, c: "#ffc078" },
                        { t: "AB", on: hud.boosting, c: "#8dffd6" },
                        { t: "RDY", on: hud.aam > 0, c: "#8dffd6" }
                    ]
                    ctx.font = "8px Consolas"
                    ctx.textAlign = "left"
                    for (let i = 0; i < lamps.length; i++) {
                        const yy = y0 + i * 20
                        ctx.fillStyle = lamps[i].on ? lamps[i].c : "#1e2a33"
                        ctx.fillRect(x, yy, 10, 10)
                        ctx.strokeStyle = "#3d5a6b"
                        ctx.lineWidth = 1
                        ctx.strokeRect(x, yy, 10, 10)
                        ctx.fillStyle = lamps[i].on ? lamps[i].c : hud.hudDim
                        ctx.fillText(lamps[i].t, x + 16, yy + 9)
                    }
                }
            }
        }
    }

    // 舱框立柱
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 26
        anchors.left: parent.left
        width: 14
        anchors.bottom: dash.top
        color: hud.bezel
        opacity: 0.9
    }
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 26
        anchors.right: parent.right
        width: 14
        anchors.bottom: dash.top
        color: hud.bezel
        opacity: 0.9
    }

    // 顶部状态条
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 26
        color: "#0c1419"
        opacity: 0.94

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: "HDG " + String(Math.round(hud.heading)).padStart(3, "0") + "°   "
                  + (hud.inverted ? "INVERTED" : "LEVEL")
            color: hud.inverted ? hud.hudAmber : hud.hudGreen
            font.family: "Consolas"
            font.pixelSize: 12
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            text: "GUARD 121.5   ·   RDR ON   ·   AAM " + hud.aam + "   ·   MASTER ARM"
            color: hud.hudDim
            font.family: "Consolas"
            font.pixelSize: 11
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: "T+" + String(Math.floor(hud.missionTime / 60)).padStart(2, "0") + ":"
                  + String(Math.floor(hud.missionTime % 60)).padStart(2, "0")
            color: hud.hudDim
            font.family: "Consolas"
            font.pixelSize: 11
        }
    }
}
