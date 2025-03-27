#ifndef TEST_FMPCLIENT_H
#define TEST_FMPCLIENT_H

#include <QtTest/QtTest>

class TestFMPClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();

    void init();

    // TODO add test about giving a bad symbol name

    void testFetchSyncQuoteShort();
    void testFetchAsyncQuoteShort();

    void testFetchSyncSharesFloat();
    void testFetchAsyncSharesFloat();

    void testFetchSyncCompanyScreener();


    void testFetchSyncQuoteShortWithFakeNetworkLatency();
    void testFetchAsyncQuoteShortWithFakeNetworkLatency();

    void testFetchingMoreThanMaximumPerMinute();
    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail
};

#endif // TEST_FMPCLIENT_H
