import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    width: 1060
    height: 680
    minimumWidth: 860
    minimumHeight: 560
    title: i18n("KWarden")
    visible: true

    property string searchText: ""
    property int selectedIndex: 0
    property string statusText: i18np("Development mode: %1 mocked Bitwarden CLI item loaded", "Development mode: %1 mocked Bitwarden CLI items loaded", allItems.length)

    // These objects mirror Bitwarden CLI's `ListResponse`/`CipherResponse` shape:
    // { object: "list", data: [{ object: "item", type: 1, login: ..., fields: ... }] }.
    readonly property var bitwardenListResponse: ({
        object: "list",
        data: [
            {
                object: "item",
                id: "1",
                type: 1,
                name: "Google",
                notes: "Primary personal Google account.",
                favorite: true,
                collectionIds: [],
                fields: [
                    { name: "Recovery Code", value: "orchid-garden-42", type: 1 },
                    { name: "Account ID", value: "acct_9Yx732", type: 0 }
                ],
                login: {
                    username: "MyUsername132",
                    password: "correct horse battery staple",
                    totp: "123 456",
                    uris: [{ uri: "https://google.example" }]
                }
            },
            {
                object: "item",
                id: "2",
                type: 1,
                name: "Twitter",
                notes: "Developer account.",
                favorite: false,
                collectionIds: [],
                fields: [],
                login: {
                    username: "otto_dev",
                    password: "blue-bird-secret",
                    totp: "714 209",
                    uris: [{ uri: "https://twitter.example" }]
                }
            },
            {
                object: "item",
                id: "3",
                type: 1,
                name: "My Bank",
                notes: "Call before international travel.",
                favorite: false,
                collectionIds: [],
                fields: [{ name: "PIN", value: "9842", type: 1 }],
                login: {
                    username: "checking-7781",
                    password: "vaulted-bank-password",
                    totp: "620 118",
                    uris: [{ uri: "https://mybank.example" }]
                }
            },
            {
                object: "item",
                id: "4",
                type: 1,
                name: "OpenRouter",
                notes: "API dashboard.",
                favorite: false,
                collectionIds: [],
                fields: [{ name: "Default model", value: "openai/gpt-4.1", type: 0 }],
                login: {
                    username: "otto@example.com",
                    password: "sk-or-v1-not-a-real-key",
                    totp: "",
                    uris: [{ uri: "https://openrouter.example" }]
                }
            },
            {
                object: "item",
                id: "5",
                type: 1,
                name: "Some Site",
                notes: "Imported from browser extension.",
                favorite: false,
                collectionIds: [],
                fields: [],
                login: {
                    username: "somebody",
                    password: "generic-password",
                    totp: "832 551",
                    uris: [{ uri: "https://somesite.example" }]
                }
            }
        ]
    })
    readonly property var allItems: bitwardenListResponse.data
    readonly property var filteredItems: allItems.filter(function(item) {
        var query = root.searchText.trim().toLowerCase()
        if (query.length === 0) {
            return true
        }

        return [item.name, item.login.username, item.notes].join(" ").toLowerCase().indexOf(query) !== -1
    })
    readonly property var selectedItem: filteredItems.length === 0 ? null : filteredItems[Math.min(selectedIndex, filteredItems.length - 1)]

    function masked(value) {
        if (!value || value.length === 0) {
            return "—"
        }

        return "•".repeat(value.length)
    }

    function totpClipboardValue(value) {
        return (value || "").replaceAll(" ", "")
    }

    function copyValue(label, value) {
        if (!root.selectedItem) {
            root.statusText = i18n("No vault item selected")
            return
        }

        if (!value || value.length === 0) {
            root.statusText = i18n("Selected item has no %1", label)
            return
        }

        clipboardBridge.copy(value)
        root.statusText = i18n("Copied %1 for %2", label, root.selectedItem.name)
    }

    function copyField(fieldName) {
        if (!root.selectedItem) {
            root.statusText = i18n("No vault item selected")
            return
        }

        if (fieldName === "username") {
            copyValue(i18n("username"), root.selectedItem.login.username)
        } else if (fieldName === "password") {
            copyValue(i18n("password"), root.selectedItem.login.password)
        } else if (fieldName === "totp") {
            copyValue(i18n("TOTP"), totpClipboardValue(root.selectedItem.login.totp))
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
                text: i18n("Bitwarden CLI vault")
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
        }
    }

    footer: Controls.Label {
        text: root.statusText
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
            Controls.SplitView.preferredWidth: 320
            Controls.SplitView.minimumWidth: 260
            color: Kirigami.Theme.backgroundColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

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

                    delegate: Controls.ItemDelegate {
                        required property var modelData
                        required property int index

                        width: ListView.view.width
                        highlighted: index === root.selectedIndex
                        text: (modelData.favorite ? "★ " : "") + modelData.name
                        icon.name: "dialog-password"
                        onClicked: root.selectedIndex = index
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        visible: itemList.count === 0
                        text: i18n("No matching vault items")
                    }
                }
            }
        }

        Kirigami.Separator {}

        Kirigami.ScrollablePage {
            Controls.SplitView.fillWidth: true
            padding: Kirigami.Units.gridUnit * 1.5

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    text: root.selectedItem ? ((root.selectedItem.favorite ? "★ " : "") + root.selectedItem.name) : i18n("No vault item selected")
                    level: 1
                    wrapMode: Text.Wrap
                }

                Controls.Label {
                    Layout.fillWidth: true
                    text: i18n("Use the buttons or keyboard shortcuts to copy fields from this item.")
                    color: Kirigami.Theme.disabledTextColor
                    font.weight: Font.DemiBold
                    visible: root.selectedItem !== null
                    wrapMode: Text.Wrap
                }

                FieldCard {
                    title: i18n("Username")
                    value: root.selectedItem ? root.selectedItem.login.username : ""
                    copyValue: value
                    visible: root.selectedItem !== null
                }

                FieldCard {
                    title: i18n("Password")
                    value: root.selectedItem ? masked(root.selectedItem.login.password) : ""
                    copyValue: root.selectedItem ? root.selectedItem.login.password : ""
                    visible: root.selectedItem !== null
                }

                FieldCard {
                    title: i18n("TOTP")
                    value: root.selectedItem ? (root.selectedItem.login.totp || "—") : ""
                    copyValue: root.selectedItem ? totpClipboardValue(root.selectedItem.login.totp) : ""
                    visible: root.selectedItem !== null
                }

                Repeater {
                    model: root.selectedItem ? root.selectedItem.fields : []

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
                            text: root.selectedItem && root.selectedItem.notes.length > 0 ? root.selectedItem.notes : "—"
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    component FieldCard: Kirigami.AbstractCard {
        property string title
        property string value
        property string copyValue

        Layout.fillWidth: true

        contentItem: RowLayout {
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
    }
}
