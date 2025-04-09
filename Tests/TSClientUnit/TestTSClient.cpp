#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

#include "TestTSClient.h"
#include "../../Clients/TSClient/TSClient.h"

static TSClient* client;

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

    QThread::currentThread()->setObjectName("UnitTestThread");

    client = TSClient::getInstancePtr(); // ***** First time to do a getInstance, this will call the constructor
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

// This test HAS to be the first, because we want to test the refresh logic, which launches when
// calling client->start() only. Since we use the same singleton instance throughout the whole test
// suite this has to be done first
void TestTSClient::testRefreshSyncAccessToken()
{
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
}

void TestTSClient::testFetchSyncAccounts()
{
    bool success;
    
    // Verify initial state
    QVERIFY(client->isCleanedUp());
    QVERIFY(client->isAuthenticated());

    QVector<Account> results;
    success = client->getAccountsSync(results);

    // Verify the results
    QVERIFY(success);
    QCOMPARE(results.size(), 2); // Verify exactly 2 accounts
    qDebug() << "Found" << results.size() << "accounts:";

    // Save the second account's ID for use in place order test
    secondAccountId = results[1].getAccountId();
    qDebug() << "Saved second account ID for place order test:" << secondAccountId;

    // Verify each account has valid data
    const QVector<Account>& constResults = results;
    for (const Account& account : constResults) {

        QVERIFY(account.isValid());

        qDebug().noquote() << "* Account info *\n" << account.toJsonString();
    }

    // Verify no resources were leaked
    QVERIFY(client->isCleanedUp());
}

