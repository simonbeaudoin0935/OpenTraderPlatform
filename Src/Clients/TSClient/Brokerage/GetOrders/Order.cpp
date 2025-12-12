#include "Order.h"

AdvancedOptions::AdvancedOptions(const QString &str)
{
    if (str == "CND") {
        type = Type::CND;
    } else if (str == "AON") {
        type = Type::AON;
    } else if (str == "TRL") {
        type = Type::TRL;
    } else if (str.startsWith("SHWQTY=")) {
        type = Type::SHWQTY;
        shwqty = 0;
    } else if (str.startsWith("DSCPR=")) {
        type = Type::DSCPR;
        dscpr = 0;
    } else if (str == "NON") {
        type = Type::NON;
    } else if (str.startsWith("PEGVAL=")) {
        type = Type::PEGVAL;
        pegval = 0;
    } else if (str == "BKO") {
        type = Type::BKO;
    } else if (str == "PSO") {
        type = Type::PSO;
    }
}

Order::Order(const QJsonObject &jsonObj, bool isUpdate_) :
    isUpdate(isUpdate_)
{
    accountID = jsonObj["AccountID"].toString();

    if (jsonObj.contains("AdvancedOptions")) {
        //TODO
    }

    // Parse timestamps
    if (jsonObj.contains("ClosedDateTime")) {
        closedDateTime = QDateTime::fromString(jsonObj["ClosedDateTime"].toString(), Qt::ISODate);
    }
    if (jsonObj.contains("OpenedDateTime")) {
        openedDateTime = QDateTime::fromString(jsonObj["OpenedDateTime"].toString(), Qt::ISODate);
    }
    
    // Parse numeric fields
    comissionsFee = jsonObj["CommissionFee"].toDouble(0.0);
    conversionRate = jsonObj["ConversionRate"].toDouble(1.0);
    filledPrice = jsonObj["FilledPrice"].toDouble(0.0);
    limitPrice = jsonObj["LimitPrice"].toDouble(0.0);
    priceUsedForBuyingPower = jsonObj["PriceUsedForBuyingPower"].toDouble(0.0);
    showOnlyQuantity = jsonObj["ShowOnlyQuantity"].toDouble(0.0);
    spread = jsonObj["Spread"].toDouble(0.0);
    stopPrice = jsonObj["StopPrice"].toDouble(0.0);
    unbundledRouteFee = jsonObj["UnbundledRouteFee"].toDouble(0.0);
    
    // Parse string fields
    currency = jsonObj["Currency"].toString();
    duration = jsonObj["Duration"].toString();
    orderID = jsonObj["OrderID"].toString();
    routing = jsonObj["Routing"].toString();
    statusDescription = jsonObj["StatusDescription"].toString();
    
    // Parse additional display fields
    symbol = jsonObj["Symbol"].toString();
    quantity = jsonObj["Quantity"].toString();
    tradeAction = jsonObj["TradeAction"].toString();
    
    // Parse optional fields
    if (jsonObj.contains("GoodTillDate")) {
        goodTillDate = QDateTime::fromString(jsonObj["GoodTillDate"].toString(), Qt::ISODate);
    }
    if (jsonObj.contains("GroupName")) {
        groupName = jsonObj["GroupName"].toString();
    }
    if (jsonObj.contains("RejectReason")) {
        rejectReason = jsonObj["RejectReason"].toString();
    }
    
    // Parse OrderType
    if (jsonObj.contains("OrderType")) {
        QString orderTypeStr = jsonObj["OrderType"].toString();
        if (orderTypeStr == "Market") {
            orderType.type = OrderType::Type::Market;
        } else if (orderTypeStr == "Limit") {
            orderType.type = OrderType::Type::Limit;
        } else if (orderTypeStr == "StopMarket") {
            orderType.type = OrderType::Type::StopMarket;
        } else if (orderTypeStr == "StopLimit") {
            orderType.type = OrderType::Type::StopLimit;
        }
    }
}

bool Order::isValid()
{
    // Check required fields
    if (accountID.isEmpty() || orderID.isEmpty()) {
        return false;
    }
    
    return true;
}
