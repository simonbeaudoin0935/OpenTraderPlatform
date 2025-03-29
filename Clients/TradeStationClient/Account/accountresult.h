#ifndef ACCOUNTRESULT_H
#define ACCOUNTRESULT_H

#include <QString>
#include <QJsonObject>

struct AccountResult {
    AccountResult(const QJsonObject& jsonObj);

    // Getters for each member
    QString getAccountId() const { return accountId; }
    QString getAccountType() const { return accountType; }
    QString getDisplayName() const { return displayName; }
    QString getStatus() const { return status; }
    bool getIsActive() const { return isActive; }
    bool getIsPrimary() const { return isPrimary; }
    QString getCurrency() const { return currency; }
    double getCurrentBalance() const { return currentBalance; }
    double getAvailableBalance() const { return availableBalance; }
    double getDayTradingBuyingPower() const { return dayTradingBuyingPower; }
    double getDayTradingEquity() const { return dayTradingEquity; }
    double getInitialMargin() const { return initialMargin; }
    double getMaintenanceMargin() const { return maintenanceMargin; }
    double getLastUpdated() const { return lastUpdated; }

    QString toJsonString() const;

private:
    QString accountId;
    QString accountType;
    QString displayName;
    QString status;
    bool isActive;
    bool isPrimary;
    QString currency;
    double currentBalance;
    double availableBalance;
    double dayTradingBuyingPower;
    double dayTradingEquity;
    double initialMargin;
    double maintenanceMargin;
    double lastUpdated;
};

#endif // ACCOUNTRESULT_H 