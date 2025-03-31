#include <QJsonDocument>

#include "AccountsResult.h"

AccountsResult::AccountsResult(const QJsonObject& jsonObj) {
    accountId = jsonObj["AccountID"].toString();
    accountType = jsonObj["AccountType"].toString();
    status = jsonObj["Status"].toString();
    currency = jsonObj["Currency"].toString();
    
    // AccountDetail is optional, only parse if it exists
    if (jsonObj.contains("AccountDetail")) {
        accountDetail = AccountDetail(jsonObj["AccountDetail"].toObject());
    }
}

QString AccountsResult::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["AccountID"] = accountId;
    jsonObj["AccountType"] = accountType;
    jsonObj["Status"] = status;
    jsonObj["Currency"] = currency;
    
    // Only include AccountDetail if it exists and has any non-default values
    if (accountDetail.has_value()) {
        const auto& detail = accountDetail.value();
        if (detail.isStockLocateEligible || 
            detail.enrolledInRegTProgram || 
            detail.requiresBuyingPowerWarning || 
            detail.dayTradingQualified || 
            detail.optionApprovalLevel != 0 || 
            detail.patternDayTrader) {
            
            QJsonObject detailObj;
            detailObj["IsStockLocateEligible"] = detail.isStockLocateEligible;
            detailObj["EnrolledInRegTProgram"] = detail.enrolledInRegTProgram;
            detailObj["RequiresBuyingPowerWarning"] = detail.requiresBuyingPowerWarning;
            detailObj["DayTradingQualified"] = detail.dayTradingQualified;
            detailObj["OptionApprovalLevel"] = detail.optionApprovalLevel;
            detailObj["PatternDayTrader"] = detail.patternDayTrader;
            
            jsonObj["AccountDetail"] = detailObj;
        }
    }

    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
} 
