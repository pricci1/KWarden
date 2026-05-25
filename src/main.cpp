#include <QApplication>
#include <QAbstractItemView>
#include <QAction>
#include <QClipboard>
#include <QDateTime>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <KAboutData>
#include <KLocalizedString>
#include <KMessageWidget>

#include <algorithm>
#include <utility>

namespace
{
constexpr int LoginCipherType = 1;
constexpr int HiddenFieldType = 1;

struct VaultField {
    QString name;
    QString value;
    int type = 0;
};

struct VaultItem {
    QString id;
    QString name;
    QString username;
    QString password;
    QString totp;
    QString notes;
    bool favorite = false;
    QList<VaultField> fields;
};

QString masked(const QString &value)
{
    if (value.isEmpty()) {
        return QStringLiteral("—");
    }

    return QString(value.size(), QChar(0x2022));
}

QString totpClipboardValue(QString value)
{
    return value.remove(QLatin1Char(' '));
}

QJsonObject loginItem(const QString &id,
                      const QString &name,
                      const QString &username,
                      const QString &password,
                      const QString &totp,
                      const QString &notes,
                      const QJsonArray &fields = {},
                      bool favorite = false)
{
    const QString uri = QStringLiteral("https://") + QString(name).toLower().remove(QLatin1Char(' ')) + QStringLiteral(".example");

    return {
        {QStringLiteral("object"), QStringLiteral("item")},
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), LoginCipherType},
        {QStringLiteral("name"), name},
        {QStringLiteral("notes"), notes},
        {QStringLiteral("favorite"), favorite},
        {QStringLiteral("revisionDate"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("creationDate"), QDateTime::currentDateTimeUtc().addDays(-90).toString(Qt::ISODate)},
        {QStringLiteral("collectionIds"), QJsonArray{}},
        {QStringLiteral("fields"), fields},
        {QStringLiteral("login"), QJsonObject{
            {QStringLiteral("username"), username},
            {QStringLiteral("password"), password},
            {QStringLiteral("totp"), totp},
            {QStringLiteral("uris"), QJsonArray{QJsonObject{{QStringLiteral("uri"), uri}}}},
            {QStringLiteral("passwordRevisionDate"), QDateTime::currentDateTimeUtc().addDays(-12).toString(Qt::ISODate)},
        }},
    };
}

QJsonArray mockBitwardenItems()
{
    // These objects mirror Bitwarden CLI's `ListResponse`/`CipherResponse` shape:
    // { object: "list", data: [{ object: "item", type: 1, login: ..., fields: ... }] }.
    const QJsonArray customFields{
        QJsonObject{{QStringLiteral("name"), QStringLiteral("Recovery Code")}, {QStringLiteral("value"), QStringLiteral("orchid-garden-42")}, {QStringLiteral("type"), HiddenFieldType}},
        QJsonObject{{QStringLiteral("name"), QStringLiteral("Account ID")}, {QStringLiteral("value"), QStringLiteral("acct_9Yx732")}, {QStringLiteral("type"), 0}},
    };

    const QJsonObject response{
        {QStringLiteral("object"), QStringLiteral("list")},
        {QStringLiteral("data"), QJsonArray{
            loginItem(QStringLiteral("1"), QStringLiteral("Google"), QStringLiteral("MyUsername132"), QStringLiteral("correct horse battery staple"), QStringLiteral("123 456"), QStringLiteral("Primary personal Google account."), customFields, true),
            loginItem(QStringLiteral("2"), QStringLiteral("Twitter"), QStringLiteral("otto_dev"), QStringLiteral("blue-bird-secret"), QStringLiteral("714 209"), QStringLiteral("Developer account.")),
            loginItem(QStringLiteral("3"), QStringLiteral("My Bank"), QStringLiteral("checking-7781"), QStringLiteral("vaulted-bank-password"), QStringLiteral("620 118"), QStringLiteral("Call before international travel."), QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("PIN")}, {QStringLiteral("value"), QStringLiteral("9842")}, {QStringLiteral("type"), HiddenFieldType}}}),
            loginItem(QStringLiteral("4"), QStringLiteral("OpenRouter"), QStringLiteral("otto@example.com"), QStringLiteral("sk-or-v1-not-a-real-key"), QString(), QStringLiteral("API dashboard."), QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("Default model")}, {QStringLiteral("value"), QStringLiteral("openai/gpt-4.1")}, {QStringLiteral("type"), 0}}}),
            loginItem(QStringLiteral("5"), QStringLiteral("Some Site"), QStringLiteral("somebody"), QStringLiteral("generic-password"), QStringLiteral("832 551"), QStringLiteral("Imported from browser extension.")),
        }},
    };

    return response.value(QStringLiteral("data")).toArray();
}

