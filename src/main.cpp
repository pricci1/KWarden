#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHostAddress>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMimeData>
#include <QMessageAuthenticationCode>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QWindow>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include "totp.h"

#include <algorithm>
#include <functional>
#include <memory>

class ClipboardBridge : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardBridge(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_clearTimer.setSingleShot(true);
        m_clearTimer.setInterval(45000);
        connect(&m_clearTimer, &QTimer::timeout, this, [this]() {
            QClipboard *clipboard = QGuiApplication::clipboard();
            if (clipboard->text() == m_lastCopiedText) {
                clipboard->clear();
            }
            m_lastCopiedText.clear();
        });
    }

    Q_INVOKABLE void copy(const QString &value)
    {
        m_lastCopiedText = value;
        auto *mimeData = new QMimeData;
        mimeData->setText(value);
        mimeData->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));
        QGuiApplication::clipboard()->setMimeData(mimeData);
        m_clearTimer.start();
    }

private:
    QString m_lastCopiedText;
    QTimer m_clearTimer;
};

class TotpBridge : public QObject
{
    Q_OBJECT

public:
    explicit TotpBridge(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    Q_INVOKABLE QString code(const QString &value) const
    {
        return Totp::currentCode(value);
    }

    Q_INVOKABLE int secondsRemaining(const QString &value) const
    {
        return Totp::currentSecondsRemaining(value);
    }
};

class KeyboardShortcuts : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardShortcuts(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

Q_SIGNALS:
    void findRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)

        if (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress) {
            return false;
        }

        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const QKeyCombination keyCombination(keyEvent->modifiers() & ~Qt::KeypadModifier, Qt::Key(keyEvent->key()));
        if (keyCombination.toCombined() == QKeySequence(QKeySequence::Find)[0].toCombined()) {
            keyEvent->accept();
            Q_EMIT findRequested();
            return true;
        }

        return false;
    }
};

class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QWindow *window, QObject *parent = nullptr)
        : QObject(parent)
        , m_window(window)
        , m_trayIcon(QIcon::fromTheme(QStringLiteral("cl.tri.kwarden")), this)
    {
        if (!m_window || !QSystemTrayIcon::isSystemTrayAvailable()) {
            return;
        }

        m_window->installEventFilter(this);
        QApplication::setQuitOnLastWindowClosed(false);

        QAction *showAction = m_menu.addAction(i18n("Show KWarden"));
        QAction *quitAction = m_menu.addAction(i18n("Quit"));

        connect(showAction, &QAction::triggered, this, &TrayController::showWindow);
        connect(quitAction, &QAction::triggered, this, [this]() {
            m_quitting = true;
            QCoreApplication::exit(0);
        });
        connect(&m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                showWindow();
            }
        });

        m_trayIcon.setToolTip(i18n("KWarden"));
        m_trayIcon.setContextMenu(&m_menu);
        m_trayIcon.show();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_window && event->type() == QEvent::Close && !m_quitting && m_trayIcon.isVisible()) {
            static_cast<QCloseEvent *>(event)->ignore();
            m_window->hide();
            return true;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void showWindow()
    {
        if (!m_window) {
            return;
        }

        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }

    QWindow *m_window = nullptr;
    QSystemTrayIcon m_trayIcon;
    QMenu m_menu;
    bool m_quitting = false;
};

class BwStrategy : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(int, const QByteArray &, const QByteArray &)>;

    explicit BwStrategy(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual void status(Callback callback) = 0;
    virtual void unlock(const QString &masterPassword, Callback callback) = 0;
    virtual void listItems(const QString &session, Callback callback) = 0;
    virtual void lock(const QString &session, Callback callback) = 0;
    virtual void clearSession() {}
};

class DirectBwStrategy : public BwStrategy
{
    Q_OBJECT

public:
    explicit DirectBwStrategy(QObject *parent = nullptr)
        : BwStrategy(parent)
    {
    }

    void status(Callback callback) override
    {
        runBw({QStringLiteral("status"), QStringLiteral("--raw")}, {}, {}, callback);
    }

