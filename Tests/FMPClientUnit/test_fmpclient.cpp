#include <QDebug>
#include "test_fmpclient.h"
#include "../../FMPClient/fmpclient.h"

extern QString fmpKey;


// Unit test of the FMPClient singleton. Having it as a singleton is somewhat
// handy for testing as the same object is used between tests, further confirming
// the good behavior between tests without having to add tests that test when the
// same object is reused.

void TestFMPClient::initTestCase() {
    QLoggingCategory::setFilterRules("FMPClient.debug=true");

    qInfo() << "Start of test suite";

    FMPClient::setAPIKey(fmpKey);

}

void TestFMPClient::testFetchSyncQuote() {
    FMPClient& client = FMPClient::getInstance();
    double price, change;
    qsizetype volume;
    bool success;

    QVERIFY(client.isCleanedUp());

    {
        success = client.fetchSyncQuoteShort("AAPL", price, change, volume);
    }

    QVERIFY2(success, "Sync fetch failed or timed out");
    QVERIFY(price > 0.0);
    QVERIFY(change != 0.0);
    QVERIFY(volume >= 0);

    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchAsyncQuote() {
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
    QVERIFY(arguments.at(0).toString() == "AAPL");
    QVERIFY(arguments.at(1).toDouble() > 0.0);  // price
    QVERIFY(arguments.at(2).toDouble() != 0.0); // change
    QVERIFY(arguments.at(3).toInt() >= 0); // volume

    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchSyncSharesFloat()
{
    FMPClient& client = FMPClient::getInstance();
    QString date;
    double freeFloat, floatShares, outstandingShares;
    bool success;

    QVERIFY(client.isCleanedUp());

    {
        success = client.fetchSyncSharesFloat("AAPL", date, freeFloat, floatShares, outstandingShares);
    }

    QVERIFY2(success, "Sync fetch failed or timed out");
    QVERIFY(!date.isEmpty());
    QVERIFY(freeFloat >= 0.0 && freeFloat < 100.0);
    QVERIFY(floatShares >= 0.0);
    QVERIFY(outstandingShares >= 0.0);

    QVERIFY(client.isCleanedUp());
}

/*
 * Testing that when the fetcher times out because the reply took too long to arrive,
 * that no resources are leaked.
 */
void TestFMPClient::testFetchSyncQuoteWithFakeNetworkLatency()
{
    FMPClient& client = FMPClient::getInstance();
    double price, change;
    qsizetype volume;
    bool success;

    QVERIFY(client.isCleanedUp());

    client.simulate_reply_network_latency = true;
    {
        unsigned int tmp = client.fetchSyncTimeoutMs;
        client.fetchSyncTimeoutMs = 1000; // Accelerate the unit test

        QVERIFY(0 == client.onReplyFinished_sem.available());

        success = client.fetchSyncQuoteShort("AAPL", price, change, volume);

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
    QVERIFY(price > 0.0);
    QVERIFY(change >= 0.0);
    QVERIFY(volume >= 0);

    // Particularly important here, this is what tests that onReplyFinished() properly handled
    // fetchSyncQuote() timing out and bailing.
    QVERIFY(client.isCleanedUp());
}

void TestFMPClient::testFetchAsyncQuoteWithFakeNetworkLatency()
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
    QVERIFY(arguments.at(0).toString() == "AAPL");
    QVERIFY(arguments.at(1).toDouble() > 0.0);  // price
    QVERIFY(arguments.at(2).toDouble() != 0.0); // change
    QVERIFY(arguments.at(3).toInt() >= 0); // volume

    // Particularly important here, this is what tests that onReplyFinished() properly handled
    // fetchSyncQuote() timing out and bailing.
    QVERIFY(client.isCleanedUp());
}
