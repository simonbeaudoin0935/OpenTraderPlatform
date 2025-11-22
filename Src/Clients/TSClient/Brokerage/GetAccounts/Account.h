#pragma once

#include <QString>
#include <QJsonObject>
#include <optional>


class AccountType {

public:
    enum class Type {
        Cash,
        Margin,
        Futures,
        DVP
    };

    AccountType() {};

    AccountType(const QString &type) : type(stringToAccountType(type)) {}

    static QString accountTypeToString(Type type) {
        switch (type) {
        case Type::Cash: return "Cash";
        case Type::Margin: return "Margin";
        case Type::Futures: return "Futures";
        case Type::DVP: return "DVP";
        default: Q_UNREACHABLE();
        }
    }

    static Type stringToAccountType(const QString &str) {
        if (str == "Cash") return Type::Cash;
        else if (str == "Margin") return Type::Margin;
        else if (str == "Futures") return Type::Futures;
        else if (str == "DVP") return Type::DVP;
        else Q_ASSERT(0);
    }

    Type type;
};



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
    AccountType getAccountType() const { return accountType; }
    QString getStatus() const { return status; }
    QString getCurrency() const { return currency; }
    const std::optional<AccountDetail>& getAccountDetail() const { return accountDetail; }

    QString toJsonString() const;
    bool isValid() const;

private:
    QString accountId;
    AccountType accountType;
    QString status;
    QString currency;
    std::optional<AccountDetail> accountDetail;
};

// Necessary to be able to use a QSignalSpy and intercept the emition of a QVector<AccountsResult>
Q_DECLARE_METATYPE(Account)
Q_DECLARE_METATYPE(QVector<Account>)