    void unlock(const QString &masterPassword, Callback callback) override
    {
        runBw({QStringLiteral("unlock"), QStringLiteral("--raw")}, masterPassword.toUtf8(), {}, callback);
    }

    void listItems(const QString &session, Callback callback) override
    {
        runBw({QStringLiteral("list"), QStringLiteral("items")}, {}, session, callback);
    }

    void lock(const QString &session, Callback callback) override
    {
        runBw({QStringLiteral("lock")}, {}, session, callback);
    }

private:
    void runBw(const QStringList &arguments, const QByteArray &stdinData, const QString &session, Callback callback)
    {
        auto *process = new QProcess(this);
        process->setProgram(QStringLiteral("bw"));
        process->setArguments(arguments);

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        if (!session.isEmpty()) {
            environment.insert(QStringLiteral("BW_SESSION"), session);
        }
        process->setProcessEnvironment(environment);

        connect(process, &QProcess::started, process, [process, stdinData]() {
            if (!stdinData.isEmpty()) {
                process->write(stdinData);
                process->write("\n");
            }
            process->closeWriteChannel();
        });

        connect(process, &QProcess::finished, this, [process, callback](int exitCode, QProcess::ExitStatus) {
            const QByteArray stdoutData = process->readAllStandardOutput();
            const QByteArray stderrData = process->readAllStandardError();
            process->deleteLater();
            callback(exitCode, stdoutData, stderrData);
        });

        connect(process, &QProcess::errorOccurred, this, [process, callback](QProcess::ProcessError) {
            const QByteArray stderrData = process->errorString().toUtf8();
            process->deleteLater();
            callback(-1, {}, stderrData);
        });

        process->start();
    }
};

class ServeBwStrategy : public BwStrategy
{
    Q_OBJECT

public:
    explicit ServeBwStrategy(QObject *parent = nullptr)
        : BwStrategy(parent)
    {
    }

    ~ServeBwStrategy() override
    {
        stopServer();
    }

    void status(Callback callback) override
    {
        ensureServer([this, callback](const QByteArray &error) {
            if (!error.isEmpty()) {
                callback(-1, {}, error);
                return;
            }

            get(QStringLiteral("/status"), [callback](int exitCode, const QByteArray &body, const QByteArray &stderrData) {
                if (exitCode != 0) {
                    callback(exitCode, {}, stderrData);
                    return;
                }

                const QJsonObject data = responseData(body);
                const QJsonObject statusObject = data.value(QStringLiteral("template")).toObject();
                if (statusObject.isEmpty()) {
                    callback(-1, {}, QByteArrayLiteral("Unexpected Bitwarden serve status response"));
                    return;
                }
                callback(0, QJsonDocument(statusObject).toJson(QJsonDocument::Compact), {});
            });
        });
    }

    void unlock(const QString &masterPassword, Callback callback) override
    {
        ensureServer([this, masterPassword, callback](const QByteArray &error) {
            if (!error.isEmpty()) {
                callback(-1, {}, error);
                return;
            }

            QJsonObject requestBody;
            requestBody.insert(QStringLiteral("password"), masterPassword);
            post(QStringLiteral("/unlock"), QJsonDocument(requestBody).toJson(QJsonDocument::Compact), [this, callback](int exitCode,
                                                                                                                        const QByteArray &body,
                                                                                                                        const QByteArray &stderrData) {
                if (exitCode != 0) {
                    callback(exitCode, {}, stderrData);
                    return;
                }

                const QJsonObject data = responseData(body);
                const QString session = data.value(QStringLiteral("raw")).toString().trimmed();
                if (session.isEmpty()) {
                    callback(-1, {}, QByteArrayLiteral("Bitwarden serve unlock response did not include a session"));
                    return;
                }

                m_session = session;
                callback(0, session.toUtf8(), {});
            });
        });
    }

