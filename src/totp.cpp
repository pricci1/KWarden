#include "totp.h"

#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QUrl>
#include <QUrlQuery>

namespace
{
std::optional<QByteArray> base32Decode(QString value)
{
    value.remove(QLatin1Char(' '));
    value.remove(QLatin1Char('-'));
    value.remove(QLatin1Char('='));
    value = value.toUpper();

    QByteArray output;
    int buffer = 0;
    int bitsLeft = 0;

    for (const QChar character : value) {
        int decoded = -1;
        const char c = character.toLatin1();
        if (c >= 'A' && c <= 'Z') {
            decoded = c - 'A';
        } else if (c >= '2' && c <= '7') {
            decoded = c - '2' + 26;
        } else {
            return std::nullopt;
        }

        buffer = (buffer << 5) | decoded;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            output.append(char((buffer >> (bitsLeft - 8)) & 0xff));
            bitsLeft -= 8;
        }
    }

    if (output.isEmpty()) {
        return std::nullopt;
    }
    return output;
}

std::optional<QCryptographicHash::Algorithm> algorithmFromString(const QString &value)
{
    const QString normalized = value.toUpper();
    if (normalized.isEmpty() || normalized == QStringLiteral("SHA1")) {
        return QCryptographicHash::Sha1;
    }
    if (normalized == QStringLiteral("SHA256")) {
        return QCryptographicHash::Sha256;
    }
    if (normalized == QStringLiteral("SHA512")) {
        return QCryptographicHash::Sha512;
    }
    return std::nullopt;
}

QByteArray counterBytes(quint64 counter)
{
    QByteArray bytes(8, Qt::Uninitialized);
    for (int i = 7; i >= 0; --i) {
        bytes[i] = char(counter & 0xff);
        counter >>= 8;
    }
    return bytes;
}
}

std::optional<TotpParameters> Totp::parse(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    TotpParameters parameters;
    QString secretText = trimmed;

    const QUrl url(trimmed);
    if (url.scheme() == QStringLiteral("otpauth")) {
        if (url.host() != QStringLiteral("totp")) {
            return std::nullopt;
        }

        const QUrlQuery query(url);
        secretText = query.queryItemValue(QStringLiteral("secret"));
        const std::optional<QCryptographicHash::Algorithm> algorithm = algorithmFromString(query.queryItemValue(QStringLiteral("algorithm")));
        if (!algorithm) {
            return std::nullopt;
        }
        parameters.algorithm = *algorithm;

        bool ok = false;
        const int digits = query.queryItemValue(QStringLiteral("digits")).toInt(&ok);
        if (ok) {
            parameters.digits = digits;
        }

        ok = false;
        const int period = query.queryItemValue(QStringLiteral("period")).toInt(&ok);
        if (ok) {
            parameters.period = period;
        }
    }

    if (parameters.digits < 6 || parameters.digits > 8 || parameters.period <= 0) {
        return std::nullopt;
    }

    const std::optional<QByteArray> secret = base32Decode(secretText);
    if (!secret) {
        return std::nullopt;
    }

    parameters.secret = *secret;
    return parameters;
}

QString Totp::code(const TotpParameters &parameters, quint64 unixTimeSeconds)
{
    if (parameters.secret.isEmpty() || parameters.digits < 6 || parameters.digits > 8 || parameters.period <= 0) {
        return {};
    }

    const QByteArray hmac = QMessageAuthenticationCode::hash(counterBytes(unixTimeSeconds / quint64(parameters.period)), parameters.secret, parameters.algorithm);
    if (hmac.size() < 20) {
        return {};
    }

    const int offset = hmac.at(hmac.size() - 1) & 0x0f;
    if (offset + 3 >= hmac.size()) {
        return {};
    }

    const quint32 binary = ((quint32(hmac.at(offset)) & 0x7f) << 24) | ((quint32(hmac.at(offset + 1)) & 0xff) << 16)
        | ((quint32(hmac.at(offset + 2)) & 0xff) << 8) | (quint32(hmac.at(offset + 3)) & 0xff);

    quint32 divisor = 1;
    for (int i = 0; i < parameters.digits; ++i) {
        divisor *= 10;
    }

    return QStringLiteral("%1").arg(binary % divisor, parameters.digits, 10, QLatin1Char('0'));
}

QString Totp::currentCode(const QString &value)
{
    const std::optional<TotpParameters> parameters = parse(value);
    if (!parameters) {
        return {};
    }
    return grouped(code(*parameters, quint64(QDateTime::currentSecsSinceEpoch())));
}

QString Totp::grouped(const QString &code)
{
    if (code.size() <= 4) {
        return code;
    }

    QString groupedCode;
    for (int i = 0; i < code.size(); ++i) {
        if (i > 0 && (code.size() - i) % 3 == 0) {
            groupedCode.append(QLatin1Char(' '));
        }
        groupedCode.append(code.at(i));
    }
    return groupedCode;
}
