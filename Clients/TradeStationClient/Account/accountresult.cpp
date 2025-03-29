#include "accountresult.h"
#include <QJsonDocument>

AccountResult::AccountResult(const QJsonObject& jsonObj) {
    accountId = jsonObj["AccountID"].toString();
    accountType = jsonObj["AccountType"].toString();
    displayName = jsonObj["DisplayName"].toString();
    status = jsonObj["Status"].toString();
    isActive = jsonObj["IsActive"].toBool();
    isPrimary = jsonObj["IsPrimary"].toBool();
    currency = jsonObj["Currency"].toString();
    currentBalance = jsonObj["CurrentBalance"].toDouble();
    availableBalance = jsonObj["AvailableBalance"].toDouble();
    dayTradingBuyingPower = jsonObj["DayTradingBuyingPower"].toDouble();
    dayTradingEquity = jsonObj["DayTradingEquity"].toDouble();
    initialMargin = jsonObj["InitialMargin"].toDouble();
    maintenanceMargin = jsonObj["MaintenanceMargin"].toDouble();
    lastUpdated = jsonObj["LastUpdated"].toDouble();
}

QString AccountResult::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["AccountID"] = accountId;
    jsonObj["AccountType"] = accountType;
    jsonObj["DisplayName"] = displayName;
    jsonObj["Status"] = status;
    jsonObj["IsActive"] = isActive;
    jsonObj["IsPrimary"] = isPrimary;
    jsonObj["Currency"] = currency;
    jsonObj["CurrentBalance"] = currentBalance;
    jsonObj["AvailableBalance"] = availableBalance;
    jsonObj["DayTradingBuyingPower"] = dayTradingBuyingPower;
    jsonObj["DayTradingEquity"] = dayTradingEquity;
    jsonObj["InitialMargin"] = initialMargin;
    jsonObj["MaintenanceMargin"] = maintenanceMargin;
    jsonObj["LastUpdated"] = lastUpdated;

    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
} 