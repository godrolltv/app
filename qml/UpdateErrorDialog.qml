import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: dialog
    width: parent ? Math.min(parent.width * 0.8, 480) : 480
    height: content.implicitHeight + 48
    radius: 14
    color: "#1a1a1a"
    visible: false
    opacity: 0
    scale: 0.97
    property string message: ""
    property string mainFont: "Space Grotesk"
    readonly property bool checking: updateChecker.checking
    signal retryRequested()
    signal rejected()

    function focusPrimaryAction() {
        if (checking)
            closeButton.forceActiveFocus()
        else
            retryButton.forceActiveFocus()
    }

    onCheckingChanged: {
        if (visible)
            focusPrimaryAction()
    }

    function show(error) {
        message = error
        closeAnimation.stop()
        visible = true
        openAnimation.restart()
        focusPrimaryAction()
    }

    function hide() {
        if (!visible)
            return
        openAnimation.stop()
        closeAnimation.restart()
    }

    function dismiss() {
        hide()
        rejected()
    }

    Keys.onEscapePressed: function(event) {
        dismiss()
        event.accepted = true
    }

    ParallelAnimation {
        id: openAnimation
        NumberAnimation { target: dialog; property: "opacity"; to: 1; duration: 150; easing.type: Easing.OutCubic }
        NumberAnimation { target: dialog; property: "scale"; to: 1; duration: 180; easing.type: Easing.OutBack }
    }
    ParallelAnimation {
        id: closeAnimation
        NumberAnimation { target: dialog; property: "opacity"; to: 0; duration: 110; easing.type: Easing.InCubic }
        NumberAnimation { target: dialog; property: "scale"; to: 0.98; duration: 110; easing.type: Easing.InCubic }
        onFinished: dialog.visible = false
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Image {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                source: "qrc:/qt/qml/GodrollLauncher/resources/logo.svg"
                sourceSize: Qt.size(40, 40)
                fillMode: Image.PreserveAspectFit
            }
            Text {
                Layout.fillWidth: true
                text: dialog.checking ? "Checking for Updates" : "Couldn't Check for Updates"
                font.family: dialog.mainFont
                font.pixelSize: 20
                font.weight: Font.Bold
                color: "white"
                wrapMode: Text.WordWrap
            }
        }

        Text {
            Layout.fillWidth: true
            text: dialog.checking ? "Contacting the update service..." : dialog.message
            textFormat: Text.PlainText
            font.family: dialog.mainFont
            font.pixelSize: 14
            color: "#bbbbbb"
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 12
            Item { Layout.fillWidth: true }
            Button {
                id: closeButton
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                text: "Close"
                onClicked: dialog.dismiss()
                contentItem: Text {
                    text: closeButton.text
                    font.family: dialog.mainFont
                    font.pixelSize: 14
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: closeButton.hovered ? "#333333" : "#252525"
                    border.color: closeButton.activeFocus ? "#09d7d0" : "#555555"
                    radius: 8
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
            Button {
                id: retryButton
                Layout.preferredWidth: 140
                Layout.preferredHeight: 40
                enabled: !dialog.checking
                text: dialog.checking ? "Checking..." : "Try Again"
                onClicked: dialog.retryRequested()
                contentItem: Text {
                    text: retryButton.text
                    font.family: dialog.mainFont
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: "#1a1a1a"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: !retryButton.enabled ? "#777777" : retryButton.down ? "#08aaa5"
                        : retryButton.hovered ? "#0bc5bf" : "#09d7d0"
                    border.width: retryButton.activeFocus ? 2 : 0
                    border.color: "white"
                    radius: 8
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                HoverHandler { cursorShape: retryButton.enabled ? Qt.PointingHandCursor : Qt.BusyCursor }
            }
        }
    }
}
