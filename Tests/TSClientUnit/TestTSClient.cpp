#include <QDebug>
#include "TestTSClient.h"
#include "../../Clients/TSClient/TSClient.h"
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

static QString retreivedSIMAccountID;
TSClient* TSClient;


// will be called to create a global test data table.
void TestTSClient::initTestCase_data()
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
void TestTSClient::initTestCase() {
    QLoggingCategory::setFilterRules("TSClient.debug=true");

    qInfo() << "Start of test suite";
}

// Will be called before each test function is executed.
void TestTSClient::init()
{
    // Make sure the debug prints are enabled
    QLoggingCategory::setFilterRules("TSClient.debug=true");

    // A test could change the logging behavior to suppress too much logging,
    // but it will be reset before each test
}

// Will be called after every test function.
void TestTSClient::cleanup() {

}

void TestTSClient::testRefreshSyncAccessToken()
{
    bool triggered;
    TSClient = TSClient::getInstancePtr(); // First time do a getInstance, this will call the constructor
    QSignalSpy authStateSpy(TSClient, &TSClient::authStateChanged); // Create signal spies to monitor authentication signals

    // Critical to do, will start the thread
    TSClient->start();

    // Verify initial state
    QVERIFY(!TSClient->isAuthenticated());
    QVERIFY(TSClient->isAuthInProgress()); // The client will immediately initiate a refresh
    QVERIFY(TSClient->isCleanedUp());

    // Wait for the first authStateChanged signal that happens
    // 2 second is generout for one refresh request round trip
    triggered = authStateSpy.wait(3000);

    QVERIFY(triggered);

    // Check if we received any authentication signals
    // At this point, there has to be only just one signal received
    QCOMPARE(authStateSpy.count(), 1);

    // Authentication completed
    bool success = authStateSpy.first().at(0).toBool();
    qInfo() << "Refresh reply : " << authStateSpy.first().at(1).toString();
    authStateSpy.removeFirst();

    QVERIFY(success);

    QVERIFY(TSClient->isAuthenticated());
    QVERIFY(!TSClient->isAuthInProgress());

    // Wait just a little bit, for some reason this test thread outruns the housekeeping done in the TSClient that cleans the serviced
    // replies. Otherwise, the following isCleanedUp() triggers because the refreshTokenReply is not flushed from the map<>
    QTest::qWait(100);

    // Verify no resources were leaked
    QVERIFY(TSClient->isCleanedUp());
}




void TestTSClient::testFetchSyncAccounts()
{
    bool success;
    
    // Verify initial state
    QVERIFY(TSClient->isCleanedUp());
    QVERIFY(TSClient->isAuthenticated());

    QVector<AccountsResult> results;
    success = TSClient->fetchSyncAccounts(results);

    // Verify the results
    QVERIFY(success);
    QCOMPARE(results.size(), 2); // Verify exactly 2 accounts
    qDebug() << "Found" << results.size() << "accounts:";

    // Save the second account's ID for use in place order test
    retreivedSIMAccountID = results[1].getAccountId();
    qDebug() << "Saved second account ID for place order test:" << retreivedSIMAccountID;

    // Verify each account has valid data
    for (const AccountsResult& account : results) {
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
    QVERIFY(TSClient->isCleanedUp());
}

void TestTSClient::testFetchAsyncAccounts()
{   
    QSignalSpy authStateSpy(TSClient, &TSClient::authStateChanged); // Create signal spies to monitor authentication signals

    QSKIP("Not implemented");

}

void TestTSClient::testPlaceSyncOrder()
{
    PlaceOrderRequest order;

    // Populate the order
    {
        order.setAccountID(retreivedSIMAccountID); // Use the saved second account ID
        order.setOrderType(OrderType::Market);
        order.setQuantity(100);
        order.setSymbol("AAPL");
        order.setTradeAction(TradeAction::Buy);
        order.setRoute("Intelligent");

        order.setOrderConfirmID("5109740935");

        // Set up time in force
        TimeInForce timeInForce(OrderDuration::Day);
        order.setTimeInForce(timeInForce);

        // Set up advanced options
        //AdvancedOptions advancedOptions;
        //advancedOptions.setAllOrNone(true);
        //order.setAdvancedOptions(advancedOptions);
    }
    
    QVERIFY(order.isValid());

    qDebug().noquote() << "Content of the request :\n" << order.toJsonString();

    // Place the order
    PlaceOrderResult result;
    bool success = TSClient->placeSyncOrder(order, result);
    
    // Verify the order was placed successfully
    QVERIFY(success);
    
    qDebug().noquote() << "Received reault :\n" << result.toJsonString();

    // Verify the result contains expected data
    QVERIFY(!result.getOrderID().isEmpty());
    QCOMPARE(result.getSymbol(), QString("AAPL"));
    QCOMPARE(result.getQuantity(), 100);
    QCOMPARE(result.getOrderType(), QString("Market"));
    
    // Verify the status is one of the expected values
    QString status = result.getStatus();
    QVERIFY(status == "OK" || status == "ACCEPTED" || status == "PENDING");
    
    // If there was an error, log it
    if (!result.getError().isEmpty()) {
        qWarning() << "Order placement had error:" << result.getError();
        qWarning() << "Detailed message:" << result.getDetailedMessage();
    }
}

void TestTSClient::testPlaceAsyncOrder()
{
    QSKIP("Not implemented");
}



void TestTSClient::testFetchingMoreThanMaximumPerMinute()
{
    QSKIP("Manual test requiring user interaction - skipping in automated tests");

    qInfo() << "Turned of qCDebug(TSClient.debug) for this test so as to not flood the console.";

    // Suppress debug prints for this test as we will do a huge number of requests
    QLoggingCategory::setFilterRules("TSClient.debug=false");

    QVERIFY(TSClient->isCleanedUp());

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

    QVERIFY(TSClient->isCleanedUp());
}
