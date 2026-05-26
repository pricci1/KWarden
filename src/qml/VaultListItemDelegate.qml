import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ItemDelegate {
    id: root

    required property var item

    property bool isCurrent: false

    signal usePasswordAsPinRequested(var item)

    function iconForType(type) {
        switch (type) {
        case 1: return "internet-services-symbolic"
        case 2: return "view-pim-notes-symbolic"
        case 3: return "credit-card-symbolic"
        case 4: return "user-identity-symbolic"
        default: return "dialog-password-symbolic"
        }
    }

    function loginSubtitle(it) {
        if (!it) {
            return ""
        }
        if (it.login && it.login.username) {
            return it.login.username
        }
        if (it.login && it.login.uris && it.login.uris.length > 0) {
            return it.login.uris[0].uri || ""
        }
        if (it.card && it.card.brand) {
            return it.card.brand + (it.card.number ? " ••••" + String(it.card.number).slice(-4) : "")
        }
        if (it.identity && it.identity.email) {
            return it.identity.email
        }
        return ""
    }

    width: ListView.view ? ListView.view.width : 0
    padding: Kirigami.Units.smallSpacing
    leftPadding: Kirigami.Units.largeSpacing
    rightPadding: Kirigami.Units.largeSpacing
    highlighted: isCurrent
    hoverEnabled: true

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint) {
            contextMenu.popup(eventPoint.position.x, eventPoint.position.y)
        }
    }

    QQC2.Menu {
        id: contextMenu

        QQC2.MenuItem {
            text: i18n("Use Item’s Password as Unlock PIN")
            icon.name: "lock-symbolic"
            enabled: root.item && root.item.id && root.item.login && root.item.login.password
            onTriggered: root.usePasswordAsPinRequested(root.item)
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            source: root.iconForType(root.item ? root.item.type : 0)
            color: root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            isMask: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: root.item ? root.item.name : ""
                color: root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: root.loginSubtitle(root.item)
                visible: text.length > 0
                color: root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                opacity: root.highlighted ? 0.85 : 1.0
                elide: Text.ElideRight
                font: Kirigami.Theme.smallFont
                textFormat: Text.PlainText
            }
        }

        Kirigami.Icon {
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
            source: "starred-symbolic"
            visible: root.item && root.item.favorite
            color: root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.neutralTextColor
            isMask: true
        }
    }
}
