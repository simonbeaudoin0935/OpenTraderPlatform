#include <QJsonDocument>

#include "Position.h"
#include "Assume.h"

Position::Position(const QJsonObject& jsonObj, bool isUpdate_) : isUpdate(isUpdate_)
{
    accountID = jsonObj["AccountID"].toString();
    assetType = jsonObj["AssetType"].toString();
    averagePrice = jsonObj["AveragePrice"].toString();
    bid = jsonObj["Bid"].toString();
    ask = jsonObj["Ask"].toString();
    conversionRate = jsonObj["ConversionRate"].toString();
    deleted = jsonObj["Deleted"].toBool(false);
    dayTradeRequirement = jsonObj["DayTradeRequirement"].toString();

    if (jsonObj.contains("ExpirationDate"))
    {
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

bool Position::isValid() const
{
    // Check required fields - these should NEVER be empty
    if (accountID.isEmpty())
    {
        qDebug() << "Position invalid: accountID empty";
    }
    ASSUME_FALSE(accountID.isEmpty());  // ASSERT: accountID must not be empty
    
    if (assetType.isEmpty())
    {
        qDebug() << "Position invalid: assetType empty";
        
    }
    ASSUME_FALSE(assetType.isEmpty());  // ASSERT: assetType must not be empty
    
    if (averagePrice.isEmpty())
    {
        qDebug() << "Position invalid: averagePrice empty";
        
    }
    ASSUME_FALSE(averagePrice.isEmpty());  // ASSERT: averagePrice must not be empty
    
    if (bid.isEmpty())
    {
        qDebug() << "Position invalid: bid empty";
        
    }
    ASSUME_FALSE(bid.isEmpty());  // ASSERT: bid must not be empty
    
    if (ask.isEmpty())
    {
        qDebug() << "Position invalid: ask empty";
        
    }
    ASSUME_FALSE(ask.isEmpty());  // ASSERT: ask must not be empty
    
    if (conversionRate.isEmpty())
    {
        qDebug() << "Position invalid: conversionRate empty";
        
    }
    ASSUME_FALSE(conversionRate.isEmpty());  // ASSERT: conversionRate must not be empty
    
    if (dayTradeRequirement.isEmpty())
    {
        qDebug() << "Position invalid: dayTradeRequirement empty";
        
    }
    ASSUME_FALSE(dayTradeRequirement.isEmpty());  // ASSERT: dayTradeRequirement must not be empty
    
    if (initialRequirement.isEmpty())
    {
        qDebug() << "Position invalid: initialRequirement empty";
        
    }
    ASSUME_FALSE(initialRequirement.isEmpty());  // ASSERT: initialRequirement must not be empty
    
    if (maintenanceMargin.isEmpty())
    {
        qDebug() << "Position invalid: maintenanceMargin empty";
        
    }
    ASSUME_FALSE(maintenanceMargin.isEmpty());  // ASSERT: maintenanceMargin must not be empty
    
    if (last.isEmpty())
    {
        qDebug() << "Position invalid: last empty";
        
    }
    ASSUME_FALSE(last.isEmpty());  // ASSERT: last must not be empty
    
    if (longShort.isEmpty())
    {
        qDebug() << "Position invalid: longShort empty";
        
    }
    ASSUME_FALSE(longShort.isEmpty());  // ASSERT: longShort must not be empty
    
    if (markToMarketPrice.isEmpty())
    {
        qDebug() << "Position invalid: markToMarketPrice empty";
        
    }
    ASSUME_FALSE(markToMarketPrice.isEmpty());  // ASSERT: markToMarketPrice must not be empty
    
    if (marketValue.isEmpty())
    {
        qDebug() << "Position invalid: marketValue empty";
        
    }
    ASSUME_FALSE(marketValue.isEmpty());  // ASSERT: marketValue must not be empty
    
    if (positionID.isEmpty())
    {
        qDebug() << "Position invalid: positionID empty";
        
    }
    ASSUME_FALSE(positionID.isEmpty());  // ASSERT: positionID must not be empty
    
    if (quantity.isEmpty())
    {
        qDebug() << "Position invalid: quantity empty";
        
    }
    ASSUME_FALSE(quantity.isEmpty());  // ASSERT: quantity must not be empty
    
    if (symbol.isEmpty())
    {
        qDebug() << "Position invalid: symbol empty";
        
    }
    ASSUME_FALSE(symbol.isEmpty());  // ASSERT: symbol must not be empty
    
    if (!timestamp.isValid())
    {
        qDebug() << "Position invalid: timestamp invalid";
        
    }
    ASSUME_TRUE(timestamp.isValid());  // ASSERT: timestamp must be valid
    
    if (todaysProfitLoss.isEmpty())
    {
        qDebug() << "Position invalid: todaysProfitLoss empty";
        
    }
    ASSUME_FALSE(todaysProfitLoss.isEmpty());  // ASSERT: todaysProfitLoss must not be empty
    
    if (totalCost.isEmpty())
    {
        qDebug() << "Position invalid: totalCost empty";
        
    }
    ASSUME_FALSE(totalCost.isEmpty());  // ASSERT: totalCost must not be empty
    
    if (unrealizedProfitLoss.isEmpty())
    {
        qDebug() << "Position invalid: unrealizedProfitLoss empty";
        
    }
    ASSUME_FALSE(unrealizedProfitLoss.isEmpty());  // ASSERT: unrealizedProfitLoss must not be empty
    
    if (unrealizedProfitLossPercent.isEmpty())
    {
        qDebug() << "Position invalid: unrealizedProfitLossPercent empty";
        
    }
    ASSUME_FALSE(unrealizedProfitLossPercent.isEmpty());  // ASSERT: unrealizedProfitLossPercent must not be empty
    
    if (unrealizedProfitLossQty.isEmpty())
    {
        qDebug() << "Position invalid: unrealizedProfitLossQty empty";
        
    }
    ASSUME_FALSE(unrealizedProfitLossQty.isEmpty());  // ASSERT: unrealizedProfitLossQty must not be empty

    // Validate asset type - MUST be one of the recognized types
    bool validAssetType = (assetType == "STOCK" || assetType == "STOCKOPTION" || 
                          assetType == "FUTURE" || assetType == "INDEXOPTION");
    if (!validAssetType)
    {
        qDebug() << "Position invalid: assetType not recognized:" << assetType;
    }
    ASSUME_TRUE(validAssetType);  // ASSERT: assetType must be recognized

    // Validate position direction - MUST be Long or Short (no other values allowed)
    bool validDirection = (longShort == "Long" || longShort == "Short");
    if (!validDirection)
    {
        qDebug() << "Position invalid: longShort not recognized:" << longShort;
    }
    ASSUME_TRUE(validDirection);  // ASSERT: Position direction must be Long or Short

    return true;
}

QString Position::toJsonString() const
{
    QJsonObject jsonObj;
    jsonObj["AccountID"] = accountID;
    jsonObj["AssetType"] = assetType;
    jsonObj["AveragePrice"] = averagePrice;
    jsonObj["Bid"] = bid;
    jsonObj["Ask"] = ask;
    jsonObj["ConversionRate"] = conversionRate;
    if (deleted)
    {
        jsonObj["Deleted"] = deleted;
    }
    jsonObj["DayTradeRequirement"] = dayTradeRequirement;
    if (expirationDate.isValid())
    {
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
    jsonObj["IsUpdate"] = isUpdate; // Include the update flag in JSON output

    QJsonDocument doc(jsonObj);
    return doc.toJson(QJsonDocument::Compact);
}