void TestTSClient::testFetchAsyncAccounts()
{
    // Verify initial state
    QVERIFY(client->isCleanedUp());
    QVERIFY(client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // Intercept the accounts when they are received
    QSignalSpy fetchAsyncAccoutnsSpy(client, &TSClient::accountsAsyncReceived); // Create signal spies to monitor authentication signals

    client->getAccountsAsync();

    bool triggered = fetchAsyncAccoutnsSpy.wait(2000);
    QVERIFY(triggered);

    // Logically, only one signal must be emited
    QCOMPARE(fetchAsyncAccoutnsSpy.count(), 1);

    // Extract the first emission's argument
    QList<QVariant> firstSignal = fetchAsyncAccoutnsSpy.first();
    QVERIFY(firstSignal.size() == 1); // One argument

    // Convert QVariant to QVector<Account>
    QVariant arg = firstSignal.at(0);
    QVERIFY(arg.canConvert<QVector<Account>>());


    const QVector<Account> results = arg.value<QVector<Account>>();
    // Verify each account has valid data
    for (const Account& account : results) {

        QVERIFY(account.isValid());

        qDebug().noquote() << "* Account info *\n" << account.toJsonString();
    }

    QVERIFY(client->isCleanedUp());
}

void TestTSClient::testFetchSyncQuoteSnapshots()
{
    bool success;

    QString symbol = "AAPL";

    // Verify initial state
    QVERIFY(client->isCleanedUp());
    QVERIFY(client->isAuthenticated());

    QVector<QuoteSnapshot> quoteResults;
    success = client->getQuoteSnapshotsSync(symbol, quoteResults);

    // Verify the results
    QVERIFY(success);
    QCOMPARE(quoteResults.size(), 1); // We asked for one symbol, there should be one quote
    qDebug() << "Found" << quoteResults.size() << " quote snapshots";


    // Verify each account has valid data
    const QVector<QuoteSnapshot>& constResults = quoteResults;
    for (const QuoteSnapshot& quote : constResults) {

        QVERIFY(quote.isValid());

        qDebug().noquote() << "Quote snapshot :\n" << quote.toJsonString();
    }

    // Verify no resources were leaked
    QVERIFY(client->isCleanedUp());
}

void TestTSClient::testFetchAsyncQuoteSnapshots()
{

}

#warning create test for concurrent sync requests, there might be a race with the wait-condition where its only one for everybody
void TestTSClient::testPlaceSyncOrder()
{
    PlaceOrderRequest order;

    QVERIFY(client->isCleanedUp());

    // Populate the order
    {
        order.setAccountID(secondAccountId); // Use the saved second account ID
        order.setOrderType(OrderType::Market);
        order.setQuantity(100);
        order.setSymbol("AAPL");
        order.setTradeAction(TradeAction::Buy);
        order.setRoute("Intelligent");

        //order.setOrderConfirmID("5109740935");  //TODO ********************** add test cases for invalid orders

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
    bool success = client->placeSyncOrder(order, result);
    
    // Verify the order was placed successfully
    QVERIFY(success);
    
    qDebug().noquote() << "Received result :\n" << result.toJsonString();


    if (isMarketOpened()) {
        QVERIFY(result.isAllSuccessful());
    } else {
        QVERIFY(result.hasErrors());
        QVERIFY(result.getErrors().first().getError().has_value());
        QVERIFY(result.getErrors().first().getMessage() == "Order failed. Reason: No Day orders after 4:00PM Eastern");
        QVERIFY(result.getErrors().first().getOrderID().isEmpty() == false);

        qDebug() << "*** Market is closed this is expected ***";
    }

    QVERIFY(client->isCleanedUp());
}

void TestTSClient::testPlaceAsyncOrder()
{
    QSKIP("Not implemented");
}



void TestTSClient::testFetchingMoreThanMaximumPerMinute()
{
    QSKIP("Not implemented");

    qInfo() << "Turned of qCDebug(TSClient.debug) for this test so as to not flood the console.";

    // Suppress debug prints for this test as we will do a huge number of requests
    QLoggingCategory::setFilterRules("TSClient.debug=false");

    QVERIFY(client->isCleanedUp());

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

    QVERIFY(client->isCleanedUp());
}

void TestTSClient::testStreamBars()
{
    QString symbol = "AAPL";
    StreamBars* streamBars;

    QVERIFY(client->isCleanedUp());

    streamBars = client->openStreamBars(symbol,
                                        1,
                                        Bar::BarUnit::Minute,
                                        10,
                                        Bar::BarSessionTemplate::USEQ24Hour);
    QVERIFY(streamBars != nullptr);

    QSignalSpy signalSpy(streamBars, &StreamBars::receivedNewBar);

    qDebug() << "Waiting 5 seconds to let Stream Bars data pile up...";

    // Let this thread's event loop run a bit to receive some market depth quotes
    for (size_t i = 5; i != 0; i--) {
        qDebug() << "Countdown : " << i << " seconds";

        QTest::qWait(1000);
    }

    // Spit on that thang
    client->closeStreamBars(streamBars);

    // Verifying integrity of received data
    {
        QVERIFY(signalSpy.count() > 0);

        qDebug() << "Received " << signalSpy.count() << " StreamBars bars";

        // Validate every signal received
        for (const QList<QVariant>& signal :  signalSpy) {
            QVERIFY(signal.size() == 2); // Two argument to StreamBar::receiveNewBar

            // Convert QVariant to Bar (bar is second argument)
            QVariant firstArgOfSignal = signal.at(0);
            QVariant secondArgOfSignal = signal.at(1);

            QVERIFY(firstArgOfSignal.canConvert<QString>());
            QVERIFY(secondArgOfSignal.canConvert<Bar>());

            QString stock = firstArgOfSignal.value<QString>();
            Bar bar = secondArgOfSignal.value<Bar>();

            QVERIFY(stock == symbol);
            QVERIFY(bar.isValid());
        }
    }

    QVERIFY(client->isCleanedUp());
}

void TestTSClient::testStreamMarketDepthQuote()
{
    // TODO: Implement test for market depth streaming
    // This will likely involve:
    // 1. Setting up a WebSocket connection
    // 2. Subscribing to market depth updates for a symbol
    // 3. Verifying the received data matches the expected format
    // 4. Testing error cases and disconnection scenarios
    // 5. Testing reconnection scenarios
    // 6. Testing multiple symbol subscriptions
    // 7. Testing unsubscribe functionality

    QString symbol = "AAPL";
    StreamMarketDepthQuote* stream;

    QVERIFY(client->isCleanedUp());

    stream = client->openStreamMarketDepthQuote(symbol);
    QVERIFY(stream != nullptr);

    QSignalSpy signalSpy(stream, &StreamMarketDepthQuote::receivedNewMarketDepthQuote);

    qDebug() << "Waiting 5 seconds to let Level 2 data pile up...";

    // Let this thread's event loop run a bit to receive some market depth quotes
    for (size_t i = 5; i != 0; i--) {
        qDebug() << "Countdown : " << i << " seconds";

        QTest::qWait(1000);
    }

    // Spit on that thang
    client->closeStreamMarketDepthQuote(stream);

    // Verifying integrity of received data
    {
        QVERIFY(signalSpy.count() > 0);

        qDebug() << "Received " << signalSpy.count() << "Market Depth quotes";

        // Validate every signal received
        for (const QList<QVariant>& signal :  signalSpy) {
            QVERIFY(signal.size() == 2); // Two argument to StreamMarketDepthQuote::receiveNewQuote

            // Convert QVariant to MarketDepthQuote
            QVariant firstArgOfSignal = signal.at(0);
            QVariant secondArgOfSignal = signal.at(1);

            QVERIFY(firstArgOfSignal.canConvert<QString>());
            QVERIFY(secondArgOfSignal.canConvert<MarketDepthQuote>());

            QString stock =firstArgOfSignal.value<QString>();
            MarketDepthQuote quote = secondArgOfSignal.value<MarketDepthQuote>();

            QVERIFY(stock == symbol);

            // Verify all the quotes contained in that signal
            {
                // Verify all the bid quotes
                //QVERIFY(quote.getBids()...)

                // Verify all the ask quotes
                //QVERIFY(quote.getAsks()...)
            }

        }
    }

    QVERIFY(client->isCleanedUp());
}

bool TestTSClient::isMarketOpened() {
    // Get the current date and time in the system's local time zone
    QDateTime currentTime = QDateTime::currentDateTime();

    // Define the Eastern Time zone (America/New_York)
    QTimeZone easternTimeZone("America/New_York");

    // Convert to Eastern Time
    QDateTime easternTime = currentTime.toTimeZone(easternTimeZone);

    // Define 4:00 PM Eastern Time as the market closing time
    QTime marketCloseTime(16, 0, 0); // 16:00:00 in 24-hour format

    // Define 9:30 PM Eastern Time as the market opening time
    QTime marketOpenTime(9, 30, 0); // 16:00:00 in 24-hour format


    // Extract the current time component in Eastern Time
    QTime currentEasternTime = easternTime.time();

    // Return true if the current time is after 4:00 PM
    return (currentEasternTime > marketOpenTime) && (currentEasternTime < marketCloseTime);
}
