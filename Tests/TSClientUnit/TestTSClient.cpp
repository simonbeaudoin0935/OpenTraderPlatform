#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QSettings>
#include <QDateTime>

#include "TestTSClient.h"
#include "TSClient.h"
#include "MarketHours.h"
#include "Logging.h"

static TSClient* client;

// will be called to create a global test data table.
void TestTSClient::initTestCase_data()
{
    AuthToken savedAuthToken = AuthToken::loadFromSettings();

    qDebug() << "Token validation status:" << (savedAuthToken.isValid() ? "valid" : "invalid");

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

    secondAccountId = "SIM2956555M";
}

// will be called before the first test function is executed.
void TestTSClient::initTestCase() {
    QLoggingCategory::setFilterRules("TSClient.debug=true");

    qInfo() << "Start of test suite";

    QThread::currentThread()->setObjectName("UnitTestThread");

    client = TSClient::getInstancePtr(); // ***** First time to do a getInstance, this will call the constructor

    testRefreshSyncAccessToken();
}

// Will be called before each test function is executed.
void TestTSClient::init()
{
    // QTest installs its own message handler which overrides our colored logging.
    // Reinstall our custom colored message handler before each test to ensure
    // proper colored output during test execution.
    reinstallColoredMessageHandler();
    
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

void TestTSClient::testGetAccountsAsync()
{
    // Verify initial state
    QVERIFY(client->isCleanedUp());
    QVERIFY(client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // Intercept the accounts when they are received
    QSignalSpy fetchAsyncAccoutnsSpy(client, &TSClient::receivedAsyncGetAccounts);

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

void TestTSClient::testGetBalancesAsync()
{
    // Verify initial state
    QVERIFY(client->isCleanedUp());
    QVERIFY(client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // Intercept the accounts when they are received
    QSignalSpy spy(client, &TSClient::receivedAsyncGetBalances);

    client->getBalancesAsync(secondAccountId);

    bool triggered = spy.wait(2000);
    QVERIFY(triggered);

    // Logically, only one signal must be emited
    QCOMPARE(spy.count(), 1);

    // Extract the first emission's argument
    QList<QVariant> firstSignal = spy.first();
    QVERIFY(firstSignal.size() == 1); // One argument

    // Convert QVariant to QVector<Balance>
    QVariant arg = firstSignal.at(0);
    QVERIFY(arg.canConvert<QVector<Balance>>());


    const QVector<Balance> results = arg.value<QVector<Balance>>();
    // Verify each account has valid data
    for (const Balance& balance: results) {

        qDebug().noquote() << "* balance info *\n" << balance.toJsonString();
    }

    QVERIFY(client->isCleanedUp());
}


void TestTSClient::testGetQuoteSnapshotsAsync()
{
    QSKIP("not implemented");
}


void TestTSClient::testPlaceOrderAsync()
{
    if (!MarketHours::isRegularHours()) {
        QSKIP("Test skipped because market is not open");
    }

    PlaceOrderRequest order;

    QVERIFY(client->isCleanedUp());

    // Populate the order
    {
        order.setAccountID(secondAccountId); // Use the saved second account ID
        order.setOrderType(OrderType::Type::Market);
        order.setQuantity(100);
        order.setSymbol("MSFT");
        order.setTradeAction(TradeAction::Buy);
        order.setRoute("Intelligent");

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

    QSignalSpy spy(client, &TSClient::receivedAsyncPlaceOrder);

    client->placeOrderAsync(order);

    bool triggered = spy.wait(2000);
    QVERIFY(triggered);

    // Logically, only one signal must be emited
    QCOMPARE(spy.count(), 1);

    // Extract the first emission's argument
    QList<QVariant> firstSignal = spy.first();
    QVERIFY(firstSignal.size() == 1); // One argument

    // Convert QVariant to QVector<Balance>
    QVariant arg = firstSignal.at(0);
    QVERIFY(arg.canConvert<PlaceOrderResult>());


    const PlaceOrderResult result = arg.value<PlaceOrderResult>();

    qDebug().noquote() << "* PlaceOrderResult info *\n" << result.toJsonString();

    if (MarketHours::isRegularHours()) {
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

void TestTSClient::testCancelOrderAsync()
{
    if (!MarketHours::isRegularHours()) {
        QSKIP("Test skipped because market is not open");
    }

    QString symbol = "AAPL";
    double lastAsk;

    #warning fixme, replace the commented out sync versions and use the async ones with signal spies
    {
        QVector<QuoteSnapshot> quoteResults;
        //bool success = client->getQuoteSnapshotsSync(symbol, quoteResults);
        //QVERIFY(success);

        lastAsk = quoteResults.first().getAsk();
    }

    // Startin by placing a limit order way high so that we can rest assured
    // it doesnt fill while we do out test
    PlaceOrderRequest orderRequest;

    QVERIFY(client->isCleanedUp());

    // Populate the order
    {
        orderRequest.setAccountID(secondAccountId); // Use the saved second account ID
        orderRequest.setOrderType(OrderType::Type::Limit);
        orderRequest.setQuantity(100);
        orderRequest.setSymbol("AAPL");
        orderRequest.setTradeAction(TradeAction::Buy);
        orderRequest.setRoute("Intelligent");
        orderRequest.setLimitPrice(lastAsk / 2); // set ridiculous low price

        // Set up time in force
        TimeInForce timeInForce(OrderDuration::DayPlus);
        orderRequest.setTimeInForce(timeInForce);

        QVERIFY(orderRequest.isValid());
        qDebug().noquote() << "Content of the request :\n" << orderRequest.toJsonString();
    }

    QString orderID;

    // Place the order
    #warning fixme, replace the commented out sync versions and use the async ones with signal spies
    {
        PlaceOrderResult orderResult;
        //bool success = client->placeOrderSync(orderRequest, orderResult);
        //QVERIFY(success);

        qDebug().noquote() << "Received result :\n" << orderResult.toJsonString();

        QVERIFY(orderResult.isAllSuccessful());

        QCOMPARE(orderResult.getOrders().size(), 1);

        orderID = orderResult.getOrders().first().getOrderID();

        QVERIFY(!orderID.isEmpty());
    }

    // Now spit on that thang
    {
        QSignalSpy spy(client, &TSClient::receivedAsyncCancelOrder);

        client->cancelOrderAsync(orderID);

        bool triggered = spy.wait(2000);
        QVERIFY(triggered);


        // Logically, only one signal must be emited
        QCOMPARE(spy.count(), 1);

        // Extract the first emission's argument
        QList<QVariant> firstSignal = spy.first();
        QVERIFY(firstSignal.size() == 1); // One argument

        // Convert QVariant to QVector<Balance>
        QVariant arg = firstSignal.at(0);
        QVERIFY(arg.canConvert<CancelOrderResult>());


        const CancelOrderResult result = arg.value<CancelOrderResult>();

        QVERIFY(result.isError() == false);

        qDebug() << "Cancel order message : " << result.getMessage();

        QVERIFY(result.getOrderID() == orderID);
    }
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


void TestTSClient::testGetBarsAsync()
{
    // Define 4:00 PM (16:00)
    const QTime fourPM(10, 0, 0);
    // Define 8:00 PM (20:00)
    const QTime eightPM(14, 0, 0);

    QString symbol = "AAPL";
    unsigned int interval = 1;
    Bar::BarUnit unit = Bar::BarUnit::Minute;
    unsigned int barsback = 0;
    Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::USEQPost;
    QDateTime firstDate;
    QDateTime lastDate;

    {
        QDateTime now = QDateTime::currentDateTime();

        // Get the date for the previous day
        QDate previousDay = now.date().addDays(-1);

        while(previousDay.dayOfWeek() > 4) previousDay = previousDay.addDays(-1);

        // Create a QDateTime for previous day at 4:00 PM in New York time zone
        QTimeZone newYorkTimeZone("America/New_York");

        firstDate = QDateTime(previousDay, fourPM, newYorkTimeZone);
        lastDate = QDateTime(previousDay, eightPM, newYorkTimeZone);
    }

    // Verify initial state
    QVERIFY(client->isCleanedUp());
    QVERIFY(client->isAuthenticated());
    QVERIFY(!client->isAuthInProgress());

    // Intercept the bars received signal
    QSignalSpy getBarsAsyncSpy(client, &TSClient::receivedAsyncGetBars);

    client->getBarsAsync(symbol, interval, unit, barsback, sessionTemplate, firstDate, lastDate);

    bool triggered = getBarsAsyncSpy.wait(2000);
    QVERIFY(triggered);

    // Logically, only one signal must be emited
    QCOMPARE(getBarsAsyncSpy.count(), 1);

    // Extract the first emission's argument
    QList<QVariant> firstSignal = getBarsAsyncSpy.first();
    QVERIFY(firstSignal.size() == 2); // Two arguments, symbol and bar vector

    // Convert first arg to QString
    QVariant firstArg = firstSignal.at(0);
    QVERIFY(firstArg.canConvert<QString>());

    QVariant secondArg = firstSignal.at(1);
    QVERIFY(secondArg.canConvert<QVector<Bar>>());

    QCOMPARE(firstArg.value<QString>(), symbol);

    QVector<Bar> receivedBars = secondArg.value<QVector<Bar>>();

    int minutesDifference = fourPM.secsTo(eightPM) / 60;

    QCOMPARE_GE(receivedBars.count(), minutesDifference - 20); // Give a 20 bars leeway

    QVERIFY(client->isCleanedUp());
}

void TestTSClient::testStreamBars()
{

}

void TestTSClient::testStreamBarsRecording()
{
    QString symbol = "AAPL";
    StreamBars* streamBars;
    const size_t countdown = 30;

    QVERIFY(client->isCleanedUp());

    streamBars = client->openStreamBars(symbol,
                                        1,
                                        Bar::BarUnit::Minute,
                                        0,
                                        Bar::BarSessionTemplate::USEQ24Hour);
    QVERIFY(streamBars != nullptr);

    QString dir = QString("/home/simon/Documents/L2T/Stream-Recordings");

    QSignalSpy signalSpy(streamBars, &StreamBars::receivedNewBar);

    qDebug() << "Waiting 5 seconds to let Stream Bars data pile up...";

    // Let this thread's event loop run a bit to receive some market depth quotes
    for (size_t i = countdown; i != 0; i--) {
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

            QString stock = firstArgOfSignal.value<QString>();
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

void TestTSClient::testStreamOrders()
{
    if (!MarketHours::isRegularHours()) {
        QSKIP("Test skipped because market is not open");
    }

    QString symbol = "AAPL";
    StreamOrders* stream;

    QVERIFY(client->isCleanedUp());

    stream = client->openStreamOrders(secondAccountId);
    QVERIFY(stream != nullptr);

    QSignalSpy spy(stream, &StreamOrders::receivedNewOrder);

    QTest::qWait(3000);

    qDebug() << "Recieved " << spy.size() << "Order signals";

    double lastAsk;

    #warning fixme, replace the commented out sync versions and use the async ones with signal spies
    {
        QVector<QuoteSnapshot> quoteResults;
        //bool success = client->getQuoteSnapshotsSync(symbol, quoteResults);
        //QVERIFY(success);

        lastAsk = quoteResults.first().getAsk();
    }

    // Startin by placing a limit order way high so that we can rest assured
    // it doesnt fill while we do out test
    PlaceOrderRequest orderRequest;


    // Populate the order
    {
        orderRequest.setAccountID(secondAccountId); // Use the saved second account ID
        orderRequest.setOrderType(OrderType::Type::Limit);
        orderRequest.setQuantity(100);
        orderRequest.setSymbol("AAPL");
        orderRequest.setTradeAction(TradeAction::Buy);
        orderRequest.setRoute("Intelligent");
        orderRequest.setLimitPrice(lastAsk-0.05); // set ridiculous low price

        // Set up time in force
        TimeInForce timeInForce(OrderDuration::DayPlus);
        orderRequest.setTimeInForce(timeInForce);

        QVERIFY(orderRequest.isValid());
        qDebug().noquote() << "Content of the request :\n" << orderRequest.toJsonString();
    }

    QString orderID;

    // Place the order
    #warning fixme, replace the commented out sync versions and use the async ones with signal spies
    {
        PlaceOrderResult orderResult;
        //bool success = client->placeOrderSync(orderRequest, orderResult);
        //QVERIFY(success);

        qDebug().noquote() << "Received result :\n" << orderResult.toJsonString();

        QVERIFY(orderResult.isAllSuccessful());

        QCOMPARE(orderResult.getOrders().size(), 1);

        orderID = orderResult.getOrders().first().getOrderID();

        QVERIFY(!orderID.isEmpty());

        qDebug().noquote() << "Order placed : " << orderResult.toJsonString();
    }


    QTest::qWait(50000);
    QVERIFY(client->isCleanedUp());

}

void TestTSClient::testStreamPositions()
{
    QSKIP("Not implemented");
}

void TestTSClient::testMockStreamBars()
{

}

void TestTSClient::testStreamCount()
{
    QString symbol1 = "AAPL";
    QString symbol2 = "MSFT";
    
    QVERIFY(client->isCleanedUp());
    
    // Initial stream count should be 0
    QCOMPARE(client->getStreamCount(), 0);
    
    qDebug() << "Initial stream count:" << client->getStreamCount();
    
    // Open first stream
    StreamBars* stream1 = client->openStreamBars(symbol1, 1, Bar::BarUnit::Minute, 0);
    QVERIFY(stream1 != nullptr);
    QCOMPARE(client->getStreamCount(), 1);
    
    qDebug() << "Stream count after opening first BarStream:" << client->getStreamCount();
    
    // Open second stream
    StreamMarketDepthQuote* stream2 = client->openStreamMarketDepthQuote(symbol2);
    QVERIFY(stream2 != nullptr);
    QCOMPARE(client->getStreamCount(), 2);
    
    qDebug() << "Stream count after opening MarketDepthQuote stream:" << client->getStreamCount();
    
    // Open third stream
    StreamBars* stream3 = client->openStreamBars(symbol2, 1, Bar::BarUnit::Minute, 0);
    QVERIFY(stream3 != nullptr);
    QCOMPARE(client->getStreamCount(), 3);
    
    qDebug() << "Stream count after opening third stream:" << client->getStreamCount();
    
    // Close first stream
    client->closeStreamBars(stream1);
    QCOMPARE(client->getStreamCount(), 2);
    
    qDebug() << "Stream count after closing first stream:" << client->getStreamCount();
    
    // Close second stream
    client->closeStreamMarketDepthQuote(stream2);
    QCOMPARE(client->getStreamCount(), 1);
    
    qDebug() << "Stream count after closing second stream:" << client->getStreamCount();
    
    // Close third stream
    client->closeStreamBars(stream3);
    QCOMPARE(client->getStreamCount(), 0);
    
    qDebug() << "Final stream count:" << client->getStreamCount();
    
    QVERIFY(client->isCleanedUp());
}