VaultItem itemFromJson(const QJsonObject &object)
{
    VaultItem item;
    item.id = object.value(QStringLiteral("id")).toString();
    item.name = object.value(QStringLiteral("name")).toString();
    item.notes = object.value(QStringLiteral("notes")).toString();
    item.favorite = object.value(QStringLiteral("favorite")).toBool();

    const auto login = object.value(QStringLiteral("login")).toObject();
    item.username = login.value(QStringLiteral("username")).toString();
    item.password = login.value(QStringLiteral("password")).toString();
    item.totp = login.value(QStringLiteral("totp")).toString();

    const auto fields = object.value(QStringLiteral("fields")).toArray();
    for (const auto &fieldValue : fields) {
        const auto field = fieldValue.toObject();
        item.fields.append({
            field.value(QStringLiteral("name")).toString(),
            field.value(QStringLiteral("value")).toString(),
            field.value(QStringLiteral("type")).toInt(),
        });
    }

    return item;
}

QList<VaultItem> loadMockVault()
{
    QList<VaultItem> items;
    for (const auto &value : mockBitwardenItems()) {
        items.append(itemFromJson(value.toObject()));
    }
    return items;
}

class SecretRow : public QWidget
{
public:
    SecretRow(const QString &label, const QString &value, bool conceal, QWidget *parent = nullptr, const QString &clipboardValue = QString())
        : QWidget(parent)
        , m_value(clipboardValue.isNull() ? value : clipboardValue)
    {
        setObjectName(QStringLiteral("fieldRow"));

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(14, 9, 14, 9);
        layout->setSpacing(2);

        auto *title = new QLabel(label, this);
        QFont titleFont = title->font();
        titleFont.setBold(true);
        title->setFont(titleFont);

        auto *valueRow = new QHBoxLayout;
        auto *valueLabel = new QLabel(conceal ? masked(value) : (value.isEmpty() ? QStringLiteral("—") : value), this);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valueLabel->setMinimumWidth(220);

        auto *copyButton = new QPushButton(i18n("Copy"), this);
        copyButton->setEnabled(!value.isEmpty());
        copyButton->setMinimumWidth(92);
        connect(copyButton, &QPushButton::clicked, this, [this, copyButton]() {
            QApplication::clipboard()->setText(m_value);
            copyButton->setText(i18n("Copied"));
            QTimer::singleShot(1200, copyButton, [copyButton]() { copyButton->setText(i18n("Copy")); });
        });

        valueRow->addSpacing(8);
        valueRow->addWidget(valueLabel, 1);
        valueRow->addWidget(copyButton);

        layout->addWidget(title);
        layout->addLayout(valueRow);
    }

private:
    QString m_value;
};

