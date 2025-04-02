#ifndef APPFRONTEND_H
#define APPFRONTEND_H

#include <QObject>
#include <QJsonObject>
#include "../Clients/TSClient/Brokerage/Accounts/AccountsResult.h"

class AppFrontend : public QObject {
    Q_OBJECT
public:
    explicit AppFrontend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~AppFrontend() = default;

signals:
    void tradeStationAuthStateChanged(bool isAuthenticated, QString reason);
    void tradeStationAccountsReceived(QVector<AccountsResult> results);
    void marketDepthNotAvailable();

    void fmpDataUsageUpdated(qsizetype newDataUsage);
    void tradeStationDataUsageUpdated(qsizetype newDataUsage);


public slots:

    // Usage uptade
    virtual void onFMPClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onTSClientDataUsageUpdate(qsizetype newDataUsage) = 0;
    virtual void onMemoryUsageUpdate(qint64 newDataUsage) = 0;

    virtual void onTradeStationAccountsReceived(QVector<AccountsResult> results) = 0;
    virtual void onMarketDepthNotAvailable() = 0;
};

#endif