    void listItems(const QString &session, Callback callback) override
    {
        if (!session.isEmpty() && session != m_session) {
            m_session = session;
            stopServer();
        }

        ensureServer([this, callback](const QByteArray &error) {
            if (!error.isEmpty()) {
                callback(-1, {}, error);
                return;
            }

            get(QStringLiteral("/list/object/items"), [callback](int exitCode, const QByteArray &body, const QByteArray &stderrData) {
                if (exitCode != 0) {
                    callback(exitCode, {}, stderrData);
                    return;
                }

                const QJsonObject data = responseData(body);
                const QJsonArray items = data.value(QStringLiteral("data")).toArray();
                callback(0, QJsonDocument(items).toJson(QJsonDocument::Compact), {});
            });
        });
    }

    void lock(const QString &session, Callback callback) override
    {
        if (!session.isEmpty() && session != m_session) {
            m_session = session;
            stopServer();
        }

        ensureServer([this, callback](const QByteArray &error) {
            if (!error.isEmpty()) {
                callback(-1, {}, error);
                return;
            }

            post(QStringLiteral("/lock"), {}, [this, callback](int exitCode, const QByteArray &, const QByteArray &stderrData) {
                if (exitCode == 0) {
                    m_session.clear();
                }
                callback(exitCode, {}, stderrData);
            });
        });
    }

    void clearSession() override
    {
        m_session.clear();
        stopServer();
    }

private:
    using ServerCallback = std::function<void(const QByteArray &)>;

    void ensureServer(ServerCallback callback)
    {
        if (m_server && m_server->state() != QProcess::NotRunning && m_port != 0) {
            callback({});
            return;
        }

        stopServer();

        QTcpServer portServer;
        if (!portServer.listen(QHostAddress::LocalHost, 0)) {
            callback(portServer.errorString().toUtf8());
            return;
        }
        m_port = portServer.serverPort();
        portServer.close();

        m_server = new QProcess(this);
        m_server->setProgram(QStringLiteral("bw"));
        m_server->setArguments({QStringLiteral("serve"), QStringLiteral("--hostname"), QStringLiteral("localhost"), QStringLiteral("--port"), QString::number(m_port)});

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        if (!m_session.isEmpty()) {
            environment.insert(QStringLiteral("BW_SESSION"), m_session);
        }
        m_server->setProcessEnvironment(environment);

        connect(m_server, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
            if (m_server) {
                m_server->deleteLater();
                m_server = nullptr;
            }
            m_port = 0;
        });

        connect(m_server, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            if (m_server) {
                m_lastServerError = m_server->errorString().toUtf8();
            }
        });

        m_lastServerError.clear();
        m_server->start();
        waitUntilReady(std::move(callback), 0);
    }

    void waitUntilReady(ServerCallback callback, int attempt)
    {
        if (!m_server || m_server->state() == QProcess::NotRunning) {
            QByteArray error = m_lastServerError;
            if (error.isEmpty()) {
                error = QByteArrayLiteral("Bitwarden serve exited before becoming ready");
            }
            callback(error);
            return;
        }

        get(QStringLiteral("/status"), [this, callback = std::move(callback), attempt](int exitCode, const QByteArray &, const QByteArray &) mutable {
            if (exitCode == 0) {
                callback({});
                return;
            }

            if (attempt >= 50) {
                callback(QByteArrayLiteral("Timed out waiting for Bitwarden serve to start"));
                return;
            }

            QTimer::singleShot(100, this, [this, callback = std::move(callback), attempt]() mutable {
                waitUntilReady(std::move(callback), attempt + 1);
            });
        });
    }

    void get(const QString &path, Callback callback)
    {
        request(QByteArrayLiteral("GET"), path, {}, std::move(callback));
    }

    void post(const QString &path, const QByteArray &body, Callback callback)
    {
        request(QByteArrayLiteral("POST"), path, body, std::move(callback));
    }

    void request(const QByteArray &method, const QString &path, const QByteArray &body, Callback callback)
    {
        QUrl url;
        url.setScheme(QStringLiteral("http"));
        url.setHost(QStringLiteral("localhost"));
        url.setPort(m_port);
        url.setPath(path);

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        QNetworkReply *reply = nullptr;
        if (method == QByteArrayLiteral("POST")) {
            reply = m_network.post(request, body);
        } else {
            reply = m_network.get(request);
        }

        connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
            const QByteArray responseBody = reply->readAll();
            const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool ok = reply->error() == QNetworkReply::NoError && statusCode >= 200 && statusCode < 300;
            const QByteArray error = ok ? QByteArray{} : serveErrorMessage(responseBody, reply->errorString().toUtf8());
            reply->deleteLater();
            callback(ok ? 0 : -1, ok ? responseBody : QByteArray{}, error);
        });
    }

    static QJsonObject responseData(const QByteArray &body)
    {
        const QJsonObject response = QJsonDocument::fromJson(body).object();
        return response.value(QStringLiteral("data")).toObject();
    }

    static QByteArray serveErrorMessage(const QByteArray &body, const QByteArray &fallback)
    {
        const QJsonObject response = QJsonDocument::fromJson(body).object();
        const QString topLevelMessage = response.value(QStringLiteral("message")).toString();
        if (!topLevelMessage.isEmpty()) {
            return topLevelMessage.toUtf8();
        }

        const QJsonObject data = responseData(body);
        const QString message = data.value(QStringLiteral("message")).toString();
        if (!message.isEmpty()) {
            return message.toUtf8();
        }
        const QString title = data.value(QStringLiteral("title")).toString();
        if (!title.isEmpty()) {
            return title.toUtf8();
        }
        return fallback;
    }

    void stopServer()
    {
        if (!m_server) {
            m_port = 0;
            return;
        }

        disconnect(m_server, nullptr, this, nullptr);
        if (m_server->state() != QProcess::NotRunning) {
            m_server->terminate();
            if (!m_server->waitForFinished(1000)) {
                m_server->kill();
                m_server->waitForFinished(1000);
            }
        }
        m_server->deleteLater();
        m_server = nullptr;
        m_port = 0;
    }

    QNetworkAccessManager m_network;
    QProcess *m_server = nullptr;
    quint16 m_port = 0;
    QString m_session;
    QByteArray m_lastServerError;
};

class BwCliProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString userEmail READ userEmail NOTIFY userEmailChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool pinSet READ pinSet NOTIFY pinSetChanged)
    Q_PROPERTY(QString pinSourceItemId READ pinSourceItemId NOTIFY pinSourceItemIdChanged)

public:
    explicit BwCliProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
        const QString backend = QString::fromLocal8Bit(qgetenv("KWARDEN_BW_BACKEND")).toLower();
        if (backend == QStringLiteral("serve")) {
            m_backend = std::make_unique<ServeBwStrategy>(this);
        } else {
            m_backend = std::make_unique<DirectBwStrategy>(this);
        }

        m_pinSourceItemId = QSettings().value(QStringLiteral("pin/sourceItemId")).toString();
    }

    QVariantList items() const
    {
        return m_items;
    }

    QString state() const
    {
        return m_state;
    }

    QString statusText() const
    {
        return m_statusText;
    }

    QString userEmail() const
    {
        return m_userEmail;
    }

    bool busy() const
    {
        return m_busy;
    }

    bool pinSet() const
    {
        return !m_pinSalt.isEmpty() && !m_wrappedSession.isEmpty();
    }

    QString pinSourceItemId() const
    {
        return m_pinSourceItemId;
    }

    Q_INVOKABLE void refresh()
    {
        if (m_busy) {
            return;
        }

        if (m_state == QStringLiteral("pinLocked")) {
            setStatusText(i18n("KWarden is locked. Unlock with your PIN or master password."));
            return;
        }

        if (QStandardPaths::findExecutable(QStringLiteral("bw")).isEmpty()) {
            setItems({});
            setState(QStringLiteral("missing"));
            setStatusText(i18n("Bitwarden CLI (bw) was not found in PATH"));
            return;
        }

        setBusy(true);
        m_backend->status([this](int exitCode, const QByteArray &stdoutData, const QByteArray &stderrData) {
            if (exitCode != 0) {
                setItems({});
                setState(QStringLiteral("error"));
                setStatusText(errorMessage(i18n("Failed to read Bitwarden CLI status"), stderrData));
                setBusy(false);
                return;
            }

            const QJsonDocument statusDocument = QJsonDocument::fromJson(stdoutData);
            const QJsonObject statusObject = statusDocument.object();
            const QString status = statusObject.value(QStringLiteral("status")).toString();
            setUserEmail(statusObject.value(QStringLiteral("userEmail")).toString());

            if (status == QStringLiteral("unlocked")) {
                setState(QStringLiteral("unlocked"));
                loadItems();
            } else if (status == QStringLiteral("locked")) {
                setItems({});
                setState(QStringLiteral("locked"));
                setStatusText(i18n("Vault locked. Enter your master password to unlock."));
                setBusy(false);
            } else if (status == QStringLiteral("unauthenticated")) {
                setItems({});
                clearSession();
                setState(QStringLiteral("unauthenticated"));
                setStatusText(i18n("Not logged in. Run ‘bw login’ in a terminal, then refresh."));
                setBusy(false);
            } else {
                setItems({});
                setState(QStringLiteral("error"));
                setStatusText(i18n("Unexpected Bitwarden CLI status response"));
                setBusy(false);
            }
        });
    }

    Q_INVOKABLE void unlock(const QString &masterPassword)
    {
        if (m_busy || masterPassword.isEmpty()) {
            return;
        }

        setBusy(true);
        m_backend->unlock(masterPassword, [this](int exitCode, const QByteArray &stdoutData, const QByteArray &stderrData) {
            if (exitCode != 0) {
                clearSession();
                setItems({});
                setState(QStringLiteral("locked"));
                setStatusText(errorMessage(i18n("Failed to unlock vault"), stderrData));
                setBusy(false);
                return;
            }

            m_session = QString::fromUtf8(stdoutData).trimmed();
            setState(QStringLiteral("unlocked"));
            setStatusText(i18n("Vault unlocked for this KWarden session"));
            if (pinSet()) {
                clearPinState();
            }
            m_applySavedPinAfterLoad = !m_pinSourceItemId.isEmpty();
            loadItems();
        });
    }

    Q_INVOKABLE void unlockWithPin(const QString &pin)
    {
        if (m_busy || pin.isEmpty() || !pinSet()) {
            return;
        }

        const QByteArray session = unwrapSession(pin);
        if (session.isEmpty()) {
            ++m_invalidPinAttempts;
            if (m_invalidPinAttempts >= 5) {
                clearPinState();
                clearSession();
                setItems({});
                setState(QStringLiteral("locked"));
                setStatusText(i18n("Too many incorrect PIN attempts. PIN unlock disabled; use your master password."));
                return;
            }

            setStatusText(i18np("Incorrect PIN. %1 attempt remaining.", "Incorrect PIN. %1 attempts remaining.", 5 - m_invalidPinAttempts));
            return;
        }

        m_invalidPinAttempts = 0;
        m_session = QString::fromUtf8(session);
        setState(QStringLiteral("unlocked"));
        setStatusText(i18n("Vault unlocked with PIN"));
        setBusy(true);
        loadItems();
    }

    Q_INVOKABLE void setPin(const QString &pin)
    {
        if (setPinFromPassword(pin, i18n("PIN enabled until KWarden quits"))) {
            clearPinSourceItemId();
        }
    }

    Q_INVOKABLE void useItemPasswordAsPin(const QString &itemId)
    {
        if (itemId.isEmpty()) {
            setStatusText(i18n("Selected item has no id"));
            return;
        }

        const QVariantMap item = itemById(itemId);
        if (item.isEmpty()) {
            setStatusText(i18n("Selected item was not found"));
            return;
        }

        const QString password = loginPassword(item);
        if (password.isEmpty()) {
            setStatusText(i18n("Selected item has no password"));
            return;
        }

        if (!setPinFromPassword(password, i18n("Using %1’s password as the unlock PIN", item.value(QStringLiteral("name")).toString()))) {
            return;
        }

        setPinSourceItemId(itemId);
    }

    Q_INVOKABLE void clearPin()
    {
        clearPinState();
        clearPinSourceItemId();
        setStatusText(i18n("PIN unlock disabled"));
    }

    Q_INVOKABLE void lock()
    {
        if (m_busy) {
            return;
        }

        if (pinSet() && !m_session.isEmpty()) {
            clearSession();
            setItems({});
            setState(QStringLiteral("pinLocked"));
            setStatusText(i18n("KWarden locked. Unlock with PIN or master password."));
            return;
        }

        setBusy(true);
        m_backend->lock(m_session, [this](int, const QByteArray &, const QByteArray &) {
            clearSession();
            clearPinState();
            setItems({});
            setState(QStringLiteral("locked"));
            setStatusText(i18n("Vault locked"));
            setBusy(false);
        });
    }

