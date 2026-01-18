#pragma once

#include <QObject>

#include "Account.h"

class Balance
{

  public:
    Balance() = default;
    explicit Balance(const QJsonObject& jsonObj);


    QString getAccountID() const
    {
        return accountID;
    }
    AccountType getAccountType() const
    {
        return accountType;
    }
    double getBuyingPower() const
    {
        return buyingPower;
    }
    double getCashBalance() const
    {
        return cashBalance;
    }
    double getComission() const
    {
        return comission;
    }
    double getEquity() const
    {
        return equity;
    }
    double getMarketValue() const
    {
        return marketValue;
    }
    double getTodaysProfitLoss() const
    {
        return todaysProfitLoss;
    }
    double getUnclearedDeposit() const
    {
        return unclearedDeposit;
    }

    QString toJsonString() const;

  private:
    QString accountID;
    AccountType accountType;
    //Balance detail
    double buyingPower;
    double cashBalance;
    double comission;
    //currency details
    double equity;
    double marketValue;
    double todaysProfitLoss;
    double unclearedDeposit;
};
