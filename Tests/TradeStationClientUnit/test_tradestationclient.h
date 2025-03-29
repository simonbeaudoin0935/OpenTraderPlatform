#ifndef TEST_TRADESTATIONCLIENT_H
#define TEST_TRADESTATIONCLIENT_H

#include <QtTest/QtTest>
#include <QSettings>
#include "../../Clients/TradeStationClient/Auth/authtoken.h"

class TestTradeStationClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase_data();
    void initTestCase();
    void init();

    void testIsAlreadyAuthenticatedReturnsFalseWhenNoTokens();
    void testIsAlreadyAuthenticatedReturnsFalseWhenTokenExpired();

    void testAuthentication();
    void testFetchingMoreThanMaximumPerMinute();
    void testFetchSyncAccounts();

    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail

private:
    AuthToken savedValidAuthToken;
};

#endif // TEST_TRADESTATIONCLIENT_H 
