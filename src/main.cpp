#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantList>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include <algorithm>
#include <functional>

class ClipboardBridge : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardBridge(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    Q_INVOKABLE void copy(const QString &value) const
    {
        QGuiApplication::clipboard()->setText(value);
    }
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

public:
    explicit BwCliProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
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

        runBw({QStringLiteral("status"), QStringLiteral("--raw")}, {}, false, [this](int exitCode, const QByteArray &stdoutData, const QByteArray &stderrData) {
            if (exitCode != 0) {
                setItems({});
                setState(QStringLiteral("error"));
                setStatusText(errorMessage(i18n("Failed to read Bitwarden CLI status"), stderrData));
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
            } else if (status == QStringLiteral("unauthenticated")) {
                setItems({});
                clearSession();
                setState(QStringLiteral("unauthenticated"));
                setStatusText(i18n("Not logged in. Run ‘bw login’ in a terminal, then refresh."));
            } else {
                setItems({});
                setState(QStringLiteral("error"));
                setStatusText(i18n("Unexpected Bitwarden CLI status response"));
            }
        });
    }

    Q_INVOKABLE void unlock(const QString &masterPassword)
    {
        if (m_busy || masterPassword.isEmpty()) {
            return;
        }

        runBw({QStringLiteral("unlock"), QStringLiteral("--raw")}, masterPassword.toUtf8(), false, [this](int exitCode, const QByteArray &stdoutData, const QByteArray &stderrData) {
            if (exitCode != 0) {
                clearSession();
                setItems({});
                setState(QStringLiteral("locked"));
                setStatusText(errorMessage(i18n("Failed to unlock vault"), stderrData));
                return;
            }

            m_session = QString::fromUtf8(stdoutData).trimmed();
            setState(QStringLiteral("unlocked"));
            setStatusText(i18n("Vault unlocked for this KWarden session"));
            if (pinSet()) {
                clearPinState();
                setStatusText(i18n("Vault unlocked. Set a new PIN if you want PIN lock for this session."));
            }
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
        loadItems();
    }

    Q_INVOKABLE void setPin(const QString &pin)
    {
        if (pin.size() < 4) {
            setStatusText(i18n("PIN must be at least 4 characters"));
            return;
        }

        if (m_session.isEmpty() || m_state != QStringLiteral("unlocked")) {
            setStatusText(i18n("Unlock with your master password before setting a PIN"));
            return;
        }

        m_pinSalt = randomBytes(16);
        m_pinNonce = randomBytes(16);
        m_invalidPinAttempts = 0;
        wrapSession(pin, m_session.toUtf8());
        Q_EMIT pinSetChanged();
        setStatusText(i18n("PIN enabled until KWarden quits"));
    }

    Q_INVOKABLE void clearPin()
    {
        clearPinState();
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

        runBw({QStringLiteral("lock")}, {}, true, [this](int, const QByteArray &, const QByteArray &) {
            clearSession();
            clearPinState();
            setItems({});
            setState(QStringLiteral("locked"));
            setStatusText(i18n("Vault locked"));
        });
    }

Q_SIGNALS:
    void itemsChanged();
    void stateChanged();
    void statusTextChanged();
    void userEmailChanged();
    void busyChanged();
    void pinSetChanged();

private:
    using ProcessCallback = std::function<void(int, const QByteArray &, const QByteArray &)>;
    static constexpr int PinKdfIterations = 120000;

    void loadItems()
    {
        runBw({QStringLiteral("list"), QStringLiteral("items")}, {}, true, [this](int exitCode, const QByteArray &stdoutData, const QByteArray &stderrData) {
            if (exitCode != 0) {
                setItems({});
                setState(QStringLiteral("error"));
                setStatusText(errorMessage(i18n("Failed to load vault items"), stderrData));
                return;
            }

            const QJsonDocument itemsDocument = QJsonDocument::fromJson(stdoutData);
            if (!itemsDocument.isArray()) {
                setItems({});
                setState(QStringLiteral("error"));
                setStatusText(i18n("Unexpected Bitwarden CLI item response"));
                return;
            }

            QVariantList items = itemsDocument.array().toVariantList();
            setItems(items);
            setState(QStringLiteral("unlocked"));
            setStatusText(i18np("Loaded %1 vault item", "Loaded %1 vault items", items.size()));
        });
    }

    void runBw(const QStringList &arguments, const QByteArray &stdinData, bool withSession, ProcessCallback callback)
    {
        setBusy(true);

        auto *process = new QProcess(this);
        process->setProgram(QStringLiteral("bw"));
        process->setArguments(arguments);

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        if (withSession && !m_session.isEmpty()) {
            environment.insert(QStringLiteral("BW_SESSION"), m_session);
        }
        process->setProcessEnvironment(environment);

        connect(process, &QProcess::started, process, [process, stdinData]() {
            if (!stdinData.isEmpty()) {
                process->write(stdinData);
                process->write("\n");
            }
            process->closeWriteChannel();
        });

        connect(process, &QProcess::finished, this, [this, process, callback](int exitCode, QProcess::ExitStatus) {
            const QByteArray stdoutData = process->readAllStandardOutput();
            const QByteArray stderrData = process->readAllStandardError();
            process->deleteLater();
            setBusy(false);
            callback(exitCode, stdoutData, stderrData);
        });

        connect(process, &QProcess::errorOccurred, this, [this, process, callback](QProcess::ProcessError) {
            const QByteArray stderrData = process->errorString().toUtf8();
            process->deleteLater();
            setBusy(false);
            callback(-1, {}, stderrData);
        });

        process->start();
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
        if (verifier != m_pinVerifier) {
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

    QVariantList m_items;
    QString m_state = QStringLiteral("loading");
    QString m_statusText = i18n("Checking Bitwarden CLI status…");
    QString m_userEmail;
    QString m_session;
    QByteArray m_pinSalt;
    QByteArray m_pinNonce;
    QByteArray m_wrappedSession;
    QByteArray m_pinVerifier;
    int m_invalidPinAttempts = 0;
    bool m_busy = false;
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
    QGuiApplication::setDesktopFileName(QStringLiteral("org.kwarden.KWarden"));

    QGuiApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("kwarden");
    KAboutData about(QStringLiteral("kwarden"),
                     i18n("KWarden"),
                     QStringLiteral("0.1.0"),
                     i18n("KDE native frontend for Bitwarden CLI"),
                     KAboutLicense::GPL_V3);
    KAboutData::setApplicationData(about);

    ClipboardBridge clipboardBridge;
    BwCliProvider vaultProvider;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("clipboardBridge"), &clipboardBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("vaultProvider"), &vaultProvider);
    engine.loadFromModule(QStringLiteral("org.kwarden"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (const int quitDelay = quitAfterMs(argc, argv); quitDelay > 0) {
        QTimer::singleShot(quitDelay, &app, []() {
            QCoreApplication::exit(0);
        });
    }

    QTimer::singleShot(0, &vaultProvider, &BwCliProvider::refresh);

    return app.exec();
}

#include "main.moc"
