import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

Kirigami.ApplicationWindow {
    id: root

    width: 1080
    height: 720
    minimumWidth: 820
    minimumHeight: 560
    title: i18n("KWarden")
    visible: true

    property string searchText: ""
    property int selectedIndex: 0
    property string actionStatusText: ""
    property bool attemptedInitialUnlockPrompt: false

    readonly property var allItems: vaultProvider.items
    readonly property var filteredItems: allItems.filter(function(item) {
        const query = root.searchText.trim().toLowerCase()
        if (query.length === 0) {
            return true
        }
        return [
            item.name,
            loginValue(item, "username"),
            (item.login && item.login.uris && item.login.uris.length > 0) ? item.login.uris[0].uri : "",
            item.notes
        ].join(" ").toLowerCase().indexOf(query) !== -1
    })
    readonly property var selectedItem: filteredItems.length === 0 ? null : filteredItems[Math.min(selectedIndex, filteredItems.length - 1)]

    function loginValue(item, fieldName) {
        if (!item || !item.login) {
            return ""
        }
        return item.login[fieldName] || ""
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

    function openInitialUnlockPrompt() {
        if (root.attemptedInitialUnlockPrompt || vaultProvider.busy) {
            return
        }

        if (vaultProvider.state === "pinLocked") {
            root.attemptedInitialUnlockPrompt = true
            pinUnlockDialog.open()
        } else if (vaultProvider.state === "locked") {
            root.attemptedInitialUnlockPrompt = true
            unlockDialog.open()
        }
    }

    function focusSearchIfUnlocked() {
        if (vaultProvider.state === "unlocked" && !vaultProvider.busy) {
            searchField.forceActiveFocus()
        }
    }

    function moveItemSelection(delta) {
        if (itemList.count === 0) {
            return
        }

        root.selectedIndex = Math.max(0, Math.min(itemList.count - 1, root.selectedIndex + delta))
        itemList.positionViewAtIndex(root.selectedIndex, ListView.Contain)
    }

    Connections {
        target: vaultProvider

        function onBusyChanged() {
            if (!vaultProvider.busy) {
                root.openInitialUnlockPrompt()
                root.focusSearchIfUnlocked()
            }
        }

        function onStateChanged() {
            if (vaultProvider.state === "locked" || vaultProvider.state === "pinLocked") {
                Qt.callLater(root.openInitialUnlockPrompt)
            } else if (vaultProvider.state === "unlocked") {
                Qt.callLater(root.focusSearchIfUnlocked)
            }
        }
    }

    Connections {
        target: keyboardShortcuts

        function onFindRequested() {
            if (!unlockDialog.opened && !pinUnlockDialog.opened && !setPinDialog.opened) {
                searchField.forceActiveFocus()
            }
        }
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

    Shortcut {
        sequence: "Ctrl+F"
        context: Qt.ApplicationShortcut
        onActivated: searchField.forceActiveFocus()
    }

    pageStack.initialPage: Kirigami.Page {
        id: mainPage

        padding: 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.ToolBar

        titleDelegate: RowLayout {
            spacing: Kirigami.Units.largeSpacing
            Layout.fillWidth: true

            Kirigami.Heading {
                text: i18n("KWarden")
                level: 2
                Layout.alignment: Qt.AlignVCenter
            }

            Kirigami.Separator {
                Layout.fillHeight: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing
                visible: vaultProvider.userEmail.length > 0
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: vaultProvider.userEmail.length > 0 ? vaultProvider.userEmail : i18n("Bitwarden CLI vault")
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }

        actions: [
            Kirigami.Action {
                text: i18n("Refresh")
                icon.name: "view-refresh-symbolic"
                enabled: !vaultProvider.busy && vaultProvider.state !== "pinLocked"
                onTriggered: root.refreshVault()
                displayHint: Kirigami.DisplayHint.KeepVisible
            },
            Kirigami.Action {
                text: (vaultProvider.pinSet || vaultProvider.pinSourceItemId.length > 0) ? i18n("Clear PIN") : i18n("Set PIN")
                icon.name: (vaultProvider.pinSet || vaultProvider.pinSourceItemId.length > 0) ? "edit-clear-symbolic" : "lock-symbolic"
                enabled: !vaultProvider.busy && vaultProvider.state === "unlocked"
                onTriggered: (vaultProvider.pinSet || vaultProvider.pinSourceItemId.length > 0) ? vaultProvider.clearPin() : setPinDialog.open()
                displayHint: Kirigami.DisplayHint.KeepVisible
            },
            Kirigami.Action {
                text: i18n("Lock")
                icon.name: "system-lock-screen-symbolic"
                enabled: !vaultProvider.busy && vaultProvider.state === "unlocked"
                onTriggered: vaultProvider.lock()
                displayHint: Kirigami.DisplayHint.KeepVisible
            }
        ]

        QQC2.SplitView {
            anchors.fill: parent
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 1
                color: Kirigami.Theme.disabledTextColor
                opacity: 0.4
            }

            QQC2.Pane {
                id: sidebar
                QQC2.SplitView.preferredWidth: Kirigami.Units.gridUnit * 18
                QQC2.SplitView.minimumWidth: Kirigami.Units.gridUnit * 14
                QQC2.SplitView.maximumWidth: Kirigami.Units.gridUnit * 28
                padding: 0

                Kirigami.Theme.colorSet: Kirigami.Theme.Window
                Kirigami.Theme.inherit: false

                background: Rectangle {
                    color: Kirigami.Theme.backgroundColor
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Kirigami.SearchField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.margins: Kirigami.Units.largeSpacing
                        placeholderText: i18n("Search vault…")
                        text: root.searchText
                        onTextChanged: {
                            root.searchText = text
                            root.selectedIndex = 0
                        }
                        Keys.onTabPressed: function(event) {
                            itemList.forceActiveFocus()
                            event.accepted = true
                        }
                        Keys.onUpPressed: function(event) {
                            root.moveItemSelection(-1)
                            event.accepted = true
                        }
                        Keys.onDownPressed: function(event) {
                            root.moveItemSelection(1)
                            event.accepted = true
                        }
                        focusSequence: "Ctrl+F"
                    }

                    Kirigami.ListSectionHeader {
                        Layout.fillWidth: true
                        text: i18np("%1 item", "%1 items", root.filteredItems.length)
                    }

                    QQC2.ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ListView {
                            id: itemList
                            model: root.filteredItems
                            currentIndex: Math.min(root.selectedIndex, Math.max(0, count - 1))
                            boundsBehavior: Flickable.StopAtBounds
                            spacing: 0
                            keyNavigationEnabled: true
                            activeFocusOnTab: true
                            highlightFollowsCurrentItem: true
                            highlightMoveDuration: Kirigami.Units.shortDuration

                            onCurrentIndexChanged: {
                                if (activeFocus && currentIndex >= 0) {
                                    root.selectedIndex = currentIndex
                                }
                            }

                            delegate: VaultListItemDelegate {
                                required property var modelData
                                required property int index

                                item: modelData
                                isCurrent: ListView.isCurrentItem
                                onClicked: root.selectedIndex = index
                                onUsePasswordAsPinRequested: function(item) {
                                    root.selectedIndex = index
                                    root.actionStatusText = ""
                                    vaultProvider.useItemPasswordAsPin(item.id)
                                }
                            }

                            Kirigami.PlaceholderMessage {
                                anchors.centerIn: parent
                                width: parent.width - Kirigami.Units.gridUnit * 2
                                visible: itemList.count === 0 && vaultProvider.state === "unlocked"
                                icon.name: "edit-none-symbolic"
                                text: root.searchText.length > 0 ? i18n("No matching items") : i18n("No vault items")
                            }

                            Kirigami.PlaceholderMessage {
                                anchors.centerIn: parent
                                width: parent.width - Kirigami.Units.gridUnit * 2
                                visible: itemList.count === 0 && vaultProvider.state !== "unlocked"
                                icon.name: vaultProvider.state === "missing" ? "dialog-error-symbolic" : "system-lock-screen-symbolic"
                                text: vaultProvider.statusText
                            }
                        }
                    }
                }
            }

            QQC2.ScrollView {
                QQC2.SplitView.fillWidth: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: Kirigami.Units.largeSpacing

                    // Detail header — banner-like
                    Item {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.gridUnit
                        Layout.leftMargin: Kirigami.Units.gridUnit
                        Layout.rightMargin: Kirigami.Units.gridUnit
                        Layout.bottomMargin: 0
                        visible: root.selectedItem !== null
                        implicitHeight: headerRow.implicitHeight

                        RowLayout {
                            id: headerRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: Kirigami.Units.largeSpacing

                            Rectangle {
                                Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                                Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                                radius: Kirigami.Units.cornerRadius
                                color: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                               Kirigami.Theme.highlightColor.g,
                                               Kirigami.Theme.highlightColor.b,
                                               0.18)

                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: Kirigami.Units.iconSizes.large
                                    height: Kirigami.Units.iconSizes.large
                                    source: {
                                        if (!root.selectedItem) return ""
                                        switch (root.selectedItem.type) {
                                        case 1: return "internet-services-symbolic"
                                        case 2: return "view-pim-notes-symbolic"
                                        case 3: return "credit-card-symbolic"
                                        case 4: return "user-identity-symbolic"
                                        default: return "dialog-password-symbolic"
                                        }
                                    }
                                    color: Kirigami.Theme.highlightColor
                                    isMask: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing / 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    Kirigami.Heading {
                                        Layout.fillWidth: true
                                        text: root.selectedItem ? root.selectedItem.name : ""
                                        level: 1
                                        wrapMode: Text.Wrap
                                        elide: Text.ElideRight
                                        maximumLineCount: 2
                                    }

                                    Kirigami.Icon {
                                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                        source: "starred-symbolic"
                                        visible: root.selectedItem && root.selectedItem.favorite
                                        color: Kirigami.Theme.neutralTextColor
                                        isMask: true

                                        QQC2.ToolTip.visible: hovered
                                        QQC2.ToolTip.text: i18n("Favorite")

                                        HoverHandler {
                                            id: hoverHandler
                                        }
                                        property bool hovered: hoverHandler.hovered
                                    }
                                }

                                QQC2.Label {
                                    Layout.fillWidth: true
                                    text: {
                                        if (!root.selectedItem) return ""
                                        const it = root.selectedItem
                                        if (it.login && it.login.uris && it.login.uris.length > 0 && it.login.uris[0].uri) {
                                            return it.login.uris[0].uri
                                        }
                                        switch (it.type) {
                                        case 1: return i18n("Login")
                                        case 2: return i18n("Secure Note")
                                        case 3: return i18n("Card")
                                        case 4: return i18n("Identity")
                                        default: return ""
                                        }
                                    }
                                    color: Kirigami.Theme.disabledTextColor
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                            }
                        }
                    }

                    // Credentials card
                    FormCard.FormHeader {
                        Layout.fillWidth: true
                        title: i18n("Credentials")
                        visible: root.selectedItem !== null && root.selectedItem.type === 1
                    }

                    FormCard.FormCard {
                        Layout.fillWidth: true
                        visible: root.selectedItem !== null && root.selectedItem.type === 1

                        VaultFieldDelegate {
                            label: i18n("Username")
                            value: root.selectedItem ? root.loginValue(root.selectedItem, "username") : ""
                            iconName: "user-symbolic"
                            onCopyRequested: (l, v) => root.copyValue(l, v)
                        }

                        FormCard.FormDelegateSeparator {}

                        VaultFieldDelegate {
                            label: i18n("Password")
                            value: root.selectedItem ? root.loginValue(root.selectedItem, "password") : ""
                            iconName: "password-show-on-symbolic"
                            sensitive: true
                            monospace: true
                            onCopyRequested: (l, v) => root.copyValue(l, v)
                        }

                        FormCard.FormDelegateSeparator {
                            visible: !!(root.selectedItem && root.loginValue(root.selectedItem, "totp").length > 0)
                        }

                        VaultFieldDelegate {
                            label: i18n("TOTP")
                            value: root.selectedItem ? root.loginValue(root.selectedItem, "totp") : ""
                            iconName: "chronometer-symbolic"
                            monospace: true
                            visible: !!(root.selectedItem && root.loginValue(root.selectedItem, "totp").length > 0)
                            copyValue: root.selectedItem ? root.totpClipboardValue(root.loginValue(root.selectedItem, "totp")) : ""
                            onCopyRequested: (l, v) => root.copyValue(l, v)
                        }
                    }

                    // URIs card
                    FormCard.FormHeader {
                        Layout.fillWidth: true
                        title: i18np("Website", "Websites", uriRepeater.count)
                        visible: uriRepeater.count > 0
                    }

                    FormCard.FormCard {
                        Layout.fillWidth: true
                        visible: uriRepeater.count > 0

                        Repeater {
                            id: uriRepeater
                            model: root.selectedItem && root.selectedItem.login && root.selectedItem.login.uris
                                   ? root.selectedItem.login.uris : []

                            VaultFieldDelegate {
                                required property var modelData
                                required property int index

                                label: i18nc("Website URL label", "URL %1", index + 1)
                                value: modelData.uri || ""
                                iconName: "globe-symbolic"
                                onCopyRequested: (l, v) => root.copyValue(l, v)
                            }
                        }
                    }

                    // Custom fields card
                    FormCard.FormHeader {
                        Layout.fillWidth: true
                        title: i18n("Custom Fields")
                        visible: customFieldsRepeater.count > 0
                    }

                    FormCard.FormCard {
                        Layout.fillWidth: true
                        visible: customFieldsRepeater.count > 0

                        Repeater {
                            id: customFieldsRepeater
                            model: root.selectedItem && root.selectedItem.fields ? root.selectedItem.fields : []

                            VaultFieldDelegate {
                                required property var modelData
                                required property int index

                                label: modelData.name || i18n("Field %1", index + 1)
                                value: modelData.value || ""
                                sensitive: modelData.type === 1
                                monospace: modelData.type === 1
                                iconName: modelData.type === 1 ? "password-show-on-symbolic" : "edit-symbolic"
                                onCopyRequested: (l, v) => root.copyValue(l, v)
                            }
                        }
                    }

                    // Notes card
                    FormCard.FormHeader {
                        Layout.fillWidth: true
                        title: i18n("Notes")
                        visible: !!(root.selectedItem && root.selectedItem.notes && root.selectedItem.notes.length > 0)
                    }

                    FormCard.FormCard {
                        Layout.fillWidth: true
                        visible: !!(root.selectedItem && root.selectedItem.notes && root.selectedItem.notes.length > 0)

                        FormCard.AbstractFormDelegate {
                            Layout.fillWidth: true
                            background: null
                            focusPolicy: Qt.NoFocus

                            contentItem: QQC2.TextArea {
                                text: root.selectedItem && root.selectedItem.notes ? root.selectedItem.notes : ""
                                readOnly: true
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                background: null
                            }
                        }
                    }

                    // Placeholder when nothing selected
                    Kirigami.PlaceholderMessage {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignCenter
                        Layout.topMargin: Kirigami.Units.gridUnit * 4
                        visible: root.selectedItem === null
                        icon.name: {
                            switch (vaultProvider.state) {
                            case "locked": return "system-lock-screen-symbolic"
                            case "pinLocked": return "system-lock-screen-symbolic"
                            case "unauthenticated": return "dialog-warning-symbolic"
                            case "missing": return "dialog-error-symbolic"
                            case "error": return "dialog-error-symbolic"
                            case "unlocked": return "edit-none-symbolic"
                            default: return "view-refresh-symbolic"
                            }
                        }
                        text: vaultProvider.state === "unlocked" ? i18n("Select a vault item") : vaultProvider.statusText
                        explanation: vaultProvider.state === "unlocked"
                                     ? i18n("Use the list on the left to view and copy credentials.")
                                     : ""

                        helpfulAction: Kirigami.Action {
                            text: vaultProvider.state === "pinLocked" ? i18n("Unlock with PIN")
                                  : (vaultProvider.state === "locked" ? i18n("Unlock vault")
                                  : i18n("Refresh"))
                            icon.name: vaultProvider.state === "pinLocked" || vaultProvider.state === "locked"
                                       ? "object-unlocked-symbolic" : "view-refresh-symbolic"
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
                        Layout.preferredHeight: Kirigami.Units.gridUnit
                    }
                }
            }
        }

        footer: QQC2.ToolBar {
            position: QQC2.ToolBar.Footer

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.largeSpacing
                anchors.rightMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    source: {
                        switch (vaultProvider.state) {
                        case "unlocked": return "object-unlocked-symbolic"
                        case "locked": return "system-lock-screen-symbolic"
                        case "pinLocked": return "system-lock-screen-symbolic"
                        case "unauthenticated": return "dialog-warning-symbolic"
                        case "missing": return "dialog-error-symbolic"
                        case "error": return "dialog-error-symbolic"
                        default: return "view-refresh-symbolic"
                        }
                    }
                    color: {
                        switch (vaultProvider.state) {
                        case "unlocked": return Kirigami.Theme.positiveTextColor
                        case "missing":
                        case "error": return Kirigami.Theme.negativeTextColor
                        case "locked":
                        case "pinLocked":
                        case "unauthenticated": return Kirigami.Theme.neutralTextColor
                        default: return Kirigami.Theme.disabledTextColor
                        }
                    }
                    isMask: true
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: root.actionStatusText.length > 0 ? root.actionStatusText : vaultProvider.statusText
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                QQC2.BusyIndicator {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    running: vaultProvider.busy
                    visible: vaultProvider.busy
                }

                QQC2.Label {
                    text: i18n("Ctrl+U  ·  Ctrl+P  ·  Ctrl+T")
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                    textFormat: Text.PlainText
                    visible: vaultProvider.state === "unlocked"
                }
            }
        }
    }

    // Dialogs ---------------------------------------------------------------

    Kirigami.PromptDialog {
        id: unlockDialog
        title: i18n("Unlock Bitwarden Vault")
        subtitle: i18n("Enter your Bitwarden master password. The resulting session key is held in memory only; if PIN unlock is enabled, Lock keeps only an in-memory PIN-wrapped copy.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        showCloseButton: false
        preferredWidth: Kirigami.Units.gridUnit * 26

        onOpened: masterPasswordField.forceActiveFocus()
        onAccepted: {
            vaultProvider.unlock(masterPasswordField.text)
            masterPasswordField.text = ""
            root.actionStatusText = ""
        }
        onRejected: masterPasswordField.text = ""

        QQC2.TextField {
            id: masterPasswordField
            Layout.fillWidth: true
            placeholderText: i18n("Master password")
            echoMode: TextInput.Password
            enabled: !vaultProvider.busy
            onAccepted: unlockDialog.accept()
        }
    }

    Kirigami.PromptDialog {
        id: pinUnlockDialog
        title: i18n("Unlock with PIN")
        subtitle: i18n("Unlocks KWarden using the in-memory PIN-wrapped BW_SESSION. Available only until KWarden quits.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        showCloseButton: false
        preferredWidth: Kirigami.Units.gridUnit * 22

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Use master password")
                icon.name: "dialog-password-symbolic"
                enabled: !vaultProvider.busy
                onTriggered: {
                    pinUnlockDialog.close()
                    pinUnlockField.text = ""
                    unlockDialog.open()
                }
            }
        ]

        onOpened: pinUnlockField.forceActiveFocus()
        onAccepted: {
            vaultProvider.unlockWithPin(pinUnlockField.text)
            pinUnlockField.text = ""
            root.actionStatusText = ""
        }
        onRejected: pinUnlockField.text = ""

        QQC2.TextField {
            id: pinUnlockField
            Layout.fillWidth: true
            placeholderText: i18n("PIN")
            echoMode: TextInput.Password
            enabled: !vaultProvider.busy
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
            onAccepted: pinUnlockDialog.accept()
        }
    }

    Kirigami.PromptDialog {
        id: setPinDialog
        title: i18n("Set PIN")
        subtitle: i18n("The PIN is ephemeral: nothing is written to disk, and PIN unlock disappears when KWarden quits. Lock keeps only a PIN-wrapped BW_SESSION in memory.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        showCloseButton: false
        preferredWidth: Kirigami.Units.gridUnit * 24

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
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: newPinField
                Layout.fillWidth: true
                placeholderText: i18n("PIN")
                echoMode: TextInput.Password
                enabled: !vaultProvider.busy
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onAccepted: setPinDialog.accept()
            }

            QQC2.TextField {
                id: confirmPinField
                Layout.fillWidth: true
                placeholderText: i18n("Confirm PIN")
                echoMode: TextInput.Password
                enabled: !vaultProvider.busy
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onAccepted: setPinDialog.accept()
            }
        }
    }
}
