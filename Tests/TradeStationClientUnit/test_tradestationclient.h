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

    void testIsAlreadyAuthenticatedReturnsFalseWhenNoTokens();
    void testIsAlreadyAuthenticatedReturnsFalseWhenTokenExpired();

    void testAuthentication();
    void testFetchingMoreThanMaximumPerMinute();
    void testFetchSyncAccounts();

    //TODO should perhaps implement a max-per-minute limiter to queue the exceeding requests
    //     for the next minute instead of having them fail

private:
    struct AuthData {
        QString accessToken;
        QString idToken;
        QString refreshToken;
        QString tokenReceivedTime;
        int tokenTimeoutSeconds;
    };
    
    AuthData readExistingAuthData();
    void restoreAuthData(const AuthData& data);
    AuthData originalAuthData;  // Store the original auth data
};

#endif // TEST_TRADESTATIONCLIENT_H 