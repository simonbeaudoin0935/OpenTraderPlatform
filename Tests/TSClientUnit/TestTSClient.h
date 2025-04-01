#ifndef TEST_TRADESTATIONCLIENT_H
#define TEST_TRADESTATIONCLIENT_H

#include <QObject>

class TestTSClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase_data();
    void initTestCase();
    void init();
    void cleanup();

    // Need to be first
    void testRefreshSyncAccessToken();

    void testFetchSyncAccounts();
    void testFetchAsyncAccounts();

    void testPlaceSyncOrder();
    void testPlaceAsyncOrder();

    void testFetchingMoreThanMaximumPerMinute();
    void testStreamMarketDepthQuote();

    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail

private:
    bool isMarketClosed();

    QString secondAccountId; // Store the second account's ID for use in place order test
};

#endif // TEST_TRADESTATIONCLIENT_H 
