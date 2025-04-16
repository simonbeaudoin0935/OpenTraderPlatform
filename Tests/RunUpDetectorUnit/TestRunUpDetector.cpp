#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

#include "TestRunUpDetector.h"
#include "TSClient.h"
#include "StockRunUpDetector.h"

static TSClient* client;
static StockRunUpDetector* detector;

static const QString symbol = "TIVC";
static const QDate   date(2025, 4, 3); // Wednesday April 3rd

// will be called to create a global test data table.
void TestRunUpDetector::initTestCase_data()
{
}

// will be called before the first test function is executed.
void TestRunUpDetector::initTestCase() {
    qInfo() << "Start of test suite";

    QThread::currentThread()->setObjectName("UnitTestThread");

    client = TSClient::getInstancePtr(); // ***** First time to do a getInstance, this will call the constructor

    bool triggered;

    QSignalSpy authStateSpy(client, &TSClient::authStateChanged); // Create signal spies to monitor authentication signals

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
    triggered = authStateSpy.wait(4000);

    QVERIFY(triggered);

    // Logically, only one signal must be emited
    QCOMPARE(authStateSpy.count(), 1);

    // Authentication completed
    bool success   = authStateSpy.first().at(0).toBool();
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

    detector = new StockRunUpDetector(symbol);
}

// Will be called before each test function is executed.
void TestRunUpDetector::init()
{

}

// Will be called after every test function.
void TestRunUpDetector::cleanup() {

}

void TestRunUpDetector::testPriorDayAfterMarket()
{
    detector->start(date);


    QTest::qWait(5000);
}

