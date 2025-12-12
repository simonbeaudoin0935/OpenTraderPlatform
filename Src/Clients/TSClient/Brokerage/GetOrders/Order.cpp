#include <QJsonDocument>
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
    // Debug: Log all keys in the JSON object
    qDebug() << "Order constructor: JSON keys:" << jsonObj.keys();
    qDebug() << "Order constructor: Full JSON:" << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
    
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
    commissionsFee = jsonObj["CommissionFee"].toDouble(0.0);
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
    
    // Parse additional display fields - try multiple possible field names and nested structures
    QJsonObject orderObj = jsonObj;
    
    // Check if the order data is nested under an "Order" key
    if (jsonObj.contains("Order") && jsonObj["Order"].isObject()) {
        orderObj = jsonObj["Order"].toObject();
        qDebug() << "Found nested Order object";
    }
    
    symbol = orderObj["Symbol"].toString();
    if (symbol.isEmpty()) {
        symbol = orderObj["symbol"].toString();
    }
    if (symbol.isEmpty()) {
        symbol = orderObj["Instrument"].toString();
    }
    
    quantity = orderObj["Quantity"].toString();
    if (quantity.isEmpty()) {
        quantity = orderObj["Qty"].toString();
        if (quantity.isEmpty()) {
            quantity = orderObj["quantity"].toString();
            if (quantity.isEmpty()) {
                quantity = orderObj["qty"].toString();
            }
        }
    }
    
    tradeAction = orderObj["TradeAction"].toString();
    if (tradeAction.isEmpty()) {
        tradeAction = orderObj["Side"].toString();
        if (tradeAction.isEmpty()) {
            tradeAction = orderObj["Action"].toString();
            if (tradeAction.isEmpty()) {
                tradeAction = orderObj["tradeAction"].toString();
                if (tradeAction.isEmpty()) {
                    tradeAction = orderObj["side"].toString();
                    if (tradeAction.isEmpty()) {
                        tradeAction = orderObj["action"].toString();
                    }
                }
            }
        }
    }
    
    // Debug: Log parsed values
    qDebug() << "Order parsed values - OrderID:" << orderID 
             << "Symbol:" << symbol 
             << "Quantity:" << quantity 
             << "TradeAction:" << tradeAction;
    
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
        } else {
            // Default to Market for unknown types
            qWarning() << "Unknown OrderType:" << orderTypeStr << "- defaulting to Market";
            orderType.type = OrderType::Type::Market;
        }
    } else {
        // Default to Market if not specified
        orderType.type = OrderType::Type::Market;
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