Q_SIGNALS:
    void itemsChanged();
    void stateChanged();
    void statusTextChanged();
    void userEmailChanged();
    void busyChanged();
    void pinSetChanged();
    void pinSourceItemIdChanged();

private:
    static constexpr int PinKdfIterations = 600000;
    static constexpr int MinimumPinLength = 6;
    static constexpr int MinimumNumericPinLength = 8;

    void loadItems()
    {
        m_backend->listItems(m_session, [this](int exitCode, const QByteArray &stdoutData, const QByteArray &stderrData) {
            if (exitCode != 0) {
                setItems({});
                setState(QStringLiteral("error"));
                setStatusText(errorMessage(i18n("Failed to load vault items"), stderrData));
                setBusy(false);
                return;
            }

            const QJsonDocument itemsDocument = QJsonDocument::fromJson(stdoutData);
            if (!itemsDocument.isArray()) {
                setItems({});
                setState(QStringLiteral("error"));
                setStatusText(i18n("Unexpected Bitwarden CLI item response"));
                setBusy(false);
                return;
            }

            QVariantList items = itemsDocument.array().toVariantList();
            setItems(items);
            setState(QStringLiteral("unlocked"));
            setStatusText(i18np("Loaded %1 vault item", "Loaded %1 vault items", items.size()));
            if (m_applySavedPinAfterLoad) {
                m_applySavedPinAfterLoad = false;
                applySavedPinSource();
            }
            setBusy(false);
        });
    }

    static QString errorMessage(const QString &prefix, const QByteArray &stderrData)
    {
        const QString detail = QString::fromUtf8(stderrData).trimmed();
        if (detail.isEmpty()) {
            return prefix;
        }
        return i18n("%1: %2", prefix, detail);
    }

    void clearSession()
    {
        m_session.clear();
        if (m_backend) {
            m_backend->clearSession();
        }
    }

    void clearPinState()
    {
        const bool hadPin = pinSet();
        m_pinSalt.clear();
        m_pinNonce.clear();
        m_wrappedSession.clear();
        m_pinVerifier.clear();
        m_invalidPinAttempts = 0;
        if (hadPin) {
            Q_EMIT pinSetChanged();
        }
    }

    void setPinSourceItemId(const QString &itemId)
    {
        if (m_pinSourceItemId == itemId) {
            return;
        }

        m_pinSourceItemId = itemId;
        QSettings().setValue(QStringLiteral("pin/sourceItemId"), itemId);
        Q_EMIT pinSourceItemIdChanged();
    }

    void clearPinSourceItemId()
    {
        if (m_pinSourceItemId.isEmpty()) {
            return;
        }

        m_pinSourceItemId.clear();
        QSettings().remove(QStringLiteral("pin/sourceItemId"));
        Q_EMIT pinSourceItemIdChanged();
    }

    bool setPinFromPassword(const QString &pin, const QString &successMessage)
    {
        const bool numericPin = std::all_of(pin.cbegin(), pin.cend(), [](const QChar character) {
            return character.isDigit();
        });
        if (numericPin && pin.size() < MinimumNumericPinLength) {
            setStatusText(i18n("Numeric PINs must be at least 8 digits"));
            return false;
        }

        if (pin.size() < MinimumPinLength) {
            setStatusText(i18n("PIN must be at least 6 characters"));
            return false;
        }

        if (m_session.isEmpty() || m_state != QStringLiteral("unlocked")) {
            setStatusText(i18n("Unlock with your master password before setting a PIN"));
            return false;
        }

        m_pinSalt = randomBytes(16);
        m_pinNonce = randomBytes(16);
        m_invalidPinAttempts = 0;
        wrapSession(pin, m_session.toUtf8());
        Q_EMIT pinSetChanged();
        setStatusText(successMessage);
        return true;
    }

    static QString loginPassword(const QVariantMap &item)
    {
        return item.value(QStringLiteral("login")).toMap().value(QStringLiteral("password")).toString();
    }

    QVariantMap itemById(const QString &itemId) const
    {
        for (const QVariant &itemVariant : m_items) {
            const QVariantMap item = itemVariant.toMap();
            if (item.value(QStringLiteral("id")).toString() == itemId) {
                return item;
            }
        }
        return {};
    }

    void applySavedPinSource()
    {
        if (m_pinSourceItemId.isEmpty()) {
            return;
        }

        const QVariantMap item = itemById(m_pinSourceItemId);
        if (item.isEmpty()) {
            setStatusText(i18n("Saved PIN item was not found. Choose another item to restore automatic PIN setup."));
            return;
        }

        const QString password = loginPassword(item);
        if (password.isEmpty()) {
            setStatusText(i18n("Saved PIN item has no password. Choose another item to restore automatic PIN setup."));
            return;
        }

        setPinFromPassword(password, i18n("PIN restored from saved vault item"));
    }

    static QByteArray randomBytes(qsizetype size)
    {
        QByteArray bytes(size, Qt::Uninitialized);
        for (qsizetype i = 0; i < size; ++i) {
            bytes[i] = char(QRandomGenerator::global()->generate() & 0xff);
        }
        return bytes;
    }

    static QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
    {
        return QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);
    }

    static bool constantTimeEquals(const QByteArray &left, const QByteArray &right)
    {
        qsizetype difference = left.size() ^ right.size();
        const qsizetype comparisonSize = std::max(left.size(), right.size());
        for (qsizetype i = 0; i < comparisonSize; ++i) {
            const uchar leftByte = i < left.size() ? uchar(left[i]) : 0;
            const uchar rightByte = i < right.size() ? uchar(right[i]) : 0;
            difference |= leftByte ^ rightByte;
        }
        return difference == 0;
    }

    static QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations, qsizetype outputSize)
    {
        QByteArray output;
        output.reserve(outputSize);

        quint32 blockIndex = 1;
        while (output.size() < outputSize) {
            QByteArray blockSalt = salt;
            blockSalt.append(char((blockIndex >> 24) & 0xff));
            blockSalt.append(char((blockIndex >> 16) & 0xff));
            blockSalt.append(char((blockIndex >> 8) & 0xff));
            blockSalt.append(char(blockIndex & 0xff));

            QByteArray u = hmacSha256(password, blockSalt);
            QByteArray block = u;
            for (int i = 1; i < iterations; ++i) {
                u = hmacSha256(password, u);
                for (qsizetype j = 0; j < block.size(); ++j) {
                    block[j] = char(uchar(block[j]) ^ uchar(u[j]));
                }
            }

            output.append(block);
            ++blockIndex;
        }

        output.truncate(outputSize);
        return output;
    }

    static QByteArray xorWithHmacStream(const QByteArray &key, const QByteArray &nonce, const QByteArray &input)
    {
        QByteArray output(input.size(), Qt::Uninitialized);
        qsizetype offset = 0;
        quint32 counter = 0;
        while (offset < input.size()) {
            QByteArray counterBytes = nonce;
            counterBytes.append(char((counter >> 24) & 0xff));
            counterBytes.append(char((counter >> 16) & 0xff));
            counterBytes.append(char((counter >> 8) & 0xff));
            counterBytes.append(char(counter & 0xff));

            const QByteArray streamBlock = hmacSha256(key, counterBytes);
            const qsizetype count = std::min(streamBlock.size(), input.size() - offset);
            for (qsizetype i = 0; i < count; ++i) {
                output[offset + i] = char(uchar(input[offset + i]) ^ uchar(streamBlock[i]));
            }
            offset += count;
            ++counter;
        }
        return output;
    }

    void wrapSession(const QString &pin, const QByteArray &session)
    {
        const QByteArray keys = pbkdf2Sha256(pin.toUtf8(), m_pinSalt, PinKdfIterations, 64);
        const QByteArray encryptionKey = keys.first(32);
        const QByteArray authenticationKey = keys.sliced(32, 32);
        m_wrappedSession = xorWithHmacStream(encryptionKey, m_pinNonce, session);
        m_pinVerifier = hmacSha256(authenticationKey, m_pinNonce + m_wrappedSession);
    }

    QByteArray unwrapSession(const QString &pin) const
    {
        const QByteArray keys = pbkdf2Sha256(pin.toUtf8(), m_pinSalt, PinKdfIterations, 64);
        const QByteArray encryptionKey = keys.first(32);
        const QByteArray authenticationKey = keys.sliced(32, 32);
        const QByteArray verifier = hmacSha256(authenticationKey, m_pinNonce + m_wrappedSession);
        if (!constantTimeEquals(verifier, m_pinVerifier)) {
            return {};
        }
        return xorWithHmacStream(encryptionKey, m_pinNonce, m_wrappedSession);
    }

    void setItems(const QVariantList &items)
    {
        if (m_items == items) {
            return;
        }
        m_items = items;
        Q_EMIT itemsChanged();
    }

    void setState(const QString &state)
    {
        if (m_state == state) {
            return;
        }
        m_state = state;
        Q_EMIT stateChanged();
    }

    void setStatusText(const QString &statusText)
    {
        if (m_statusText == statusText) {
            return;
        }
        m_statusText = statusText;
        Q_EMIT statusTextChanged();
    }

    void setUserEmail(const QString &userEmail)
    {
        if (m_userEmail == userEmail) {
            return;
        }
        m_userEmail = userEmail;
        Q_EMIT userEmailChanged();
    }

    void setBusy(bool busy)
    {
        if (m_busy == busy) {
            return;
        }
        m_busy = busy;
        Q_EMIT busyChanged();
    }

    std::unique_ptr<BwStrategy> m_backend;
    QVariantList m_items;
    QString m_state = QStringLiteral("loading");
    QString m_statusText = i18n("Checking Bitwarden CLI status…");
    QString m_userEmail;
    QString m_session;
    QString m_pinSourceItemId;
    QByteArray m_pinSalt;
    QByteArray m_pinNonce;
    QByteArray m_wrappedSession;
    QByteArray m_pinVerifier;
    int m_invalidPinAttempts = 0;
    bool m_busy = false;
    bool m_applySavedPinAfterLoad = false;
};

