#include <QJsonObject>
#include <QSettings>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QCoreApplication>
#include <QUuid>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

#include "AuthHandler.h"
#include "AuthToken.h"
#include "SecureStorage.h"
#include "YubiKeyStorage.h"

namespace
{
    QStringList g_messages;

    void captureMessage(QtMsgType, const QMessageLogContext&, const QString& p_message)
    {
        g_messages.append(p_message);
    }

    class TestAuthHandler : public AuthHandler
    {
      public:
        TestAuthHandler()
        {
            m_expectedState = "expected-state-sentinel";
        }

      protected:
        bool promptForCredentials(QString&, QString&) override
        {
            return false;
        }
        void showAuthUrl(const QString&) override {}
        void showError(const QString&, const QString&) override {}
    };

    QJsonObject tokenResponse()
    {
        return {{"access_token", "access-sentinel"},
                {"id_token", "id-sentinel"},
                {"token_type", AuthConstants::EXPECTED_TOKEN_TYPE},
                {"scope", AuthConstants::EXPECTED_SCOPES.join(' ')},
                {"expires_in", AuthConstants::EXPECTED_EXPIRES_IN}};
    }
} // namespace

class OAuthSecurityTests : public QObject
{
    Q_OBJECT

  private slots:
    void encryptedVaultRoundTrip()
    {
        const QByteArray key(CredentialStorageConstants::KEY_SIZE, 'k');
        const QByteArray challenge(CredentialStorageConstants::CHALLENGE_SIZE, 'c');
        const QByteArray plaintext("{\"TradeStation\":{\"refresh_token\":\"test-sentinel\"}}");
        QString error;
        const QByteArray file = YubiKeyStorage::encrypt(plaintext, key, challenge, error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!file.contains("test-sentinel"));
        QCOMPARE(YubiKeyStorage::decrypt(file, key, error), plaintext);
        QVERIFY(error.isEmpty());
        const QByteArray second = YubiKeyStorage::encrypt(plaintext, key, challenge, error);
        QVERIFY(file != second);
        QByteArray tampered = file;
        tampered[tampered.size() - 1] ^= 1;
        QVERIFY(YubiKeyStorage::decrypt(tampered, key, error).isEmpty());
        QVERIFY(!error.isEmpty());
        tampered = file;
        tampered[12] ^= 1;
        QVERIFY(YubiKeyStorage::decrypt(tampered, key, error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(YubiKeyStorage::decrypt(file, QByteArray(key.size(), 'x'), error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(YubiKeyStorage::decrypt(file.left(20), key, error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void initTestCase()
    {
        QVERIFY(m_configDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_configDir.path());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_configDir.path());
        if (qEnvironmentVariableIsSet("OTP_TEST_YUBIKEY"))
        {
            qputenv("XDG_CONFIG_HOME", m_configDir.path().toUtf8());
            QSettings settings;
            settings.setValue(CredentialStorageConstants::SETTINGS_KEY, "YubiKey");
            settings.sync();
        }
        if (qEnvironmentVariableIsSet("OTP_TEST_NO_KEYRING"))
        {
            qputenv("DBUS_SESSION_BUS_ADDRESS", ("unix:path=" + m_configDir.path() + "/missing-bus").toUtf8());
        }
    }

    void yubiKeySession()
    {
        if (!qEnvironmentVariableIsSet("OTP_TEST_YUBIKEY"))
        {
            QSKIP("Run in the dedicated simulated-YubiKey process");
        }
        const QByteArray oldPath = qgetenv("PATH");
        const QString scriptPath = m_configDir.path() + "/ykman";
        const QString counterPath = m_configDir.path() + "/calls";
        QFile script(scriptPath);
        QVERIFY(script.open(QIODevice::WriteOnly));
        const QByteArray code = "#!/bin/sh\n"
                                "echo call >> \"" +
                                counterPath.toUtf8() +
                                "\"\n"
                                "echo 'Touch your YubiKey...' >&2\n"
                                "echo '00112233445566778899aabbccddeeff00112233'\n";
        QByteArray noTouch = code;
        noTouch.replace("echo 'Touch your YubiKey...' >&2\n", "");
        QCOMPARE(script.write(noTouch), noTouch.size());
        script.close();
        QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        qputenv("PATH", m_configDir.path().toUtf8() + ":" + oldPath);
        const auto cleanup = qScopeGuard([&]() { qputenv("PATH", oldPath); });
        SecureStorage storage;
        QCOMPARE(SecureStorage::activeBackend(), SecureStorage::Backend::YubiKey);
        QVERIFY(!storage.storeValuesSync("TradeStation", {{"refresh_token", "refresh-test"}}));
        QVERIFY(!QFile::exists(YubiKeyStorage::filePath()));
        QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(script.write(code), code.size());
        script.close();
        QString unlockError;
        QVERIFY2(SecureStorage::unlockYubiKey(unlockError), qPrintable(unlockError));
        QVERIFY(storage.storeValuesSync("TradeStation", {{"refresh_token", "refresh-test"}}));
        QVERIFY(storage.storeValuesSync("Databento", {{"api_key", "databento-test"}}));
        QCOMPARE(storage.retrieveValuesSync("TradeStation", {"refresh_token"}).value("refresh_token"),
                 QString("refresh-test"));
        QFile vault(YubiKeyStorage::filePath());
        QVERIFY(vault.open(QIODevice::ReadOnly));
        const QByteArray contents = vault.readAll();
        QVERIFY(!contents.contains("refresh-test"));
        QVERIFY(!contents.contains("databento-test"));
        QVERIFY(!(vault.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther)));
        // Once unlocked, even removing the simulated device must not interrupt refresh persistence.
        QVERIFY(QFile::remove(scriptPath));
        QVERIFY(storage.storeValuesSync("TradeStation", {{"refresh_token", "rotated-test"}}));
        QCOMPARE(storage.retrieveValuesSync("TradeStation", {"refresh_token"}).value("refresh_token"),
                 QString("rotated-test"));
        QFile counter(counterPath);
        QVERIFY(counter.open(QIODevice::ReadOnly));
        QCOMPARE(counter.readAll().count("call"), 2);
        QVERIFY(SecureStorage::configureBackend(SecureStorage::Backend::OSKeyring));
        QCOMPARE(SecureStorage::configuredBackend(), SecureStorage::Backend::OSKeyring);
        QCOMPARE(SecureStorage::activeBackend(), SecureStorage::Backend::YubiKey);
        vault.close();
        QVERIFY2(SecureStorage::resetYubiKey(unlockError), qPrintable(unlockError));
        QVERIFY(!QFile::exists(YubiKeyStorage::filePath()));
        QVERIFY(!storage.storeValuesSync("TradeStation", {{"refresh_token", "must-not-return"}}));
        QVERIFY(!SecureStorage::unlockYubiKey(unlockError));
        QVERIFY(unlockError.contains("Restart"));
        QVERIFY(!QFile::exists(YubiKeyStorage::filePath()));
        QVERIFY2(SecureStorage::resetYubiKey(unlockError), qPrintable(unlockError));
    }

    void yubiKeyLockedReset()
    {
        if (!qEnvironmentVariableIsSet("OTP_TEST_YUBIKEY"))
        {
            QSKIP("Run in the dedicated simulated-YubiKey process");
        }
        const QString path = YubiKeyStorage::filePath();
        QVERIFY(QDir().mkpath(path));
        QString error;
        QVERIFY(!SecureStorage::resetYubiKey(error));
        QVERIFY(!error.isEmpty());
        QVERIFY(QDir(path).exists());
        QVERIFY(QDir().rmdir(path));
        QFile vault(path);
        QVERIFY(vault.open(QIODevice::WriteOnly));
        QCOMPARE(vault.write("unreadable-vault"), qint64(16));
        vault.close();
        SecureStorage storage;
        QVERIFY(storage.retrieveValuesSync("TradeStation", {"refresh_token"}).value("refresh_token").isEmpty());
        QVERIFY(QFile::exists(path));
        QVERIFY2(SecureStorage::resetYubiKey(error), qPrintable(error));
        QVERIFY(!QFile::exists(path));
        QVERIFY(!storage.storeValuesSync("Databento", {{"api_key", "must-not-return"}}));
        QVERIFY(!QFile::exists(path));
    }

    void refreshTokenSelection_data()
    {
        QTest::addColumn<QJsonObject>("response");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<bool>("valid");
        QJsonObject response = tokenResponse();
        QTest::newRow("omitted") << response << QString("previous-sentinel") << true;
        response["refresh_token"] = "rotated-sentinel";
        QTest::newRow("rotated") << response << QString("rotated-sentinel") << true;
        response["refresh_token"] = "";
        QTest::newRow("empty-replacement") << response << QString() << false;
        response["refresh_token"] = QJsonValue::Null;
        QTest::newRow("null-replacement") << response << QString() << false;
        response["refresh_token"] = "rotated-sentinel";
        response.remove("access_token");
        QTest::newRow("invalid-access") << response << QString("rotated-sentinel") << false;
    }

    void refreshTokenSelection()
    {
        QFETCH(QJsonObject, response);
        QFETCH(QString, expected);
        QFETCH(bool, valid);
        const AuthToken token = AuthToken::receiveRefreshedAuthToken(response, "previous-sentinel");
        QCOMPARE(token.getRefreshToken(), expected);
        QCOMPARE(token.isValid(), valid);
    }

    void refreshValidationAllowsRotation()
    {
        QJsonObject response = tokenResponse();
        QVERIFY(AuthToken::receiveAuthToken(response).isValidRefreshedToken());
        response["refresh_token"] = "rotated-sentinel";
        QVERIFY(AuthToken::receiveAuthToken(response).isValidRefreshedToken());
    }

    void callbackLoggingDoesNotExposeQueries()
    {
        g_messages.clear();
        const QtMessageHandler previous = qInstallMessageHandler(captureMessage);
        TestAuthHandler handler;
        QTcpServer server;
        const bool listening = server.listen(QHostAddress::LocalHost, 0);
        QTcpSocket client;
        bool wired = false;
        QObject::connect(&server,
                         &QTcpServer::newConnection,
                         &handler,
                         [&]()
                         {
                             QTcpSocket* socket = server.nextPendingConnection();
                             wired =
                                 QObject::connect(socket, SIGNAL(readyRead()), &handler, SLOT(handleSocketReadyRead()));
                         });
        QSignalSpy connectionSpy(&server, &QTcpServer::newConnection);
        client.connectToHost(QHostAddress::LocalHost, server.serverPort());
        QSignalSpy responseSpy(&client, &QTcpSocket::readyRead);
        const bool connected = connectionSpy.wait();
        client.write("GET /callback?code=code-sentinel&state=state-sentinel HTTP/1.1\r\n"
                     "Host: localhost\r\n\r\n");
        const bool responseReceived = responseSpy.wait();
        const QByteArray response = client.readAll();
        qInstallMessageHandler(previous);
        QVERIFY(listening);
        QVERIFY(connected);
        QVERIFY(wired);
        QVERIFY(responseReceived);
        QVERIFY(response.startsWith("HTTP/1.1 403"));
        const QString messages = g_messages.join('\n');
        QVERIFY(!messages.contains("code-sentinel"));
        QVERIFY(!messages.contains("state-sentinel"));
        QVERIFY(!messages.contains("expected-state-sentinel"));
    }

    void legacyStorageIsDiscarded()
    {
        QSettings legacy(QSettings::IniFormat, QSettings::UserScope, "OpenTraderPlatform", "SecureStorage");
        legacy.setValue("TradeStation/client_secret", "obsolete-sentinel");
        legacy.setValue("Databento/api_key", "obsolete-sentinel");
        legacy.sync();
        SecureStorage storage;
        legacy.sync();
        QVERIFY(legacy.allKeys().isEmpty());
    }

    void keyringUnavailable()
    {
        if (!qEnvironmentVariableIsSet("OTP_TEST_NO_KEYRING"))
        {
            QSKIP("Run in the dedicated unavailable-keyring test process");
        }
        SecureStorage storage;
        const QString service = "OpenTraderPlatform-security-test-" + QUuid::createUuid().toString();
        QVERIFY(!storage.storeValuesSync(service, {{"test-key", "test-secret"}}, 1000));
        QVERIFY(storage.retrieveValuesSync(service, {"test-key"}, 1000).isEmpty());

        QJsonObject response = tokenResponse();
        response["refresh_token"] = "refresh-sentinel";
        TestAuthHandler handler;
        QSignalSpy authSpy(&handler, &AuthHandler::authFinished);
        QVERIFY(QMetaObject::invokeMethod(&handler, "handleTokenResponse", Q_ARG(QJsonObject, response)));
        QCOMPARE(authSpy.size(), 1);
        QVERIFY(!authSpy.at(0).at(0).toBool());
        QVERIFY(authSpy.at(0).at(2).toString().contains("OS keyring"));

        bool completed = false;
        bool success = true;
        storage.storeValue(service,
                           "async-key",
                           "test-secret",
                           [&](bool p_success)
                           {
                               success = p_success;
                               completed = true;
                           });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 5000);
        QVERIFY(!success);
        completed = false;
        QString retrieved = "not-completed";
        storage.retrieveValue(service,
                              "async-key",
                              [&](const QString& p_value)
                              {
                                  retrieved = p_value;
                                  completed = true;
                              });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 5000);
        QVERIFY(retrieved.isEmpty());
        QSettings legacy(QSettings::IniFormat, QSettings::UserScope, "OpenTraderPlatform", "SecureStorage");
        QVERIFY(legacy.allKeys().isEmpty());
        QSettings fallback(service);
        QVERIFY(fallback.allKeys().isEmpty());
    }

    void keyringRoundTrip()
    {
        if (!qEnvironmentVariableIsSet("OTP_TEST_KEYRING_ROUNDTRIP"))
        {
            QSKIP("Run the dedicated native-keyring integration test");
        }
        SecureStorage storage;
        const QString service = "OpenTraderPlatform-security-test-" + QUuid::createUuid().toString();
        const auto cleanup =
            qScopeGuard([&]() { storage.deleteValuesSync(service, {"first", "second", "async-key"}); });
        const QMap<QString, QString> values = {{"first", "first-sentinel"}, {"second", "second-sentinel"}};
        QVERIFY(storage.storeValuesSync(service, values));
        QCOMPARE(storage.retrieveValuesSync(service, values.keys()), values);
        QVERIFY(storage.deleteValuesSync(service, values.keys()));
        QVERIFY(storage.retrieveValuesSync(service, values.keys()).isEmpty());

        bool completed = false;
        bool success = false;
        storage.storeValue(service,
                           "async-key",
                           "async-sentinel",
                           [&](bool p_success)
                           {
                               success = p_success;
                               completed = true;
                           });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 10000);
        QVERIFY(success);
        QCOMPARE(storage.retrieveValuesSync(service, {"async-key"}).value("async-key"), QString("async-sentinel"));
        completed = false;
        storage.deleteValue(service,
                            "async-key",
                            [&](bool p_success)
                            {
                                success = p_success;
                                completed = true;
                            });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 10000);
        QVERIFY(success);
        QVERIFY(storage.retrieveValuesSync(service, {"async-key"}).isEmpty());
    }

    void workerThreadKeyringReads()
    {
        // Reproduce startup: GUI-thread reads initialize QtKeychain, then a worker performs a read.
        SecureStorage storage;
        const QString service = "OpenTraderPlatform-security-test-" + QUuid::createUuid().toString();
        QVERIFY(storage.retrieveValuesSync(service, {"absent-key"}).isEmpty());
        class ReaderThread : public QThread
        {
          public:
            QString m_service;
            QMap<QString, QString> m_values;
            void run() override
            {
                SecureStorage workerStorage;
                m_values = workerStorage.retrieveValuesSync(m_service, {"absent-key"});
            }
        };
        ReaderThread worker;
        worker.m_service = service;
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 15000);
        QVERIFY(worker.wait());
        QVERIFY(worker.m_values.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QTest::qWait(50);
    }

  private:
    QTemporaryDir m_configDir;
};

QTEST_GUILESS_MAIN(OAuthSecurityTests)
#include "OAuthSecurityTests.moc"
