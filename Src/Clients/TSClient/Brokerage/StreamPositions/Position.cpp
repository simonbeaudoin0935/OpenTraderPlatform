#include <QJsonDocument>

#include "Position.h"

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
    // Check required fields
    if (accountID.isEmpty())
    {
        qDebug() << "Position invalid: accountID empty";
        return false;
    }
    if (assetType.isEmpty())
    {
        qDebug() << "Position invalid: assetType empty";
        return false;
    }
    if (averagePrice.isEmpty())
    {
        qDebug() << "Position invalid: averagePrice empty";
        return false;
    }
    if (bid.isEmpty())
    {
        qDebug() << "Position invalid: bid empty";
        return false;
    }
    if (ask.isEmpty())
    {
        qDebug() << "Position invalid: ask empty";
        return false;
    }
    if (conversionRate.isEmpty())
    {
        qDebug() << "Position invalid: conversionRate empty";
        return false;
    }
    if (dayTradeRequirement.isEmpty())
    {
        qDebug() << "Position invalid: dayTradeRequirement empty";
        return false;
    }
    if (initialRequirement.isEmpty())
    {
        qDebug() << "Position invalid: initialRequirement empty";
        return false;
    }
    if (maintenanceMargin.isEmpty())
    {
        qDebug() << "Position invalid: maintenanceMargin empty";
        return false;
    }
    if (last.isEmpty())
    {
        qDebug() << "Position invalid: last empty";
        return false;
    }
    if (longShort.isEmpty())
    {
        qDebug() << "Position invalid: longShort empty";
        return false;
    }
    if (markToMarketPrice.isEmpty())
    {
        qDebug() << "Position invalid: markToMarketPrice empty";
        return false;
    }
    if (marketValue.isEmpty())
    {
        qDebug() << "Position invalid: marketValue empty";
        return false;
    }
    if (positionID.isEmpty())
    {
        qDebug() << "Position invalid: positionID empty";
        return false;
    }
    if (quantity.isEmpty())
    {
        qDebug() << "Position invalid: quantity empty";
        return false;
    }
    if (symbol.isEmpty())
    {
        qDebug() << "Position invalid: symbol empty";
        return false;
    }
    if (!timestamp.isValid())
    {
        qDebug() << "Position invalid: timestamp invalid";
        return false;
    }
    if (todaysProfitLoss.isEmpty())
    {
        qDebug() << "Position invalid: todaysProfitLoss empty";
        return false;
    }
    if (totalCost.isEmpty())
    {
        qDebug() << "Position invalid: totalCost empty";
        return false;
    }
    if (unrealizedProfitLoss.isEmpty())
    {
        qDebug() << "Position invalid: unrealizedProfitLoss empty";
        return false;
    }
    if (unrealizedProfitLossPercent.isEmpty())
    {
        qDebug() << "Position invalid: unrealizedProfitLossPercent empty";
        return false;
    }
    if (unrealizedProfitLossQty.isEmpty())
    {
        qDebug() << "Position invalid: unrealizedProfitLossQty empty";
        return false;
    }

    // Validate asset type
    if (assetType != "STOCK" && assetType != "STOCKOPTION" && assetType != "FUTURE" && assetType != "INDEXOPTION")
    {
        qDebug() << "Position invalid: assetType not recognized:" << assetType;
        return false;
    }

    // Validate position direction
    if (longShort != "Long" && longShort != "Short")
    {
        qDebug() << "Position invalid: longShort not recognized:" << longShort;
        return false;
    }

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
