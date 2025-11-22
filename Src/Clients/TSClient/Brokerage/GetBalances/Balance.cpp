#include <QJsonDocument>

#include "Balance.h"

Balance::Balance(const QJsonObject &jsonObj)
{
    accountID = jsonObj["AccountID"].toString();
    accountType = AccountType(jsonObj["AccountType"].toString());
    buyingPower = jsonObj["BuyingPower"].toString().toDouble();
    cashBalance = jsonObj["CashBalance"].toString().toDouble();
    comission = jsonObj["Comission"].toString().toDouble();
    equity = jsonObj["Equity"].toString().toDouble();
    marketValue = jsonObj["MarketValue"].toString().toDouble();
    todaysProfitLoss = jsonObj["TodaysProfitLoss"].toString().toDouble();
    unclearedDeposit = jsonObj["UnclearedDeposit"].toString().toDouble();
}

QString Balance::toJsonString() const
{
    QJsonObject jsonObj;

    jsonObj["AccountID"] = accountID;
    jsonObj["AccountType"] = AccountType::accountTypeToString(accountType.type);
    jsonObj["BuyingPower"] = buyingPower;
    jsonObj["cashBalance"] = cashBalance;
    jsonObj["Comission"] = comission;
    jsonObj["Equity"] = equity;
    jsonObj["MarketValue"] = marketValue;
    jsonObj["TodaysProfitLoss"] = todaysProfitLoss;
    jsonObj["UnclearedDeposit"] = unclearedDeposit;

    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}
