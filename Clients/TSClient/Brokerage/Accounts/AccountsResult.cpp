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

bool AccountsResult::isValid() const {
    // Check required fields
    if (accountId.isEmpty()) {
        qWarning() << "AccountID is required but not set";
        return false;
    }

    if (accountType.isEmpty()) {
        qWarning() << "AccountType is required but not set";
        return false;
    }

    if (status.isEmpty()) {
        qWarning() << "Status is required but not set";
        return false;
    }

    if (currency.isEmpty()) {
        qWarning() << "Currency is required but not set";
        return false;
    }

    // Validate status values
    if (status != "Active" && status != "Pending" && status != "Closed") {
        qWarning() << "Invalid status value:" << status;
        return false;
    }

    // Validate account type values
    if (accountType != "Cash" && accountType != "Margin" && accountType != "Futures" && accountType != "DVP") {
        qWarning() << "Invalid account type:" << accountType;
        return false;
    }

    // Validate currency format (should be 3-letter code)
    if (currency.length() != 3) {
        qWarning() << "Invalid currency format:" << currency;
        return false;
    }

    // If AccountDetail is present, validate its fields
    if (accountDetail.has_value()) {
        // No specific validation needed for AccountDetail fields
        // as they are all boolean flags except optionApprovalLevel
        // which can have any non-negative value
    }

    return true;
}
