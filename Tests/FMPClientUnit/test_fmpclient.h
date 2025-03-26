#ifndef TEST_FMPCLIENT_H
#define TEST_FMPCLIENT_H

#include <QtTest/QtTest>
#include "../../FMPClient/fmpclient.h"

class TestFMPClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();

    void testFetchQuoteSync();
    void testFetchQuoteAsync();

    void testFetchSharesFloatSync();

    void testFetchQuoteSyncWithFake6sNetworkLatency();

};

#endif // TEST_FMPCLIENT_H
