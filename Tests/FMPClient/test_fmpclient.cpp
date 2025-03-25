#include <QDebug>
#include "test_fmpclient.h"

extern QString fmpKey;

void TestFMPClient::initTestCase() {
    qDebug() << "Start of test suite";
}

void TestFMPClient::testFetchQuoteSync() {
    FMPClient client(fmpKey);
    double price, bid, ask;

    bool success = client.fetchQuoteSync("AAPL", price, bid, ask);
    QVERIFY2(success, "Sync fetch failed or timed out");
    QVERIFY(price > 0.0);
    QVERIFY(bid >= 0.0);
    QVERIFY(ask >= 0.0);
}

void TestFMPClient::testFetchQuoteAsync() {
    FMPClient client(fmpKey);
    QSignalSpy spy(&client, &FMPClient::quoteReceived);

    client.fetchQuoteAsync("AAPL");
    QTest::qWait(2000);

    QVERIFY2(spy.count() == 1, "Async fetch did not emit signal");
    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).toDouble() > 0.0);
    QVERIFY(arguments.at(1).toDouble() >= 0.0);
    QVERIFY(arguments.at(2).toDouble() >= 0.0);
}