class KWardenWindow : public QMainWindow
{
public:
    explicit KWardenWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
        , m_items(loadMockVault())
    {
        setWindowTitle(i18n("KWarden"));
        resize(1060, 680);
        setMinimumSize(860, 560);

        auto *central = new QWidget(this);
        central->setObjectName(QStringLiteral("appRoot"));
        central->setStyleSheet(QStringLiteral(R"(
            #appRoot {
                background: palette(window);
            }
            #topBar {
                background: palette(window);
                border-bottom: 1px solid palette(mid);
            }
            #sidebarPane {
                background: palette(base);
                border-right: 1px solid palette(mid);
            }
            #detailPane {
                background: palette(base);
            }
            #sidebarTitle, #detailTitle {
                font-size: 20px;
                font-weight: 600;
            }
            #sectionLabel, #shortcutLabel {
                color: palette(mid);
                font-weight: 600;
            }
            #searchBox {
                min-height: 34px;
                padding-left: 10px;
                border: 1px solid palette(mid);
                border-radius: 6px;
                background: palette(base);
            }
            QListWidget {
                background: transparent;
                border: 0;
                outline: 0;
            }
            QListWidget::item {
                min-height: 34px;
                padding: 6px 10px;
                border-radius: 6px;
            }
            QListWidget::item:selected {
                background: palette(highlight);
                color: palette(highlighted-text);
            }
            #fieldRow {
                background: palette(alternate-base);
                border: 1px solid palette(midlight);
                border-radius: 8px;
            }
            QScrollArea {
                border: 0;
                background: transparent;
            }
            QPushButton {
                min-height: 28px;
                padding-left: 12px;
                padding-right: 12px;
            }
        )"));

        auto *root = new QVBoxLayout(central);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto *topBar = new QFrame(central);
        topBar->setObjectName(QStringLiteral("topBar"));
        auto *topBarLayout = new QHBoxLayout(topBar);
        topBarLayout->setContentsMargins(20, 12, 20, 12);
        topBarLayout->setSpacing(16);

        auto *title = new QLabel(i18n("KWarden"), topBar);
        title->setObjectName(QStringLiteral("detailTitle"));
        auto *subtitle = new QLabel(i18n("Bitwarden CLI vault"), topBar);
        subtitle->setObjectName(QStringLiteral("sectionLabel"));
        topBarLayout->addWidget(title);
        topBarLayout->addWidget(subtitle);
        topBarLayout->addStretch();

        auto *shortcutHint = new QLabel(i18n("Ctrl+U Username   Ctrl+P Password   Ctrl+T TOTP"), topBar);
        shortcutHint->setObjectName(QStringLiteral("shortcutLabel"));
        topBarLayout->addWidget(shortcutHint);
        root->addWidget(topBar);

        auto *splitter = new QSplitter(Qt::Horizontal, central);

        auto *sidebar = new QWidget(splitter);
        sidebar->setObjectName(QStringLiteral("sidebarPane"));
        auto *sidebarLayout = new QVBoxLayout(sidebar);
        sidebarLayout->setContentsMargins(12, 12, 12, 12);
        sidebarLayout->setSpacing(10);

        m_search = new QLineEdit(central);
        m_search->setObjectName(QStringLiteral("searchBox"));
        m_search->setPlaceholderText(i18n("Search vault…"));
        m_search->setClearButtonEnabled(true);
        sidebarLayout->addWidget(m_search);

        auto *sidebarTitle = new QLabel(i18n("Vault Items"), sidebar);
        sidebarTitle->setObjectName(QStringLiteral("sectionLabel"));
        sidebarLayout->addWidget(sidebarTitle);

        m_list = new QListWidget(sidebar);
        m_list->setMinimumWidth(260);
        m_list->setSelectionMode(QAbstractItemView::SingleSelection);
        sidebarLayout->addWidget(m_list, 1);
        splitter->addWidget(sidebar);

        m_detail = new QWidget(splitter);
        m_detail->setObjectName(QStringLiteral("detailPane"));
        m_detailLayout = new QVBoxLayout(m_detail);
        m_detailLayout->setContentsMargins(32, 24, 32, 24);
        m_detailLayout->setSpacing(16);
        splitter->addWidget(m_detail);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);

        root->addWidget(splitter, 1);
        setCentralWidget(central);

        connect(m_search, &QLineEdit::textChanged, this, &KWardenWindow::refilter);
        connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
            const auto item = m_list->item(row);
            if (item == nullptr) {
                showEmptyDetail();
                return;
            }
            showItem(item->data(Qt::UserRole).toString());
        });

        addCopyShortcut(QKeySequence(Qt::CTRL | Qt::Key_U), i18n("username"), &VaultItem::username);
        addCopyShortcut(QKeySequence(Qt::CTRL | Qt::Key_P), i18n("password"), &VaultItem::password);
        addCopyShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), i18n("TOTP"), &VaultItem::totp);

        refilter();
        statusBar()->showMessage(i18n("Development mode: %1 mocked Bitwarden CLI items loaded", m_items.size()));
    }

