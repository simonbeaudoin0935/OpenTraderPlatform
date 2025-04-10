#include <QJsonDocument>

#include "Position.h"

Position::Position(const QJsonObject& jsonObj, bool isUpdate_) : isUpdate(isUpdate_) {
    accountID = jsonObj["AccountID"].toString();
    assetType = jsonObj["AssetType"].toString();
    averagePrice = jsonObj["AveragePrice"].toString();
    bid = jsonObj["Bid"].toString();
    ask = jsonObj["Ask"].toString();
    conversionRate = jsonObj["ConversionRate"].toString();
    deleted = jsonObj["Deleted"].toBool(false);
    dayTradeRequirement = jsonObj["DayTradeRequirement"].toString();
    
    if (jsonObj.contains("ExpirationDate")) {
        expirationDate = QDateTime::fromString(jsonObj["ExpirationDate"].toString(), Qt::ISODate);
    }
    
    initialRequirement = jsonObj["InitialRequirement"].toString();
    maintenanceMargin = jsonObj["MaintenanceMargin"].toString();
    last = jsonObj["Last"].toString();
    longShort = jsonObj["LongShort"].toString();
    markToMarketPrice = jsonObj["MarkToMarketPrice"].toString();
    marketValue = jsonObj["MarketValue"].toString();
    positionID = jsonObj["PositionID"].toString();
    quantity = jsonObj["Quantity"].toString();
    symbol = jsonObj["Symbol"].toString();
    timestamp = QDateTime::fromString(jsonObj["Timestamp"].toString(), Qt::ISODate);
    todaysProfitLoss = jsonObj["TodaysProfitLoss"].toString();
    totalCost = jsonObj["TotalCost"].toString();
    unrealizedProfitLoss = jsonObj["UnrealizedProfitLoss"].toString();
    unrealizedProfitLossPercent = jsonObj["UnrealizedProfitLossPercent"].toString();
    unrealizedProfitLossQty = jsonObj["UnrealizedProfitLossQty"].toString();
}

bool Position::isValid() const {
    // Check required fields
    if (accountID.isEmpty() || assetType.isEmpty() || averagePrice.isEmpty() ||
        bid.isEmpty() || ask.isEmpty() || conversionRate.isEmpty() ||
        dayTradeRequirement.isEmpty() || initialRequirement.isEmpty() ||
        maintenanceMargin.isEmpty() || last.isEmpty() || longShort.isEmpty() ||
        markToMarketPrice.isEmpty() || marketValue.isEmpty() || positionID.isEmpty() ||
        quantity.isEmpty() || symbol.isEmpty() || !timestamp.isValid() ||
        todaysProfitLoss.isEmpty() || totalCost.isEmpty() ||
        unrealizedProfitLoss.isEmpty() || unrealizedProfitLossPercent.isEmpty() ||
        unrealizedProfitLossQty.isEmpty()) {
        return false;
    }

    // Validate asset type
    if (assetType != "STOCK" && assetType != "STOCKOPTION" && 
        assetType != "FUTURE" && assetType != "INDEXOPTION") {
        return false;
    }

    // Validate position direction
    if (longShort != "Long" && longShort != "Short") {
        return false;
    }

    return true;
}

QString Position::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["AccountID"] = accountID;
    jsonObj["AssetType"] = assetType;
    jsonObj["AveragePrice"] = averagePrice;
    jsonObj["Bid"] = bid;
    jsonObj["Ask"] = ask;
    jsonObj["ConversionRate"] = conversionRate;
    if (deleted) {
        jsonObj["Deleted"] = deleted;
    }
    jsonObj["DayTradeRequirement"] = dayTradeRequirement;
    if (expirationDate.isValid()) {
        jsonObj["ExpirationDate"] = expirationDate.toString(Qt::ISODate);
    }
    jsonObj["InitialRequirement"] = initialRequirement;
    jsonObj["MaintenanceMargin"] = maintenanceMargin;
    jsonObj["Last"] = last;
    jsonObj["LongShort"] = longShort;
    jsonObj["MarkToMarketPrice"] = markToMarketPrice;
    jsonObj["MarketValue"] = marketValue;
    jsonObj["PositionID"] = positionID;
    jsonObj["Quantity"] = quantity;
    jsonObj["Symbol"] = symbol;
    jsonObj["Timestamp"] = timestamp.toString(Qt::ISODate);
    jsonObj["TodaysProfitLoss"] = todaysProfitLoss;
    jsonObj["TotalCost"] = totalCost;
    jsonObj["UnrealizedProfitLoss"] = unrealizedProfitLoss;
    jsonObj["UnrealizedProfitLossPercent"] = unrealizedProfitLossPercent;
    jsonObj["UnrealizedProfitLossQty"] = unrealizedProfitLossQty;
    jsonObj["IsUpdate"] = isUpdate;  // Include the update flag in JSON output

    QJsonDocument doc(jsonObj);
    return doc.toJson(QJsonDocument::Compact);
} 
