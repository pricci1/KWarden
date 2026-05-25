import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    width: 980
    height: 640
    minimumWidth: 760
    minimumHeight: 520
    title: i18n("KWarden")
    visible: true

    property string searchText: ""
    property int selectedIndex: 0
    property string actionStatusText: ""
    readonly property color selectionColor: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                                   Kirigami.Theme.highlightColor.g,
                                                   Kirigami.Theme.highlightColor.b,
                                                   0.28)
    readonly property color hoverColor: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                               Kirigami.Theme.highlightColor.g,
                                               Kirigami.Theme.highlightColor.b,
                                               0.12)

    readonly property var allItems: vaultProvider.items
    readonly property var filteredItems: allItems.filter(function(item) {
        var query = root.searchText.trim().toLowerCase()
        if (query.length === 0) {
            return true
        }

        return [item.name, loginValue(item, "username"), item.notes].join(" ").toLowerCase().indexOf(query) !== -1
    })
    readonly property var selectedItem: filteredItems.length === 0 ? null : filteredItems[Math.min(selectedIndex, filteredItems.length - 1)]

    function loginValue(item, fieldName) {
        if (!item || !item.login) {
            return ""
        }

        return item.login[fieldName] || ""
    }

    function masked(value) {
        if (!value || value.length === 0) {
            return "—"
        }

        return "•".repeat(value.length)
    }

    function totpClipboardValue(value) {
        return String(value || "").split(" ").join("")
    }

    function copyValue(label, value) {
        if (!root.selectedItem) {
            root.actionStatusText = i18n("No vault item selected")
            return
        }

        if (!value || value.length === 0) {
            root.actionStatusText = i18n("Selected item has no %1", label)
            return
        }

        clipboardBridge.copy(value)
        root.actionStatusText = i18n("Copied %1 for %2", label, root.selectedItem.name)
    }

    function copyField(fieldName) {
        if (!root.selectedItem) {
            root.actionStatusText = i18n("No vault item selected")
            return
        }

        if (fieldName === "username") {
            copyValue(i18n("username"), root.loginValue(root.selectedItem, "username"))
        } else if (fieldName === "password") {
            copyValue(i18n("password"), root.loginValue(root.selectedItem, "password"))
        } else if (fieldName === "totp") {
            copyValue(i18n("TOTP"), totpClipboardValue(root.loginValue(root.selectedItem, "totp")))
        }
    }

    function refreshVault() {
        root.actionStatusText = ""
        vaultProvider.refresh()
    }

    Shortcut {
        sequence: "Ctrl+U"
        onActivated: root.copyField("username")
    }

    Shortcut {
        sequence: "Ctrl+P"
        onActivated: root.copyField("password")
    }

    Shortcut {
        sequence: "Ctrl+T"
        onActivated: root.copyField("totp")
    }

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    header: Controls.ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.largeSpacing
            anchors.rightMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Heading {
                text: i18n("KWarden")
                level: 2
            }

            Controls.Label {
                text: vaultProvider.userEmail.length > 0 ? vaultProvider.userEmail : i18n("Bitwarden CLI vault")
                color: Kirigami.Theme.disabledTextColor
                font.weight: Font.DemiBold
            }

            Item {
                Layout.fillWidth: true
            }

            Controls.Label {
                text: i18n("Ctrl+U Username   Ctrl+P Password   Ctrl+T TOTP")
                color: Kirigami.Theme.disabledTextColor
                font.weight: Font.DemiBold
            }

            Controls.Button {
                text: i18n("Refresh")
                enabled: !vaultProvider.busy && vaultProvider.state !== "pinLocked"
                onClicked: root.refreshVault()
            }

            Controls.Button {
                text: vaultProvider.pinSet ? i18n("Clear PIN") : i18n("Set PIN")
                enabled: !vaultProvider.busy && vaultProvider.state === "unlocked"
                onClicked: vaultProvider.pinSet ? vaultProvider.clearPin() : setPinDialog.open()
            }

            Controls.Button {
                text: i18n("Lock")
                enabled: !vaultProvider.busy && vaultProvider.state === "unlocked"
                onClicked: vaultProvider.lock()
            }
        }
    }

    footer: Controls.Label {
        text: root.actionStatusText.length > 0 ? root.actionStatusText : vaultProvider.statusText
        leftPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing
        topPadding: Kirigami.Units.smallSpacing / 2
        bottomPadding: Kirigami.Units.smallSpacing / 2
        elide: Text.ElideRight
    }

    Controls.SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Rectangle {
            Controls.SplitView.preferredWidth: 300
            Controls.SplitView.minimumWidth: 240
            color: Kirigami.Theme.backgroundColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                Controls.TextField {
                    Layout.fillWidth: true
                    placeholderText: i18n("Search vault…")
                    text: root.searchText
                    onTextChanged: {
                        root.searchText = text
                        root.selectedIndex = 0
                    }
                }

                Controls.Label {
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    text: i18n("Vault Items")
                    color: Kirigami.Theme.disabledTextColor
                    font.weight: Font.DemiBold
                }

                ListView {
                    id: itemList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.filteredItems
                    currentIndex: Math.min(root.selectedIndex, Math.max(0, count - 1))
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: Kirigami.Units.smallSpacing / 2

                    Controls.ScrollBar.vertical: Controls.ScrollBar {
                        policy: Controls.ScrollBar.AsNeeded
                    }

                    delegate: Controls.ItemDelegate {
                        required property var modelData
                        required property int index

                        width: ListView.view.width
                        height: Kirigami.Units.gridUnit * 2
                        leftPadding: Kirigami.Units.smallSpacing
                        rightPadding: Kirigami.Units.smallSpacing
                        topPadding: 0
                        bottomPadding: 0
                        highlighted: ListView.isCurrentItem
                        onClicked: root.selectedIndex = index

                        background: Rectangle {
                            radius: Kirigami.Units.cornerRadius
                            color: parent.highlighted ? root.selectionColor : (parent.hovered ? root.hoverColor : "transparent")
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                                source: "dialog-password"
                                opacity: 0.8
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Controls.Label {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    elide: Text.ElideRight
                                }

                                Controls.Label {
                                    Layout.fillWidth: true
                                    text: root.loginValue(modelData, "username")
                                    color: Kirigami.Theme.disabledTextColor
                                    elide: Text.ElideRight
                                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                                }
                            }

                            Controls.Label {
                                text: "★"
                                visible: modelData.favorite
                                color: Kirigami.Theme.neutralTextColor
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        visible: itemList.count === 0
                        text: vaultProvider.state === "unlocked" ? i18n("No matching vault items") : vaultProvider.statusText
                    }
                }
            }
        }

        Kirigami.Separator {}

        Kirigami.ScrollablePage {
            Controls.SplitView.fillWidth: true
            padding: Kirigami.Units.gridUnit
            background: Rectangle {
                color: Kirigami.Theme.backgroundColor
            }

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true

                    Kirigami.Heading {
                        Layout.fillWidth: true
                        text: root.selectedItem ? root.selectedItem.name : i18n("No vault item selected")
                        level: 2
                        wrapMode: Text.Wrap
                    }

                    Controls.Label {
                        text: i18n("Favorite")
                        visible: root.selectedItem && root.selectedItem.favorite
                        color: Kirigami.Theme.neutralTextColor
                        font.weight: Font.DemiBold
                    }
                }

                Controls.Label {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    text: i18n("Use the buttons or keyboard shortcuts to copy fields from this item.")
                    color: Kirigami.Theme.disabledTextColor
                    font.weight: Font.DemiBold
                    visible: root.selectedItem !== null
                    wrapMode: Text.Wrap
                }

                FieldCard {
                    title: i18n("Username")
                    value: root.selectedItem ? root.loginValue(root.selectedItem, "username") : ""
                    copyValue: value
                    visible: root.selectedItem !== null
                }

                FieldCard {
                    title: i18n("Password")
                    value: root.selectedItem ? masked(root.loginValue(root.selectedItem, "password")) : ""
                    copyValue: root.selectedItem ? root.loginValue(root.selectedItem, "password") : ""
                    visible: root.selectedItem !== null
                }

                FieldCard {
                    title: i18n("TOTP")
                    value: root.selectedItem ? (root.loginValue(root.selectedItem, "totp") || "—") : ""
                    copyValue: root.selectedItem ? totpClipboardValue(root.loginValue(root.selectedItem, "totp")) : ""
                    visible: root.selectedItem !== null
                }

                Repeater {
                    model: root.selectedItem && root.selectedItem.fields ? root.selectedItem.fields : []

                    FieldCard {
                        required property var modelData

                        title: modelData.name
                        value: modelData.type === 1 ? masked(modelData.value) : modelData.value
                        copyValue: modelData.value
                    }
                }

                Kirigami.AbstractCard {
                    Layout.fillWidth: true
                    visible: root.selectedItem !== null

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Label {
                            text: i18n("Notes")
                            font.weight: Font.Bold
                        }

                        Controls.Label {
                            Layout.fillWidth: true
                            text: root.selectedItem && root.selectedItem.notes && root.selectedItem.notes.length > 0 ? root.selectedItem.notes : "—"
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    Layout.fillWidth: true
                    visible: root.selectedItem === null
                    text: vaultProvider.statusText
                    helpfulAction: Kirigami.Action {
                        text: vaultProvider.state === "pinLocked" ? i18n("Unlock with PIN") : (vaultProvider.state === "locked" ? i18n("Unlock") : i18n("Refresh"))
                        enabled: !vaultProvider.busy && vaultProvider.state !== "unlocked"
                        onTriggered: {
                            if (vaultProvider.state === "pinLocked") {
                                pinUnlockDialog.open()
                            } else if (vaultProvider.state === "locked") {
                                unlockDialog.open()
                            } else {
                                root.refreshVault()
                            }
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    component FieldCard: Rectangle {
        property string title
        property string value
        property string copyValue

        Layout.fillWidth: true
        implicitHeight: fieldRow.implicitHeight + Kirigami.Units.largeSpacing
        color: "transparent"

        RowLayout {
            id: fieldRow
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing
            anchors.rightMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    text: title
                    font.weight: Font.Bold
                }

                Controls.Label {
                    Layout.fillWidth: true
                    text: value && value.length > 0 ? value : "—"
                    elide: Text.ElideRight
                }
            }

            Controls.Button {
                text: i18n("Copy")
                enabled: copyValue.length > 0
                onClicked: root.copyValue(title, copyValue)
            }
        }

        Kirigami.Separator {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    Controls.Dialog {
        id: unlockDialog
        title: i18n("Unlock Bitwarden Vault")
        modal: true
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        closePolicy: Controls.Popup.CloseOnEscape
        anchors.centerIn: parent

        onOpened: masterPasswordField.forceActiveFocus()
        onAccepted: {
            vaultProvider.unlock(masterPasswordField.text)
            masterPasswordField.text = ""
            root.actionStatusText = ""
        }
        onRejected: masterPasswordField.text = ""

        ColumnLayout {
            width: Math.min(root.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 24)
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                Layout.fillWidth: true
                text: i18n("KWarden sends this password to ‘bw unlock --raw’ over stdin. The resulting session key is kept in memory only. If PIN unlock is enabled, Lock keeps only an in-memory PIN-wrapped copy of that session key.")
                wrapMode: Text.Wrap
            }

            Controls.TextField {
                id: masterPasswordField
                Layout.fillWidth: true
                placeholderText: i18n("Master password")
                echoMode: TextInput.Password
                enabled: !vaultProvider.busy
                onAccepted: unlockDialog.accept()
            }
        }
    }

    Controls.Dialog {
        id: pinUnlockDialog
        title: i18n("Unlock with PIN")
        modal: true
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        closePolicy: Controls.Popup.CloseOnEscape
        anchors.centerIn: parent

        onOpened: pinUnlockField.forceActiveFocus()
        onAccepted: {
            vaultProvider.unlockWithPin(pinUnlockField.text)
            pinUnlockField.text = ""
            root.actionStatusText = ""
        }
        onRejected: pinUnlockField.text = ""

        ColumnLayout {
            width: Math.min(root.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 20)
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                Layout.fillWidth: true
                text: i18n("This unlocks KWarden using the in-memory PIN-wrapped BW_SESSION. It is available only until KWarden quits.")
                wrapMode: Text.Wrap
            }

            Controls.TextField {
                id: pinUnlockField
                Layout.fillWidth: true
                placeholderText: i18n("PIN")
                echoMode: TextInput.Password
                enabled: !vaultProvider.busy
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onAccepted: pinUnlockDialog.accept()
            }

            Controls.Button {
                text: i18n("Use master password instead")
                enabled: !vaultProvider.busy
                onClicked: {
                    pinUnlockDialog.close()
                    pinUnlockField.text = ""
                    unlockDialog.open()
                }
            }
        }
    }

    Controls.Dialog {
        id: setPinDialog
        title: i18n("Set PIN")
        modal: true
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        closePolicy: Controls.Popup.CloseOnEscape
        anchors.centerIn: parent

        onOpened: newPinField.forceActiveFocus()
        onAccepted: {
            if (newPinField.text.length < 4 || newPinField.text !== confirmPinField.text) {
                root.actionStatusText = i18n("PINs must match and be at least 4 characters")
                newPinField.text = ""
                confirmPinField.text = ""
            } else {
                vaultProvider.setPin(newPinField.text)
                newPinField.text = ""
                confirmPinField.text = ""
                root.actionStatusText = ""
            }
        }
        onRejected: {
            newPinField.text = ""
            confirmPinField.text = ""
        }

        ColumnLayout {
            width: Math.min(root.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 22)
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                Layout.fillWidth: true
                text: i18n("The PIN is ephemeral: nothing is written to disk, and PIN unlock disappears when KWarden quits. Lock will keep only a PIN-wrapped BW_SESSION in memory.")
                wrapMode: Text.Wrap
            }

            Controls.TextField {
                id: newPinField
                Layout.fillWidth: true
                placeholderText: i18n("PIN")
                echoMode: TextInput.Password
                enabled: !vaultProvider.busy
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onTextChanged: setPinDialog.standardButton(Controls.Dialog.Ok).enabled = text.length >= 4 && text === confirmPinField.text
                onAccepted: setPinDialog.accept()
            }

            Controls.TextField {
                id: confirmPinField
                Layout.fillWidth: true
                placeholderText: i18n("Confirm PIN")
                echoMode: TextInput.Password
                enabled: !vaultProvider.busy
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onTextChanged: setPinDialog.standardButton(Controls.Dialog.Ok).enabled = text.length >= 4 && text === newPinField.text
                onAccepted: setPinDialog.accept()
            }
        }
    }
}
