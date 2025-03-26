#ifndef TEST_FMPCLIENT_H
#define TEST_FMPCLIENT_H

#include <QtTest/QtTest>

class TestFMPClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();

    // TODO add test about giving a bad symbol name

    void testFetchSyncQuote();
    void testFetchAsyncQuote();

    void testFetchSyncSharesFloat();

    void testFetchSyncQuoteWithFakeNetworkLatency();
    void testFetchAsyncQuoteWithFakeNetworkLatency();

};

#endif // TEST_FMPCLIENT_H
