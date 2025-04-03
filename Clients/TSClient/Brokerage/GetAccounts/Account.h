#ifndef ACCOUNTRESULT_H
#define ACCOUNTRESULT_H

#include <QString>
#include <QJsonObject>
#include <optional>

struct AccountDetail {
    AccountDetail() = default;
    AccountDetail(const QJsonObject& jsonObj) {
        isStockLocateEligible = jsonObj["IsStockLocateEligible"].toBool();
        enrolledInRegTProgram = jsonObj["EnrolledInRegTProgram"].toBool();
        requiresBuyingPowerWarning = jsonObj["RequiresBuyingPowerWarning"].toBool();
        dayTradingQualified = jsonObj["DayTradingQualified"].toBool();
        optionApprovalLevel = jsonObj["OptionApprovalLevel"].toInt();
        patternDayTrader = jsonObj["PatternDayTrader"].toBool();
    }

    bool isStockLocateEligible = false;
    bool enrolledInRegTProgram = false;
    bool requiresBuyingPowerWarning = false;
    bool dayTradingQualified = false;
    int optionApprovalLevel = 0;
    bool patternDayTrader = false;
};

struct Account {
    Account() = default;
    Account(const QJsonObject& jsonObj);

    // Getters for each member
    QString getAccountId() const { return accountId; }
    QString getAccountType() const { return accountType; }
    QString getStatus() const { return status; }
    QString getCurrency() const { return currency; }
    const std::optional<AccountDetail>& getAccountDetail() const { return accountDetail; }

    QString toJsonString() const;
    bool isValid() const;

private:
    QString accountId;
    QString accountType;
    QString status;
    QString currency;
    std::optional<AccountDetail> accountDetail;
};

// Necessary to be able to use a QSignalSpy and intercept the emition of a QVector<AccountsResult>
Q_DECLARE_METATYPE(Account)
Q_DECLARE_METATYPE(QVector<Account>)

#endif // ACCOUNTRESULT_H 
