#pragma once

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

    void testGetAccountsSync();
    void testGetAccountsAsync();

    void testGetQuoteSnapshotsSync();
    void testGetQuoteSnapshotsAsync();

    void testPlaceSyncOrder();
    void testPlaceAsyncOrder();

    void testFetchingMoreThanMaximumPerMinute();

    void testGetBarsAsync();
    void testGetBarsSync();

    void testStreamBars();
    void testStreamMarketDepthQuote();



    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail

private:
    QString secondAccountId; // Store the second account's ID for use in place order test
};
