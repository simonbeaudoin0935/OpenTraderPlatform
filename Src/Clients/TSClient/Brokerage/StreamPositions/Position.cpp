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
    if (jsonObj.contains("OpenedDateTime"))
    {
        openedDateTime = QDateTime::fromString(jsonObj["OpenedDateTime"].toString(), Qt::ISODate);
    }
    if (jsonObj.contains("ClosedDateTime"))
    {
        closedDateTime = QDateTime::fromString(jsonObj["ClosedDateTime"].toString(), Qt::ISODate);
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
    const auto fail = [](const char* p_message)
    {
        qDebug() << "Position invalid:" << p_message;
        return false;
    };

    if (accountID.isEmpty())
        return fail("accountID empty");
    if (assetType.isEmpty())
        return fail("assetType empty");
    if (averagePrice.isEmpty())
        return fail("averagePrice empty");
    if (bid.isEmpty())
        return fail("bid empty");
    if (ask.isEmpty())
        return fail("ask empty");
    if (conversionRate.isEmpty())
        return fail("conversionRate empty");
    if (dayTradeRequirement.isEmpty())
        return fail("dayTradeRequirement empty");
    if (initialRequirement.isEmpty())
        return fail("initialRequirement empty");
    if (maintenanceMargin.isEmpty())
        return fail("maintenanceMargin empty");
    if (last.isEmpty())
        return fail("last empty");
    if (longShort.isEmpty())
        return fail("longShort empty");
    if (markToMarketPrice.isEmpty())
        return fail("markToMarketPrice empty");
    if (marketValue.isEmpty())
        return fail("marketValue empty");
    if (positionID.isEmpty())
        return fail("positionID empty");
    if (quantity.isEmpty())
        return fail("quantity empty");
    if (symbol.isEmpty())
        return fail("symbol empty");
    if (!timestamp.isValid())
        return fail("timestamp invalid");
    if (todaysProfitLoss.isEmpty())
        return fail("todaysProfitLoss empty");
    if (totalCost.isEmpty())
        return fail("totalCost empty");
    if (unrealizedProfitLoss.isEmpty())
        return fail("unrealizedProfitLoss empty");
    if (unrealizedProfitLossPercent.isEmpty())
        return fail("unrealizedProfitLossPercent empty");
    if (unrealizedProfitLossQty.isEmpty())
        return fail("unrealizedProfitLossQty empty");

    const bool validAssetType =
        (assetType == "STOCK" || assetType == "STOCKOPTION" || assetType == "FUTURE" || assetType == "INDEXOPTION");
    if (!validAssetType)
    {
        qDebug() << "Position invalid: assetType not recognized:" << assetType;
        return false;
    }

    const bool validDirection = (longShort == "Long" || longShort == "Short");
    if (!validDirection)
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
    if (openedDateTime.isValid())
    {
        jsonObj["OpenedDateTime"] = openedDateTime.toString(Qt::ISODate);
    }
    if (closedDateTime.isValid())
    {
        jsonObj["ClosedDateTime"] = closedDateTime.toString(Qt::ISODate);
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
