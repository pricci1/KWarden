import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.AbstractFormDelegate {
    id: root

    property string label
    property string value
    property string copyValue: value
    property bool sensitive: false
    property bool monospace: false
    property string iconName: ""
    property string emptyPlaceholder: "—"
    property int wrapMode: TextEdit.NoWrap

    property bool reveal: false

    signal copyRequested(string label, string value)

    background: null
    focusPolicy: Qt.NoFocus

    function displayValue() {
        if (!value || value.length === 0) {
            return emptyPlaceholder
        }
        if (sensitive && !reveal) {
            return "•".repeat(Math.min(value.length, 16))
        }
        return value
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            visible: root.iconName.length > 0
            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            Layout.alignment: Qt.AlignVCenter
            source: root.iconName
            color: Kirigami.Theme.disabledTextColor
            isMask: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: root.label
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                textFormat: Text.PlainText
            }

            TextEdit {
                Layout.fillWidth: true
                text: root.displayValue()
                readOnly: true
                selectByMouse: true
                wrapMode: root.wrapMode
                color: Kirigami.Theme.textColor
                font.family: root.monospace ? "monospace" : Kirigami.Theme.defaultFont.family
                font.pointSize: root.monospace ? Kirigami.Theme.defaultFont.pointSize + 1 : Kirigami.Theme.defaultFont.pointSize
                font.letterSpacing: root.monospace && root.value && root.value.length > 0 ? 1 : 0
                textFormat: TextEdit.PlainText
            }
        }

        QQC2.ToolButton {
            visible: root.sensitive
            display: QQC2.AbstractButton.IconOnly
            icon.name: root.reveal ? "view-hidden-symbolic" : "view-visible-symbolic"
            text: root.reveal ? i18n("Hide") : i18n("Reveal")
            onClicked: root.reveal = !root.reveal

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            display: QQC2.AbstractButton.IconOnly
            icon.name: "edit-copy-symbolic"
            text: i18n("Copy %1", root.label)
            enabled: root.copyValue && root.copyValue.length > 0
            onClicked: root.copyRequested(root.label, root.copyValue)

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }
}
