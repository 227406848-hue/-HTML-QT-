import QtQuick
import DuskPatrol 1.0

Item {
    id: root
    focus: true

    GameItem {
        id: gameItem
        anchors.fill: parent
    }

    // T 切视角；空格特技（单击 180° 倒飞 / 双击 360° 翻滚）
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_T) {
            gameItem.cycleView()
            event.accepted = true
        } else if (event.key === Qt.Key_Space) {
            gameItem.spaceTap()
            event.accepted = true
        } else if (event.key === Qt.Key_M) {
            gameItem.toggleMute()
            event.accepted = true
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPositionChanged: (m) => {
            gameItem.stickX = Math.max(-1, Math.min(1, (m.x / width) * 2 - 1))
            gameItem.stickY = Math.max(-1, Math.min(1, -(m.y / height) * 2 + 1))
        }
        onPressed: (m) => {
            if (m.button === Qt.LeftButton) gameItem.boosting = true
            else if (m.button === Qt.RightButton) gameItem.fireMissile()
        }
        onReleased: (m) => {
            if (m.button === Qt.LeftButton) gameItem.boosting = false
        }
        onDoubleClicked: (m) => {
            if (m.button === Qt.LeftButton) gameItem.triggerRoll()
        }
    }

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 18
        visible: gameItem.viewIndex !== 2
        text: "FPS " + Math.round(game.fps) + "   VIEW " + ["TOP", "CHASE", "COCKPIT"][gameItem.viewIndex]
              + (gameItem.muted ? "   MUTED" : "")
        color: "#fff3dc"
        font.family: "Consolas"
        font.pixelSize: 16
        style: Text.Outline
        styleColor: "#1b1326"
    }

    Text {
        anchors.centerIn: parent
        visible: !gameItem.worldReady
        text: "SPOOLING UP THE ENGINES"
        color: "#ffd884"
        font.family: "Consolas"
        font.pixelSize: 20
        style: Text.Outline
        styleColor: "#1b1326"
    }

    // 座舱仪表盘（只在座舱视角显示）
    CockpitHud {
        anchors.fill: parent
        visible: gameItem.viewIndex === 2 && gameItem.worldReady
        speed: gameItem.speed
        altitude: gameItem.altitude
        fuel: gameItem.fuel
        heading: gameItem.heading
        pitchDeg: gameItem.pitchDeg
        rollDeg: gameItem.rollDeg
        boosting: gameItem.boosting
        inverted: gameItem.inverted
        verticalSpeed: gameItem.verticalSpeed
        agl: gameItem.agl
        gLoad: gameItem.gLoad
        aam: gameItem.aam
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        visible: gameItem.worldReady && gameItem.viewIndex !== 2
        width: hint.implicitWidth + 28
        height: hint.implicitHeight + 14
        color: "#1b1326"
        opacity: 0.82

        Text {
            id: hint
            anchors.centerIn: parent
            text: "MOUSE STEER  ·  HOLD LEFT AFTERBURNER  ·  SPACE TACTIC  ·  RIGHT CLICK MISSILE  ·  T VIEW"
            color: "#fff3dc"
            font.family: "Consolas"
            font.pixelSize: 13
            opacity: 0.9
        }
    }
}
