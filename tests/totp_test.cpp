#include "totp.h"

#include <QTest>

class TotpTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void generatesRfc6238Sha1Vectors();
    void parsesOtpauthDefaults();
    void parsesOtpauthOptions();
    void rejectsInvalidInput();
    void groupsDisplayCode();
};

void TotpTest::generatesRfc6238Sha1Vectors()
{
    const std::optional<TotpParameters> parameters = Totp::parse(QStringLiteral("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"));
    QVERIFY(parameters.has_value());
    QCOMPARE(Totp::code(*parameters, 59), QStringLiteral("287082"));
    QCOMPARE(Totp::code(*parameters, 1111111109), QStringLiteral("081804"));
    QCOMPARE(Totp::code(*parameters, 1111111111), QStringLiteral("050471"));
    QCOMPARE(Totp::code(*parameters, 1234567890), QStringLiteral("005924"));
    QCOMPARE(Totp::code(*parameters, 2000000000), QStringLiteral("279037"));
    QCOMPARE(Totp::code(*parameters, 20000000000), QStringLiteral("353130"));
}

void TotpTest::parsesOtpauthDefaults()
{
    const std::optional<TotpParameters> parameters = Totp::parse(QStringLiteral("otpauth://totp/Example:alice?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&issuer=Example"));
    QVERIFY(parameters.has_value());
    QCOMPARE(parameters->algorithm, QCryptographicHash::Sha1);
    QCOMPARE(parameters->digits, 6);
    QCOMPARE(parameters->period, 30);
    QCOMPARE(Totp::code(*parameters, 59), QStringLiteral("287082"));
}

void TotpTest::parsesOtpauthOptions()
{
    const std::optional<TotpParameters> sha256 = Totp::parse(QStringLiteral("otpauth://totp/alice?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZA&algorithm=SHA256&digits=8&period=30"));
    QVERIFY(sha256.has_value());
    QCOMPARE(sha256->algorithm, QCryptographicHash::Sha256);
    QCOMPARE(sha256->digits, 8);
    QCOMPARE(sha256->period, 30);
    QCOMPARE(Totp::code(*sha256, 59), QStringLiteral("46119246"));

    const std::optional<TotpParameters> sha512 = Totp::parse(QStringLiteral("otpauth://totp/alice?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNA&algorithm=SHA512&digits=8"));
    QVERIFY(sha512.has_value());
    QCOMPARE(sha512->algorithm, QCryptographicHash::Sha512);
    QCOMPARE(Totp::code(*sha512, 59), QStringLiteral("90693936"));
}

void TotpTest::rejectsInvalidInput()
{
    QVERIFY(!Totp::parse(QString()).has_value());
    QVERIFY(!Totp::parse(QStringLiteral("not base32!" )).has_value());
    QVERIFY(!Totp::parse(QStringLiteral("otpauth://hotp/alice?secret=GEZDGNBVGY3TQOJQ")).has_value());
    QVERIFY(!Totp::parse(QStringLiteral("otpauth://totp/alice?secret=GEZDGNBVGY3TQOJQ&algorithm=MD5")).has_value());
    QVERIFY(!Totp::parse(QStringLiteral("otpauth://totp/alice?secret=GEZDGNBVGY3TQOJQ&digits=5")).has_value());
    QVERIFY(!Totp::parse(QStringLiteral("otpauth://totp/alice?secret=GEZDGNBVGY3TQOJQ&period=0")).has_value());
}

void TotpTest::groupsDisplayCode()
{
    QCOMPARE(Totp::grouped(QStringLiteral("123456")), QStringLiteral("123 456"));
    QCOMPARE(Totp::grouped(QStringLiteral("12345678")), QStringLiteral("12 345 678"));
}

QTEST_MAIN(TotpTest)

#include "totp_test.moc"