private:
    using VaultItemStringMember = QString VaultItem::*;

    void addCopyShortcut(const QKeySequence &sequence, const QString &fieldName, VaultItemStringMember field)
    {
        auto *shortcut = new QShortcut(sequence, this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, fieldName, field]() {
            copyCurrentField(fieldName, field);
        });
    }

    void copyCurrentField(const QString &fieldName, VaultItemStringMember field)
    {
        const auto *item = selectedItem();
        if (item == nullptr) {
            statusBar()->showMessage(i18n("No vault item selected"), 2500);
            return;
        }

        const QString value = (*item).*field;
        if (value.isEmpty()) {
            statusBar()->showMessage(i18n("Selected item has no %1", fieldName), 2500);
            return;
        }

        QApplication::clipboard()->setText(field == &VaultItem::totp ? totpClipboardValue(value) : value);
        statusBar()->showMessage(i18n("Copied %1 for %2", fieldName, item->name), 2500);
    }

    const VaultItem *selectedItem() const
    {
        const QString id = currentItemId();
        const auto it = std::find_if(m_items.cbegin(), m_items.cend(), [&id](const VaultItem &item) {
            return item.id == id;
        });

        return it == m_items.cend() ? nullptr : &(*it);
    }

    void refilter()
    {
        const QString query = m_search->text().trimmed();
        const QString previouslySelectedId = currentItemId();
        m_list->clear();

        for (const auto &item : std::as_const(m_items)) {
            const QString haystack = QStringList{item.name, item.username, item.notes}.join(QLatin1Char(' '));
            if (!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive)) {
                continue;
            }

            auto *row = new QListWidgetItem(item.favorite ? i18n("★ %1", item.name) : item.name, m_list);
            row->setData(Qt::UserRole, item.id);
        }

        if (m_list->count() == 0) {
            showEmptyDetail();
            return;
        }

        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(Qt::UserRole).toString() == previouslySelectedId) {
                m_list->setCurrentRow(i);
                return;
            }
        }

        m_list->setCurrentRow(0);
    }

    QString currentItemId() const
    {
        const auto item = m_list->currentItem();
        return item == nullptr ? QString() : item->data(Qt::UserRole).toString();
    }

    void clearDetail()
    {
        while (auto *child = m_detailLayout->takeAt(0)) {
            delete child->widget();
            delete child;
        }
    }

    void showEmptyDetail()
    {
        clearDetail();
        auto *empty = new QLabel(i18n("No vault item selected"), m_detail);
        empty->setAlignment(Qt::AlignCenter);
        m_detailLayout->addWidget(empty, 1);
    }

    void showItem(const QString &id)
    {
        const auto it = std::find_if(m_items.cbegin(), m_items.cend(), [&id](const VaultItem &item) {
            return item.id == id;
        });

        if (it == m_items.cend()) {
            showEmptyDetail();
            return;
        }

        clearDetail();
        const VaultItem &item = *it;

        auto *title = new QLabel(item.favorite ? i18n("★ %1", item.name) : item.name, m_detail);
        title->setObjectName(QStringLiteral("detailTitle"));
        QFont titleFont = title->font();
        titleFont.setBold(true);
        title->setFont(titleFont);
        m_detailLayout->addWidget(title);

        auto *hint = new QLabel(i18n("Use the buttons or keyboard shortcuts to copy fields from this item."), m_detail);
        hint->setObjectName(QStringLiteral("sectionLabel"));
        m_detailLayout->addWidget(hint);

        auto *scrollArea = new QScrollArea(m_detail);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);

        auto *content = new QWidget(scrollArea);
        auto *contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(12);

        contentLayout->addWidget(new SecretRow(i18n("Username"), item.username, false, content));
        contentLayout->addWidget(new SecretRow(i18n("Password"), item.password, true, content));
        contentLayout->addWidget(new SecretRow(i18n("TOTP"), item.totp, false, content, totpClipboardValue(item.totp)));

        for (const auto &field : item.fields) {
            contentLayout->addWidget(new SecretRow(field.name, field.value, field.type == HiddenFieldType, content));
        }

        auto *notesTitle = new QLabel(i18n("Notes"), content);
        QFont notesFont = notesTitle->font();
        notesFont.setBold(true);
        notesTitle->setFont(notesFont);
        auto *notes = new QLabel(item.notes.isEmpty() ? QStringLiteral("—") : item.notes, content);
        notes->setWordWrap(true);
        notes->setTextInteractionFlags(Qt::TextSelectableByMouse);
        contentLayout->addWidget(notesTitle);
        contentLayout->addWidget(notes);
        contentLayout->addStretch();

        scrollArea->setWidget(content);
        m_detailLayout->addWidget(scrollArea, 1);
    }

    QList<VaultItem> m_items;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QWidget *m_detail = nullptr;
    QVBoxLayout *m_detailLayout = nullptr;
};

int quitAfterMs(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--quit-after-ms")) {
            bool ok = false;
            const int value = QString::fromLocal8Bit(argv[i + 1]).toInt(&ok);
            if (ok && value > 0) {
                return value;
            }
        }
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("kwarden");
    KAboutData about(QStringLiteral("kwarden"),
                     i18n("KWarden"),
                     QStringLiteral("0.1.0"),
                     i18n("KDE native frontend for Bitwarden CLI"),
                     KAboutLicense::GPL_V3);
    KAboutData::setApplicationData(about);

    KWardenWindow window;
    window.show();
    if (const int quitDelay = quitAfterMs(argc, argv); quitDelay > 0) {
        QTimer::singleShot(quitDelay, &window, &QWidget::close);
    }

    return app.exec();
}
