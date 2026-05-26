#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QString>

#include <optional>

struct TotpParameters {
    QByteArray secret;
    QCryptographicHash::Algorithm algorithm = QCryptographicHash::Sha1;
    int digits = 6;
    int period = 30;
};

class Totp
{
public:
    static std::optional<TotpParameters> parse(const QString &value);
    static QString code(const TotpParameters &parameters, quint64 unixTimeSeconds);
    static int secondsRemaining(const TotpParameters &parameters, quint64 unixTimeSeconds);
    static int currentSecondsRemaining(const QString &value);
    static QString currentCode(const QString &value);
    static QString grouped(const QString &code);
};
