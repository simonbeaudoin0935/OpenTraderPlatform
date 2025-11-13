#pragma once

#include <QObject>

class TestTSClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase_data();
    void initTestCase();
    void init();
    void cleanup();



    void testGetAccountsSync();
    void testGetAccountsAsync();

    void testGetBalancesSync();
    void testGetBalancesAsync();

    void testGetQuoteSnapshotsSync();
    void testGetQuoteSnapshotsAsync();

    void testPlaceOrderSync();
    void testPlaceOrderAsync();

    void testCancelOrderSync();
    void testCancelOrderAsync();

    void testFetchingMoreThanMaximumPerMinute();

    void testGetBarsSync();
    void testGetBarsAsync();

    void testStreamBars();
    void testStreamBarsRecording();

    void testStreamMarketDepthQuote();
    void testStreamOrders();
    void testStreamPositions();

    void testMockStreamBars();
    void testStreamCount();

    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail

private:
    // Needs to be first
    void testRefreshSyncAccessToken();

    QString secondAccountId; // Store the second account's ID for use in place order test
};
