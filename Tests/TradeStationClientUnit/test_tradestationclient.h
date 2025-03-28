#ifndef TEST_TRADESTATIONCLIENT_H
#define TEST_TRADESTATIONCLIENT_H

#include <QtTest/QtTest>

class TestTradeStationClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();

    void testFetchingMoreThanMaximumPerMinute();
    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail
};

#endif // TEST_TRADESTATIONCLIENT_H 