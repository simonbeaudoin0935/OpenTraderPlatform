#include <QDebug>
#include "TestTradeStationClient.h"
#include "../../Clients/TradeStationClient/TradeStationClient.h"
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

// will be called to create a global test data table.
void TestTradeStationClient::initTestCase_data()
{
    AuthToken savedAuthToken = AuthToken::loadFromSettings();

    qDebug() << "Token details:" << savedAuthToken.toString();

    // This test suite neet a valid token to be present in the settings.
    QVERIFY(savedAuthToken.isValid());

    // The first thing we are going to test is that the client is able to
    // start with an expired token and refresh it.
    // If the present token is not expired, we will artificially set it to expired
    if(!savedAuthToken.isExpired()) {
        qInfo() << "Token is not expired, setting it to expired";

        AuthToken expiredToken = AuthToken(savedAuthToken.getAccessToken(),
                                           savedAuthToken.getRefreshToken(),
                                           savedAuthToken.getIdToken(),
                                           savedAuthToken.getTokenType(),
                                           savedAuthToken.getScope(),
                                           savedAuthToken.getExpiresIn(),
                                           QDateTime::currentDateTime().addSecs(-3600));

        QVERIFY(AuthToken::storeToSettings(expiredToken));
    } else {
        qInfo() << "Token is expired, no need to set it to expired";
    }

}

// will be called before the first test function is executed.
void TestTradeStationClient::initTestCase() {
    QLoggingCategory::setFilterRules("TradeStationClient.debug=true");

    qInfo() << "Start of test suite";
}

// Will be called before each test function is executed.
void TestTradeStationClient::init()
{
    // Make sure the debug prints are enabled
    QLoggingCategory::setFilterRules("TradeStationClient.debug=true");

    // A test could change the logging behavior to suppress too much logging,
    // but it will be reset before each test
}

// Will be called after every test function.
void TestTradeStationClient::cleanup() {

}

void TestTradeStationClient::testRefreshSyncAccessToken()
{
    bool triggered;
    TradeStationClient& client = TradeStationClient::getInstance(); // First time do a getInstance, this will call the constructor
    QSignalSpy authStateSpy(&client, &TradeStationClient::authStateChanged); // Create signal spies to monitor authentication signals

    // Critical to do, will start the thread
    client.start();

    // Verify initial state
    QVERIFY(!client.isAuthenticated());
    QVERIFY(client.isAuthInProgress()); // The client will immediately initiate a refresh
    QVERIFY(client.isCleanedUp());

    // Wait for the first authStateChanged signal that happens
    // 2 second is generout for one refresh request round trip
    triggered = authStateSpy.wait(3000);
    if (!triggered) {
        QFAIL("FUCK");
    }
    QVERIFY(triggered);

    // Check if we received any authentication signals
    // At this point, there has to be only just one signal received
    if (authStateSpy.count() != 1) {
        QFAIL("FUCK");
    }

    // Authentication completed
    bool success = authStateSpy.first().at(0).toBool();
    qInfo() << "Refresh reply : " << authStateSpy.first().at(1).toString();
    authStateSpy.removeFirst();

    if (!success) {
        QFAIL("FUCK");
    }

    if (success != client.isAuthenticated()) {
        QFAIL("FUCK");
    }

    // Wait just a little bit, for some reason this test thread outruns the housekeeping done in the TradeStationClient that cleans the serviced
    // replies. Otherwise, the following isCleanedUp() triggers because the refreshTokenReply is not flushed from the map<>
    QTest::qWait(100);

    // Verify no resources were leaked
    QVERIFY(client.isCleanedUp());
}



void TestTradeStationClient::testFetchingMoreThanMaximumPerMinute()
{
    QSKIP("Manual test requiring user interaction - skipping in automated tests");

    qInfo() << "Turned of qCDebug(TradeStationClient.debug) for this test so as to not flood the console.";

    // Suppress debug prints for this test as we will do a huge number of requests
    QLoggingCategory::setFilterRules("TradeStationClient.debug=false");

    TradeStationClient& client = TradeStationClient::getInstance();

    QVERIFY(client.isCleanedUp());

    const qsizetype APIMaxCallsPerMinute = 750;
    {
        qInfo() << "Expect a couple of 'Error with the reply' :";
        // Launch max + 1 as fast as possible
        for(size_t i = 0; i != APIMaxCallsPerMinute + 1; i++) {
            // TODO: Add actual API call when we implement endpoints
        }

        // 5s should be enough to perform those requests
        QTest::qWait(5000);
    }

    QVERIFY(client.isCleanedUp());
}

void TestTradeStationClient::testFetchSyncAccounts()
{
    TradeStationClient& client = TradeStationClient::getInstance();
    bool success;
    
    // Verify initial state
    QVERIFY(client.isCleanedUp());
    QVERIFY(client.isAuthenticated());

    QVector<AccountResult> results;
    success = client.fetchSyncAccounts(results);

    // Verify the results
    QVERIFY(success);
    QVERIFY(!results.isEmpty());
    qDebug() << "Found" << results.size() << "accounts:";

    // Verify each account has valid data
    for (const AccountResult& account : results) {
        qDebug() << "Verifying Account:";
        
        qDebug() << "  ID:" << account.getAccountId();
        QVERIFY(!account.getAccountId().isEmpty());
        
        qDebug() << "  Type:" << account.getAccountType();
        QVERIFY(!account.getAccountType().isEmpty());
        
        qDebug() << "  Status:" << account.getStatus();
        QVERIFY(!account.getStatus().isEmpty());
        
        qDebug() << "  Currency:" << account.getCurrency();
        QVERIFY(!account.getCurrency().isEmpty());

        // Check AccountDetail if it exists
        const auto& detail = account.getAccountDetail();
        if (detail.has_value()) {
            qDebug() << "  Account Detail:";
            qDebug() << "    Stock Locate Eligible:" << detail->isStockLocateEligible;
            qDebug() << "    Enrolled in RegT Program:" << detail->enrolledInRegTProgram;
            qDebug() << "    Requires Buying Power Warning:" << detail->requiresBuyingPowerWarning;
            qDebug() << "    Day Trading Qualified:" << detail->dayTradingQualified;
            qDebug() << "    Option Approval Level:" << detail->optionApprovalLevel;
            qDebug() << "    Pattern Day Trader:" << detail->patternDayTrader;
        } else {
            qDebug() << "  No Account Detail available";
        }
    }

    // Verify no resources were leaked
    QVERIFY(client.isCleanedUp());
}

void TestTradeStationClient::testFetchAsyncAccounts()
{
    QSKIP("Not implemented");

}

void TestTradeStationClient::testPlaceSyncOrder()
{
    QSKIP("Not implemented");
}

void TestTradeStationClient::testPlaceAsyncOrder()
{
    QSKIP("Not implemented");
}
