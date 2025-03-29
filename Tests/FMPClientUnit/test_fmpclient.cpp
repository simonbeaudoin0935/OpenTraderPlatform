#include <QDebug>
#include "test_fmpclient.h"
#include "../../Clients/FMPClient/fmpclient.h"

extern QString fmpKey;

#include <QElapsedTimer>

// Unit test of the FMPClient singleton. Having it as a singleton is somewhat
// handy for testing as the same object is used between tests, further confirming
// the good behavior between tests without having to add tests that test when the
// same object is reused.

void TestFMPClient::initTestCase() {
    QLoggingCategory::setFilterRules("FMPClient.debug=true");

    qInfo() << "Start of test suite";

    FMPClient::getInstance().setAPIKey(fmpKey);

}

void TestFMPClient::init()
{
    // Make sure the debug prints are enabled
    QLoggingCategory::setFilterRules("FMPClient.debug=true");

    // A test could change the logging behavior to suppress too much logging,
    // but it will be reset before each test
}

void TestFMPClient::testFetchSyncQuoteShort() {
    FMPClient& client = FMPClient::getInstance();
    FMPClient::QuoteShortResult quoteResult;
    bool success;

    QVERIFY(client.isCleanedUp());

    {
        success = client.fetchSyncQuoteShort("AAPL", quoteResult);
    }

    QVERIFY2(success, "Sync fetch failed or timed out");
    QVERIFY(quoteResult.price > 0.0);
    QVERIFY(quoteResult.change != 0.0);
    QVERIFY(quoteResult.volume >= 0);

    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchAsyncQuoteShort() {
    FMPClient& client = FMPClient::getInstance();    

    QVERIFY(client.isCleanedUp());

    QSignalSpy spy(&client, &FMPClient::quoteShortReceived);

    QVERIFY(spy.isValid());

    {
        client.fetchAsyncQuoteShort("AAPL");

        bool triggered = spy.wait(2000);
        QVERIFY(triggered);
    }

    QVERIFY2(spy.count() == 1, "Async fetch did not emit signal");

    QList<QVariant> arguments = spy.takeFirst();
    FMPClient::QuoteShortResult quoteResult = arguments.at(0).value<FMPClient::QuoteShortResult>();

    QVERIFY(quoteResult.symbol == "AAPL");
    QVERIFY(quoteResult.price > 0.0);
    QVERIFY(quoteResult.change != 0.0);
    QVERIFY(quoteResult.volume >= 0);

    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchSyncSharesFloat()
{
    FMPClient& client = FMPClient::getInstance();
    FMPClient::SharesFloatResult floatResult;
    bool success;

    QVERIFY(client.isCleanedUp());

    {
        success = client.fetchSyncSharesFloat("AAPL", floatResult);
    }

    QVERIFY2(success, "Sync fetch failed or timed out");
    QVERIFY(!floatResult.date.isEmpty());
    QVERIFY(floatResult.freeFloat >= 0.0 && floatResult.freeFloat < 100.0);
    QVERIFY(floatResult.floatShares >= 0);
    QVERIFY(floatResult.outstandingShares >= 0);

    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchAsyncSharesFloat()
{
    FMPClient& client = FMPClient::getInstance();

    QVERIFY(client.isCleanedUp());

    QSignalSpy spy(&client, &FMPClient::sharesFloatReceived);

    QVERIFY(spy.isValid());

    {
        client.fetchAsyncSharesFloat("AAPL");

        bool triggered = spy.wait(2000);
        QVERIFY(triggered);
    }

    QVERIFY2(spy.count() == 1, "Async fetch did not emit signal");
    QList<QVariant> arguments = spy.takeFirst();
    FMPClient::SharesFloatResult floatResult = arguments.at(0).value<FMPClient::SharesFloatResult>();
    
    QVERIFY(floatResult.symbol == "AAPL");
    {
        QString format = "yyyy-MM-dd HH:mm:ss";
        QDateTime dt = QDateTime::fromString(floatResult.date, format);
        QVERIFY(dt.isValid());  // date
    }
    QVERIFY(floatResult.freeFloat != 0.0); // free-float
    QVERIFY(floatResult.floatShares >= 0); // float shares
    QVERIFY(floatResult.outstandingShares >= 0); // outstanding shares

    QVERIFY(client.isCleanedUp());
}

// TODO do more testing?? perhaps
void TestFMPClient::testFetchSyncCompanyScreener()
{
    FMPClient& client = FMPClient::getInstance();
    bool success;
    QVector<CompanyScreenerResult> results;

    QVERIFY(client.isCleanedUp());

    {
        CompanyScreenerFilter filter;

        filter.setIndustry("Biotechnology");
        filter.setPriceMoreThan(2.0);
        filter.setPriceLowerThan(20.0);
        filter.setExchange("NASDAQ");
        filter.setIsActivelyTrading(true);
        filter.setIsEtf(false);
        filter.setIsFund(false);
        filter.setCountry("US");
        filter.setVolumeMoreThan(10000);

        success = client.fetchSyncCompanyScreener(filter, results);

        qDebug() << "Company-Screener produced " << results.size() << " results";

    }

    QVERIFY2(success, "Sync fetch failed or timed out");
    QVERIFY(!results.isEmpty());
    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchSyncStockNews()
{
    FMPClient& client = FMPClient::getInstance();
    bool success;
    QVector<StockNewsResult> results;
    StockNewsFilter filter;

    QVERIFY(client.isCleanedUp());

    {
        // Test with date range
        filter.setSymbol("AAPL");
        filter.setLimit(5);  // Limit to 5 news items for faster testing
        
        // Set date range for last 7 days
        QDate toDate = QDate::currentDate();
        QDate fromDate = toDate.addDays(-7);
        filter.setFrom(fromDate);
        filter.setTo(toDate);

        success = client.fetchSyncStockNews(filter, results);

        qDebug() << "Stock News produced" << results.size() << "results between" 
                 << fromDate.toString("yyyy-MM-dd") << "and" << toDate.toString("yyyy-MM-dd");
    }

    QVERIFY2(success, "Sync fetch failed or timed out");
    QVERIFY(!results.isEmpty());
    QVERIFY(results.size() <= 5);  // Should not exceed our limit

    // Verify the content of each result
    for (const StockNewsResult& news : results) {
        QVERIFY(!news.getSymbol().isEmpty());
        QVERIFY(!news.getTitle().isEmpty());
        QVERIFY(!news.getText().isEmpty());
        QVERIFY(!news.getUrl().isEmpty());

        // Verify date is within our specified range
        if (!news.getDate().isEmpty()) {
            QString format = "yyyy-MM-dd HH:mm:ss";
            QDateTime newsDate = QDateTime::fromString(news.getDate(), format);
            QVERIFY(newsDate.isValid());
            
            // Convert to date for comparison (ignoring time)
            QDate articleDate = newsDate.date();
            QVERIFY2(articleDate >= filter.getFrom() && articleDate <= filter.getTo(),
                    qPrintable(QString("News date %1 is outside range %2 to %3")
                             .arg(articleDate.toString("yyyy-MM-dd"))
                             .arg(filter.getFrom().value_or(QDate()).toString("yyyy-MM-dd"))
                             .arg(filter.getTo().value_or(QDate()).toString("yyyy-MM-dd"))));
        }
    }

    QVERIFY(client.isCleanedUp());
}

/*
 * Testing that when the fetcher times out because the reply took too long to arrive,
 * that no resources are leaked.
 */
void TestFMPClient::testFetchSyncQuoteShortWithFakeNetworkLatency()
{
    FMPClient& client = FMPClient::getInstance();
    FMPClient::QuoteShortResult quoteResult;
    bool success;

    QVERIFY(client.isCleanedUp());

    client.simulate_reply_network_latency = true;
    {
        unsigned int tmp = client.fetchSyncTimeoutMs;
        client.fetchSyncTimeoutMs = 1000; // Accelerate the unit test

        QVERIFY(0 == client.onReplyFinished_sem.available());

        success = client.fetchSyncQuoteShort("AAPL", quoteResult);

        // The timeout for a sync fetch is FMPClient::fetchSyncTimeoutMs, and the activated internal delay to the FMPClient thread is +1000ms of that.
        // Wait here until the object notifies us it completed its onReplyFinished()
        // We should technically have to wait for 1000ms, give 2000ms for 1000ms of room.
        // We WANT to wait this extra second in order to test how the rest of onReplyFinished() handles
        // the case whese the fetchSyncQuote() timedout and bailed.
        bool notified = client.onReplyFinished_sem.tryAcquire(1, 2000);

        // We SHOULD have been notified within the 1000ms extra room!
        QVERIFY(notified);

        client.fetchSyncTimeoutMs = tmp; // Back to normal
    }
    client.simulate_reply_network_latency = false;

    QVERIFY2(success == false, "Sync fetch SHOULD fail or time out");
    QVERIFY(quoteResult.price > 0.0);
    QVERIFY(quoteResult.change >= 0.0);
    QVERIFY(quoteResult.volume >= 0);

    // Particularly important here, this is what tests that onReplyFinished() properly handled
    // fetchSyncQuote() timing out and bailing.
    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchAsyncQuoteShortWithFakeNetworkLatency()
{
    FMPClient& client = FMPClient::getInstance();

    QVERIFY(client.isCleanedUp());

    QSignalSpy spy(&client, &FMPClient::quoteShortReceived);

    QVERIFY(spy.isValid());

    client.simulate_reply_network_latency = true;
    {
        bool triggered;
        unsigned int tmp = client.fetchSyncTimeoutMs;
        client.fetchSyncTimeoutMs = 1000; // Accelerate the unit test

        QVERIFY(0 == client.onReplyFinished_sem.available());

        client.fetchAsyncQuoteShort("AAPL");

        triggered = spy.wait(500);
        QVERIFY(false == triggered);

        triggered = spy.wait(3000); // TODO explain why 3000ms
        QVERIFY(triggered);


        // The timeout for a sync fetch is FMPClient::fetchSyncTimeoutMs, and the activated internal delay to the FMPClient thread is +1000ms of that.
        // Wait here until the object notifies us it completed its onReplyFinished()
        // We should technically have to wait for 1000ms, give 2000ms for 1000ms of room.
        // We WANT to wait this extra second in order to test how the rest of onReplyFinished() handles
        // the case whese the fetchSyncQuote() timedout and bailed.
        bool notified = client.onReplyFinished_sem.tryAcquire(1, 2000);

        // We SHOULD have been notified within the 1000ms extra room!
        QVERIFY(notified);

        client.fetchSyncTimeoutMs = tmp; // Back to normal
    }
    client.simulate_reply_network_latency = false;

    QVERIFY2(spy.count() == 1, "Async fetch did not emit signal");
    QList<QVariant> arguments = spy.takeFirst();
    FMPClient::QuoteShortResult quoteResult = arguments.at(0).value<FMPClient::QuoteShortResult>();
    
    QVERIFY(quoteResult.symbol == "AAPL");
    QVERIFY(quoteResult.price > 0.0);
    QVERIFY(quoteResult.change != 0.0);
    QVERIFY(quoteResult.volume >= 0);

    // Particularly important here, this is what tests that onReplyFinished() properly handled
    // fetchSyncQuote() timing out and bailing.
    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchFloatFrom100CompaniesMixingSyncAndAsync()
{
    FMPClient& client = FMPClient::getInstance();
    bool success;
    QVector<CompanyScreenerResult> resultsScreen100Companies;

    QVERIFY(client.isCleanedUp());

    const int numberOfCompaniesToFetchFloat = 100;
    {
        CompanyScreenerFilter filter;

        filter.setIndustry("Biotechnology");
        filter.setPriceMoreThan(2.0);
        filter.setPriceLowerThan(20.0);
        filter.setExchange("NASDAQ");
        filter.setIsActivelyTrading(true);
        filter.setIsEtf(false);
        filter.setIsFund(false);
        filter.setCountry("US");
        filter.setVolumeMoreThan(10000);

        filter.setLimit(numberOfCompaniesToFetchFloat); // <-- ***

        success = client.fetchSyncCompanyScreener(filter, resultsScreen100Companies);

        QVERIFY2(success, "Sync fetch failed or timed out");

        qDebug() << "Company-Screener produced " << resultsScreen100Companies.size() << " results";

        QCOMPARE(resultsScreen100Companies.size(), numberOfCompaniesToFetchFloat);
    }

    QLoggingCategory::setFilterRules("FMPClient.debug=false");
    {
        QElapsedTimer timer;
        QSignalSpy spy(&client, &FMPClient::sharesFloatReceived);

        qInfo() << "Fetch the float of those companies in ASYNC fashion";
        {
            QVERIFY(spy.isValid());

            timer.start();
            {
                for (size_t i = 0; i < numberOfCompaniesToFetchFloat; i++) {
                    client.fetchAsyncSharesFloat(resultsScreen100Companies.at(1).getSymbol());
                }

                // The compare with timeout of the number of async replies will be done AFTER the sync
                // block following. The reason is that we want the requests to be mixed, not all of the
                // async finishing due to the compare-wait
            }
            qInfo() << "Time elapsed:" << timer.elapsed() << "ms";
        }

        qInfo() << "Fetch the float of those companies in SYNC fashion";
        {
            timer.start();
            {
                FMPClient::SharesFloatResult floatResult;

                for (size_t i = 0; i < numberOfCompaniesToFetchFloat; i++) {
                    success = client.fetchSyncSharesFloat(resultsScreen100Companies.at(1).getSymbol(),
                                                          floatResult);
                    QVERIFY(success);
                }
            }
            qInfo() << "Time elapsed:" << timer.elapsed() << "ms";
        }

        // The async compare is made at the end
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(),
                                  numberOfCompaniesToFetchFloat,
                                  20000);
    }
    QLoggingCategory::setFilterRules("FMPClient.debug=true");

    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchingMoreThanMaximumPerMinute()
{
    qInfo() << "Turned of qCDebug(FMPClient.debug) for this test so as to not flood the console.";

    // Suppress debug prints for this test as we will do a huge number of requests
    QLoggingCategory::setFilterRules("FMPClient.debug=false");


    FMPClient& client = FMPClient::getInstance();

    QVERIFY(client.isCleanedUp());

    QSignalSpy spy(&client, &FMPClient::quoteShortReceived);

    QVERIFY(spy.isValid());

    const qsizetype APIMaxCallsPerMinute = 750;
    {
        qInfo() << "Expect a couple of 'Error with the reply' :";
        // Launch max + 1 as fast as possible
        for(size_t i = 0; i != APIMaxCallsPerMinute + 1; i++) {
            client.fetchAsyncQuoteShort("AAPL");
        }

        // 5s should be enough to perform those 701 requests
        QTest::qWait(5000);
    }

    qDebug() << "Made " << spy.count() << " requests";
    QVERIFY2(spy.count() > (APIMaxCallsPerMinute/2), "Expecting at *least* half the requests succeeded and made it to the slot");
    QVERIFY2(spy.count() <= APIMaxCallsPerMinute, "Should not have been able to perform 1 more request than 750");

    QVERIFY(client.isCleanedUp());

}
