#ifndef TEST_TRADESTATIONCLIENT_H
#define TEST_TRADESTATIONCLIENT_H

#include <QtTest/QtTest>
#include <QSettings>

class TestTradeStationClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase_data();
    void initTestCase();
    void init();
    void cleanup();

    // Need to be first
    void testRefreshSyncAccessToken();

    void testIsAlreadyAuthenticatedReturnsFalseWhenNoTokens();
    void testIsAlreadyAuthenticatedReturnsFalseWhenTokenExpired();

    void testFetchingMoreThanMaximumPerMinute();
    void testFetchSyncAccounts();

    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail

private:
};

#endif // TEST_TRADESTATIONCLIENT_H 
