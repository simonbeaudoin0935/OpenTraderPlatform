#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

#include "TestRunUpDetector.h"
#include "TSClient.h"
#include "RunUpDetector.h"
#include "Logging.h"

static TSClient* client;
static BarCache* barCache;
static RunUpDetector* detector;

//static const QString symbol = "TIVC";
//static const QDate   date(2025, 4, 3); // Wednesday April 3rd

static const QString symbol = "UPXI";
static const QDate date(2025, 4, 21); // Wednesday April 3rd

// will be called to create a global test data table.
void TestRunUpDetector::initTestCase_data() {}

// will be called before the first test function is executed.
void TestRunUpDetector::initTestCase()
{

    // manually expire the token to force a refresh on startup
    {
        AuthToken savedAuthToken = AuthToken::loadFromSettings();

        QString tokenStr = savedAuthToken.toString();
        QString obfuscated = tokenStr.size() > 10 ? tokenStr.left(10) + "***" : tokenStr + "***";
        qDebug() << "Token details:" << obfuscated;

        // This test suite neet a valid token to be present in the settings.
        QVERIFY(savedAuthToken.isValid());

        // The first thing we are going to test is that the client is able to
        // start with an expired token and refresh it.
        // If the present token is not expired, we will artificially set it to expired
        if (!savedAuthToken.isExpired())
        {
            qInfo() << "Token is not expired, setting it to expired";

            AuthToken expiredToken = AuthToken(savedAuthToken.getAccessToken(),
                                               savedAuthToken.getRefreshToken(),
                                               savedAuthToken.getIdToken(),
                                               savedAuthToken.getTokenType(),
                                               savedAuthToken.getScope(),
                                               savedAuthToken.getExpiresIn(),
                                               QDateTime::currentDateTime().addSecs(-3600));

            QVERIFY(AuthToken::storeToSettings(expiredToken));
        }
        else
        {
            qInfo() << "Token is expired, no need to set it to expired";
        }
    }

    qInfo() << "Start of test suite";

    QThread::currentThread()->setObjectName("UnitTestThread");

    client = TSClient::getInstancePtr(); // ***** First time to do a getInstance, this will call the constructor

    bool triggered;

    QSignalSpy authStateSpy(client,
                            &TSClient::authStateChanged); // Create signal spies to monitor authentication signals

    QVERIFY(client->isCleanedUp());
    QVERIFY(!client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // ****** This will kickstart the refresh logic
    client->start();

    // Wait 1ms, just to make sure the client thread has started and
    QTest::qWait(1);

    // Verify the refresh has started
    QVERIFY(!client->isAuthenticated());
    QVERIFY(client->isAuthInProgress()); // The client will immediately initiate a refresh


    // Wait for authStateChanged signal to be fired
    // 2 second is generous for one refresh request round trip
    // When debugging, 2 seconds wasnt enough. Bumbed it to 4.
    // This is because the start of the suite is slow as the debugger sets up
    // the maaaany runtime thangs the app links to
    triggered = authStateSpy.wait(8000);

    QVERIFY(triggered);

    // Logically, only one signal must be emited
    QCOMPARE(authStateSpy.count(), 1);

    // Authentication completed
    bool success = authStateSpy.first().at(0).toBool();
    QString reason = authStateSpy.first().at(1).toString();

    qInfo() << "Refresh reply : " << reason;
    authStateSpy.removeFirst();

    QVERIFY(success);

    QVERIFY(client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // Wait just a little bit, for some reason this test thread outruns the housekeeping done in the TSClient that cleans the serviced
    // replies. Otherwise, the following isCleanedUp() triggers because the refreshTokenReply is not flushed from the map<>
    QTest::qWait(100);

    // Verify no resources were leaked
    QVERIFY(client->isCleanedUp());

    barCache = new BarCache(symbol);
    detector = new RunUpDetector(barCache);
}

// Will be called before each test function is executed.
void TestRunUpDetector::init()
{
    // QTest installs its own message handler which overrides our colored logging.
    // Reinstall our custom colored message handler before each test to ensure
    // proper colored output during test execution.
    reinstallColoredMessageHandler();

    QLoggingCategory::setFilterRules("TSClient.debug=true");
    QLoggingCategory::setFilterRules("BarCache.debug=true");
}

// Will be called after every test function.
void TestRunUpDetector::cleanup() {}

void TestRunUpDetector::testPriorDayAfterMarket()
{
    detector->start(date, 30);

    for (size_t i = 0; i != 200; i++)
    {
        detector->computeNextCandle();
    }

    QTest::qWait(5000);
}
