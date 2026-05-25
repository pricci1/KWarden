#include <QClipboard>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

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
    QGuiApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("kwarden");
    KAboutData about(QStringLiteral("kwarden"),
                     i18n("KWarden"),
                     QStringLiteral("0.1.0"),
                     i18n("KDE native frontend for Bitwarden CLI"),
                     KAboutLicense::GPL_V3);
    KAboutData::setApplicationData(about);

    ClipboardBridge clipboardBridge;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("clipboardBridge"), &clipboardBridge);
    engine.loadFromModule(QStringLiteral("org.kwarden"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (const int quitDelay = quitAfterMs(argc, argv); quitDelay > 0) {
        QTimer::singleShot(quitDelay, &app, []() {
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}

#include "main.moc"
