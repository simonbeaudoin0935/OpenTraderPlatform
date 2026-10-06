#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "CONSTANTS.h"

class StartupRestoreTests : public QObject
{
    Q_OBJECT

  private slots:
    void defersRestorationUntilUnlockCompletes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString root = directory.path();
        QVERIFY(QDir().mkpath(root + "/bin"));
        QVERIFY(QDir().mkpath(root + "/config"));
        const QString readyPath = root + "/unlock-ready";
        const QString releasePath = root + "/unlock-release";
        QFile helper(root + "/bin/ykman");
        QVERIFY(helper.open(QIODevice::WriteOnly));
        const QByteArray script = "#!/bin/sh\n"
                                  "touch \"$OTP_TEST_UNLOCK_READY\"\n"
                                  "while [ ! -f \"$OTP_TEST_UNLOCK_RELEASE\" ]; do sleep 0.05; done\n"
                                  "echo 'Touch your YubiKey' >&2\n"
                                  "echo '1111111111111111111111111111111111111111'\n";
        QCOMPARE(helper.write(script), script.size());
        helper.close();
        QVERIFY(helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, root + "/config");
        QSettings settings(QSettings::NativeFormat, QSettings::UserScope, QString(), "OpenTraderPlatform");
        settings.setValue(CredentialStorageConstants::SETTINGS_KEY, "YubiKey");
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
        QVERIFY(QDir().mkpath(root + "/state/OpenTraderPlatform"));
        QSettings appState(root + "/state/OpenTraderPlatform/AppState.ini", QSettings::IniFormat);
        appState.setValue("Replay/Active", true);
        appState.setValue("Replay/Date", "2026-10-02");
        appState.setValue("Replay/StartTime", "07:00:00");
        appState.sync();
        QCOMPARE(appState.status(), QSettings::NoError);

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("XDG_CONFIG_HOME", root + "/config");
        environment.insert("XDG_STATE_HOME", root + "/state");
        environment.insert("XDG_DATA_HOME", root + "/data");
        environment.insert("XDG_CACHE_HOME", root + "/cache");
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        environment.insert("PATH", root + "/bin:" + environment.value("PATH"));
        environment.insert("OTP_TEST_UNLOCK_READY", readyPath);
        environment.insert("OTP_TEST_UNLOCK_RELEASE", releasePath);
        QProcess platform;
        platform.setProcessEnvironment(environment);
        platform.setProcessChannelMode(QProcess::MergedChannels);
        platform.start(QString::fromLocal8Bit(OTP_PLATFORM_EXECUTABLE), {});
        const auto cleanup = qScopeGuard(
            [&]()
            {
                if (platform.state() != QProcess::NotRunning)
                {
                    platform.terminate();
                    if (!platform.waitForFinished(3000))
                    {
                        platform.kill();
                        platform.waitForFinished(3000);
                    }
                }
            });
        QVERIFY(platform.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(readyPath), 10000);

        // Exercise the nested unlock event loop beyond both former constructor timers.
        QTest::qWait(800);
        QByteArray output = platform.readAll();
        QCOMPARE(platform.state(), QProcess::Running);
        QVERIFY2(!output.contains("restoring state via fallback timer"), output.constData());

        QFile release(releasePath);
        QVERIFY(release.open(QIODevice::WriteOnly));
        release.close();
        QTRY_VERIFY_WITH_TIMEOUT((output += platform.readAll()).contains("restoring state via fallback timer"), 10000);
        QVERIFY2(!output.contains("Startup YubiKey unlock failed"), output.constData());
        QTRY_VERIFY_WITH_TIMEOUT((output += platform.readAll()).contains("skipping replay restore"), 10000);
    }
};

QTEST_GUILESS_MAIN(StartupRestoreTests)
#include "StartupRestoreTests.moc"