namespace
{
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
    QGuiApplication::setDesktopFileName(QStringLiteral("cl.tri.kwarden"));

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("kwarden"));
    QCoreApplication::setApplicationName(QStringLiteral("kwarden"));

    KLocalizedString::setApplicationDomain("kwarden");
    KAboutData about(QStringLiteral("kwarden"),
                     i18n("KWarden"),
                     QStringLiteral("0.1.0"),
                     i18n("KDE native frontend for Bitwarden CLI"),
                     KAboutLicense::GPL_V3);
    KAboutData::setApplicationData(about);

    ClipboardBridge clipboardBridge;
    TotpBridge totpBridge;
    KeyboardShortcuts keyboardShortcuts;
    app.installEventFilter(&keyboardShortcuts);
    BwCliProvider vaultProvider;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("clipboardBridge"), &clipboardBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("totpBridge"), &totpBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("keyboardShortcuts"), &keyboardShortcuts);
    engine.rootContext()->setContextProperty(QStringLiteral("vaultProvider"), &vaultProvider);
    engine.loadFromModule(QStringLiteral("org.kwarden"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    engine.rootObjects().constFirst()->installEventFilter(&keyboardShortcuts);

    TrayController trayController(qobject_cast<QWindow *>(engine.rootObjects().constFirst()), &app);

    if (const int quitDelay = quitAfterMs(argc, argv); quitDelay > 0) {
        QTimer::singleShot(quitDelay, &app, []() {
            QCoreApplication::exit(0);
        });
    }

    QTimer::singleShot(0, &vaultProvider, &BwCliProvider::refresh);

    return app.exec();
}

#include "main.moc"
