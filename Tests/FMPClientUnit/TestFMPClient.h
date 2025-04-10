#pragma once

#include <QObject>

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
    void testFetchSyncStockNews();

    void testFetchSyncQuoteShortWithFakeNetworkLatency();
    void testFetchAsyncQuoteShortWithFakeNetworkLatency();

    void testFetchFloatFrom100CompaniesMixingSyncAndAsync();

    void testFetchingMoreThanMaximumPerMinute();
    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail
};
