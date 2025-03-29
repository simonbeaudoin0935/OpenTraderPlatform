#include <QDebug>
#include "test_tradestationclient.h"
#include "../../Clients/TradeStationClient/tradestationclient.h"
#include "../../Clients/TradeStationClient/Auth/AuthWindow.h"
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>


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

    // Create settings object to manually set expired tokens
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "TradeStationAuth", "Tokens", this);
    settings.setFallbacksEnabled(false);

    // Set an expired token (received 1 hour ago with 30-minute timeout)
    settings.setValue("access_token", "expired_token");
    settings.setValue("id_token", "expired_token");
    settings.setValue("refresh_token", "expired_token");
    settings.setValue("token_received_time", QDateTime::currentDateTime().addSecs(-3600).toString(Qt::ISODate));
    settings.setValue("token_timeout_seconds", 1800); // 30 minutes
    settings.sync();

    // Verify that isAlreadyAuthenticated returns false for expired token
    QVERIFY(!AuthWindow::isAlreadyAuthenticated());
    QCOMPARE(AuthWindow::isAlreadyAuthenticated(), client.isAuthenticated());

    // Clean up
    settings.remove("access_token");
    settings.remove("id_token");
    settings.remove("refresh_token");
    settings.remove("token_received_time");
    settings.remove("token_timeout_seconds");
    settings.sync();

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
