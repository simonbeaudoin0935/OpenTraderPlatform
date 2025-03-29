#include <QDebug>
#include "test_tradestationclient.h"
#include "../../Clients/TradeStationClient/tradestationclient.h"
#include "../../Clients/TradeStationClient/Auth/AuthWindow.h"
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

void TestTradeStationClient::initTestCase_data()
{
    savedValidAuthToken = AuthToken::loadFromSettings();

    qDebug() << "Token details:" << savedValidAuthToken.toString();

    QVERIFY(savedValidAuthToken.isValid());

    QVERIFY(!savedValidAuthToken.isExpired());
}

void TestTradeStationClient::initTestCase() {
    QLoggingCategory::setFilterRules("TradeStationClient.debug=true");

    qInfo() << "Start of test suite";
}

void TestTradeStationClient::init()
{
    // Make sure the debug prints are enabled
    QLoggingCategory::setFilterRules("TradeStationClient.debug=true");

    // A test could change the logging behavior to suppress too much logging,
    // but it will be reset before each test
}

void TestTradeStationClient::testIsAlreadyAuthenticatedReturnsFalseWhenTokenExpired()
{
    TradeStationClient& client = TradeStationClient::getInstance();
    
    // Verify initial state
    QVERIFY(!client.isAuthenticated());
    QVERIFY(!client.isAuthInProgress());
    QVERIFY(client.isCleanedUp());

    // Create an expired token using the AuthToken class
    AuthToken expiredToken(
        "expired_token",  // accessToken
        "expired_token",  // refreshToken
        "expired_token",  // idToken
        "Bearer",         // tokenType
        "openid profile MarketData ReadAccount Trade offline_access",  // scope
        1200,            // expiresIn
        QDateTime::currentDateTime().addSecs(-3600)  // received 1 hour ago
    );

    // Save the expired token
    QVERIFY(AuthToken::storeToSettings(expiredToken));

    // Verify that isAlreadyAuthenticated returns false for expired token
    QVERIFY(!AuthWindow::isAlreadyAuthenticated());
    QCOMPARE(AuthWindow::isAlreadyAuthenticated(), client.isAuthenticated());

    // Verify no resources were leaked
    QVERIFY(client.isCleanedUp());
}

void TestTradeStationClient::testIsAlreadyAuthenticatedReturnsFalseWhenNoTokens()
{
    TradeStationClient& client = TradeStationClient::getInstance();
    
    // Verify initial state
    QVERIFY(!client.isAuthenticated());
    QVERIFY(!client.isAuthInProgress());
    QVERIFY(client.isCleanedUp());

    // Clear any existing tokens to ensure a clean state
    AuthWindow::clearTokens();
    QVERIFY(!AuthWindow::isAlreadyAuthenticated());

    QCOMPARE(AuthWindow::isAlreadyAuthenticated(), client.isAuthenticated());

    // Verify no resources were leaked
    QVERIFY(client.isCleanedUp());
}

void TestTradeStationClient::testAuthentication()
{
    QSKIP("Manual test requiring user interaction - skipping in automated tests");
    
    TradeStationClient& client = TradeStationClient::getInstance();
    
    // Create signal spies to monitor authentication signals
    QSignalSpy authStateSpy(&client, &TradeStationClient::authenticationStateChanged);
    QSignalSpy authErrorSpy(&client, &TradeStationClient::authenticationError);

    // Verify initial state
    QVERIFY(!client.isAuthenticated());
    QVERIFY(!client.isAuthInProgress());
    QVERIFY(client.isCleanedUp());
    QCOMPARE(AuthWindow::isAlreadyAuthenticated(), client.isAuthenticated());

    // Test that we can't start multiple auth processes
    client.launchAuthProcess();
    QVERIFY(client.isAuthInProgress());
    client.launchAuthProcess(); // Second call should be ignored
    QVERIFY(client.isAuthInProgress());

    // Wait for authentication to complete or fail
    // Note: This is a manual test as we can't automate the OAuth flow
    // The user will need to complete the authentication process
    QTest::qWait(30000); // Wait up to 30 seconds for manual authentication

    // Verify that auth is no longer in progress
    QVERIFY(!client.isAuthInProgress());

    // Check if we received any authentication signals
    if (authStateSpy.count() > 0) {
        // Authentication completed
        bool success = authStateSpy.last().at(0).toBool();
        QVERIFY(success == client.isAuthenticated());
        QCOMPARE(AuthWindow::isAlreadyAuthenticated(), client.isAuthenticated());
    } else if (authErrorSpy.count() > 0) {
        // Authentication failed
        QString error = authErrorSpy.last().at(0).toString();
        qDebug() << "Authentication failed with error:" << error;
        QVERIFY(!client.isAuthenticated());
        QCOMPARE(AuthWindow::isAlreadyAuthenticated(), client.isAuthenticated());
    }

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

    success = AuthToken::storeToSettings(savedValidAuthToken);

    QVERIFY(success);
    
    // Verify initial state
    QVERIFY(client.isCleanedUp());
    QVERIFY(AuthWindow::isAlreadyAuthenticated());
    

    qDebug() << "Setting Key into TraceStationClient";
    client.setAPIKey(savedValidAuthToken.getAccessToken()); //TODO fix this for good, api key should be set internally

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
