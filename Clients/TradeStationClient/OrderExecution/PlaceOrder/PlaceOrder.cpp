#include "PlaceOrder.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QJsonArray>

// TimeInForce implementation
TimeInForce::TimeInForce(OrderDuration duration)
    : duration(duration)
{
}

void TimeInForce::setDuration(OrderDuration value) { duration = value; }
void TimeInForce::setExpiration(const std::optional<QString>& value) { expiration = value; }

OrderDuration TimeInForce::getDuration() const { return duration; }
std::optional<QString> TimeInForce::getExpiration() const { return expiration; }

QJsonObject TimeInForce::toJson() const {
    QJsonObject json;
    
    // Convert duration to string
    QString durationStr;
    switch (duration) {
        case OrderDuration::Day: durationStr = "DAY"; break;
        case OrderDuration::DayPlus: durationStr = "DYP"; break;
        case OrderDuration::GTC: durationStr = "GTC"; break;
        case OrderDuration::GTCPlus: durationStr = "GCP"; break;
        case OrderDuration::GTD: durationStr = "GTD"; break;
        case OrderDuration::GTDPlus: durationStr = "GDP"; break;
        case OrderDuration::Opening: durationStr = "OPG"; break;
        case OrderDuration::OnClose: durationStr = "CLO"; break;
        case OrderDuration::IOC: durationStr = "IOC"; break;
        case OrderDuration::FOK: durationStr = "FOK"; break;
        case OrderDuration::OneMinute: durationStr = "1"; break;
        case OrderDuration::ThreeMinutes: durationStr = "3"; break;
        case OrderDuration::FiveMinutes: durationStr = "5"; break;
    }
    json["Duration"] = durationStr;
    
    if (expiration) {
        json["Expiration"] = *expiration;
    }
    
    return json;
}

bool TimeInForce::isValidExpiration(const QString& expiration) {
    // Try to parse the string as RFC3339 date
    QDateTime dateTime = QDateTime::fromString(expiration, Qt::ISODate);
    if (!dateTime.isValid()) {
        return false;
    }

    // Get current date
    QDateTime currentDate = QDateTime::currentDateTimeUtc();
    
    // Calculate the difference in days
    qint64 daysDifference = currentDate.daysTo(dateTime);
    
    // Check if the date is in the future and within 90 days
    return daysDifference > 0 && daysDifference <= 90;
}

// PlaceOrderRequest implementation
PlaceOrderRequest::PlaceOrderRequest()
    : orderType(OrderType::Market)
    , tradeAction(TradeAction::Buy)
    , quantity(0)
    , timeInForce(OrderDuration::Day)
{
}

// Setters
void PlaceOrderRequest::setAccountID(const QString& value) { accountID = value; }
void PlaceOrderRequest::setOrderType(OrderType value) { orderType = value; }
void PlaceOrderRequest::setQuantity(int value) { quantity = value; }
void PlaceOrderRequest::setSymbol(const QString& value) { symbol = value; }
void PlaceOrderRequest::setTimeInForce(const TimeInForce& value) { timeInForce = value; }
void PlaceOrderRequest::setTradeAction(TradeAction value) { tradeAction = value; }
void PlaceOrderRequest::setLimitPrice(const std::optional<double>& value) { limitPrice = value; }
void PlaceOrderRequest::setOrderConfirmID(const std::optional<QString>& value) { 
    if (value) {
        // Check length is between 1 and 22 characters
        Q_ASSERT(value->length() >= 1 && value->length() <= 22);
        
        // Check that all characters are digits
        bool allDigits = true;
        for (const QChar& c : *value) {
            if (!c.isDigit()) {
                allDigits = false;
                break;
            }
        }
        Q_ASSERT(allDigits);
    }
    orderConfirmID = value; 
}
void PlaceOrderRequest::setRoute(const std::optional<QString>& value) { route = value; }
void PlaceOrderRequest::setStopPrice(const std::optional<double>& value) { stopPrice = value; }
void PlaceOrderRequest::setOcaGroupName(const std::optional<QString>& value) { ocaGroupName = value; }
void PlaceOrderRequest::setOcaGroupType(const std::optional<QString>& value) { ocaGroupType = value; }

// Getters
QString PlaceOrderRequest::getAccountID() const { return accountID; }
OrderType PlaceOrderRequest::getOrderType() const { return orderType; }
int PlaceOrderRequest::getQuantity() const { return quantity; }
QString PlaceOrderRequest::getSymbol() const { return symbol; }
TimeInForce PlaceOrderRequest::getTimeInForce() const { return timeInForce; }
TradeAction PlaceOrderRequest::getTradeAction() const { return tradeAction; }
std::optional<double> PlaceOrderRequest::getLimitPrice() const { return limitPrice; }
std::optional<QString> PlaceOrderRequest::getOrderConfirmID() const { return orderConfirmID; }
std::optional<QString> PlaceOrderRequest::getRoute() const { return route; }
std::optional<double> PlaceOrderRequest::getStopPrice() const { return stopPrice; }
std::optional<QString> PlaceOrderRequest::getOcaGroupName() const { return ocaGroupName; }
std::optional<QString> PlaceOrderRequest::getOcaGroupType() const { return ocaGroupType; }

QJsonObject PlaceOrderRequest::toJson() const {
    QJsonObject json;
    
    // Required fields
    json["AccountID"] = accountID;
    json["Symbol"] = symbol;
    
    // Convert enums to strings
    QString orderTypeStr;
    switch (orderType) {
        case OrderType::Market: orderTypeStr = "Market"; break;
        case OrderType::Limit: orderTypeStr = "Limit"; break;
        case OrderType::StopMarket: orderTypeStr = "StopMarket"; break;
        case OrderType::StopLimit: orderTypeStr = "StopLimit"; break;
    }
    json["OrderType"] = orderTypeStr;
    
    // Convert trade action to string
    QString tradeActionStr;
    switch (tradeAction) {
        case TradeAction::Buy: tradeActionStr = "BUY"; break;
        case TradeAction::Sell: tradeActionStr = "SELL"; break;
        case TradeAction::BuyToCover: tradeActionStr = "BUYTOCOVER"; break;
        case TradeAction::SellShort: tradeActionStr = "SELLSHORT"; break;
        case TradeAction::BuyToOpen: tradeActionStr = "BUYTOOPEN"; break;
        case TradeAction::BuyToClose: tradeActionStr = "BUYTOCLOSE"; break;
        case TradeAction::SellToOpen: tradeActionStr = "SELLTOOPEN"; break;
        case TradeAction::SellToClose: tradeActionStr = "SELLTOCLOSE"; break;
    }
    json["TradeAction"] = tradeActionStr;
    
    json["Quantity"] = quantity;
    json["TimeInForce"] = timeInForce.toJson();
    
    // Optional fields
    if (limitPrice) json["LimitPrice"] = *limitPrice;
    if (stopPrice) json["StopPrice"] = *stopPrice;
    
    if (ocaGroupName) json["OCAGroupName"] = *ocaGroupName;
    if (ocaGroupType) json["OCAGroupType"] = *ocaGroupType;
    if (route) json["Route"] = *route;
    if (orderConfirmID) json["OrderConfirmID"] = *orderConfirmID;
    
    if (advancedOptions) {
        json["AdvancedOptions"] = advancedOptions->toJson();
    }
    
    return json;
}

// PlaceOrderResult implementation
PlaceOrderResult::PlaceOrderResult(const QJsonObject& jsonObj) {
    orderID = jsonObj["OrderID"].toString();
    status = jsonObj["Status"].toString();
    message = jsonObj["Message"].toString();
    error = jsonObj["Error"].toString();
    detailedMessage = jsonObj["DetailedMessage"].toString();
    orderDateTime = jsonObj["OrderDateTime"].toString();
    orderStatus = jsonObj["OrderStatus"].toString();
    primaryOrderID = jsonObj["PrimaryOrderID"].toString();
    secondaryOrderID = jsonObj["SecondaryOrderID"].toString();
    orderType = jsonObj["OrderType"].toString();
    symbol = jsonObj["Symbol"].toString();
    quantity = jsonObj["Quantity"].toInt();
    limitPrice = jsonObj["LimitPrice"].toDouble();
    stopPrice = jsonObj["StopPrice"].toDouble();
    duration = jsonObj["Duration"].toString();
    allOrNone = jsonObj["AllOrNone"].toBool();
    gtdDate = jsonObj["GTDDate"].toString();
}

// Getters
QString PlaceOrderResult::getOrderID() const { return orderID; }
QString PlaceOrderResult::getStatus() const { return status; }
QString PlaceOrderResult::getMessage() const { return message; }
QString PlaceOrderResult::getError() const { return error; }
QString PlaceOrderResult::getDetailedMessage() const { return detailedMessage; }
QString PlaceOrderResult::getOrderDateTime() const { return orderDateTime; }
QString PlaceOrderResult::getOrderStatus() const { return orderStatus; }
QString PlaceOrderResult::getPrimaryOrderID() const { return primaryOrderID; }
QString PlaceOrderResult::getSecondaryOrderID() const { return secondaryOrderID; }
QString PlaceOrderResult::getOrderType() const { return orderType; }
QString PlaceOrderResult::getSymbol() const { return symbol; }
int PlaceOrderResult::getQuantity() const { return quantity; }
double PlaceOrderResult::getLimitPrice() const { return limitPrice; }
double PlaceOrderResult::getStopPrice() const { return stopPrice; }
QString PlaceOrderResult::getDuration() const { return duration; }
bool PlaceOrderResult::getAllOrNone() const { return allOrNone; }
QString PlaceOrderResult::getGtdDate() const { return gtdDate; }

QString PlaceOrderResult::toJsonString() const {
    QJsonObject json;
    json["OrderID"] = orderID;
    json["Status"] = status;
    json["Message"] = message;
    json["Error"] = error;
    json["DetailedMessage"] = detailedMessage;
    json["OrderDateTime"] = orderDateTime;
    json["OrderStatus"] = orderStatus;
    json["PrimaryOrderID"] = primaryOrderID;
    json["SecondaryOrderID"] = secondaryOrderID;
    json["OrderType"] = orderType;
    json["Symbol"] = symbol;
    json["Quantity"] = quantity;
    json["LimitPrice"] = limitPrice;
    json["StopPrice"] = stopPrice;
    json["Duration"] = duration;
    json["AllOrNone"] = allOrNone;
    json["GTDDate"] = gtdDate;
    
    QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Compact);
}

// MarketActivationRule implementation
QJsonObject MarketActivationRule::toJson() const {
    QJsonObject json;
    json["RuleType"] = ruleType;
    json["Symbol"] = symbol;
    
    // Convert predicate to string
    QString predicateStr;
    switch (predicate) {
        case MarketActivationRulePredicate::LessThan: predicateStr = "Lt"; break;
        case MarketActivationRulePredicate::LessThanOrEqual: predicateStr = "Lte"; break;
        case MarketActivationRulePredicate::GreaterThan: predicateStr = "Gt"; break;
        case MarketActivationRulePredicate::GreaterThanOrEqual: predicateStr = "Gte"; break;
    }
    json["Predicate"] = predicateStr;
    
    // Convert trigger key to string
    QString triggerKeyStr;
    switch (triggerKey) {
        case MarketActivationRuleTriggerKey::STT: triggerKeyStr = "STT"; break;
        case MarketActivationRuleTriggerKey::STTN: triggerKeyStr = "STTN"; break;
        case MarketActivationRuleTriggerKey::SBA: triggerKeyStr = "SBA"; break;
        case MarketActivationRuleTriggerKey::SAB: triggerKeyStr = "SAB"; break;
        case MarketActivationRuleTriggerKey::DTT: triggerKeyStr = "DTT"; break;
        case MarketActivationRuleTriggerKey::DTTN: triggerKeyStr = "DTTN"; break;
        case MarketActivationRuleTriggerKey::DBA: triggerKeyStr = "DBA"; break;
        case MarketActivationRuleTriggerKey::DAB: triggerKeyStr = "DAB"; break;
        case MarketActivationRuleTriggerKey::TTT: triggerKeyStr = "TTT"; break;
        case MarketActivationRuleTriggerKey::TTTN: triggerKeyStr = "TTTN"; break;
        case MarketActivationRuleTriggerKey::TBA: triggerKeyStr = "TBA"; break;
        case MarketActivationRuleTriggerKey::TAB: triggerKeyStr = "TAB"; break;
    }
    json["TriggerKey"] = triggerKeyStr;
    
    json["Price"] = price;
    
    // Convert logic operator to string
    QString logicOperatorStr;
    switch (logicOperator) {
        case MarketActivationRuleLogicOperator::And: logicOperatorStr = "And"; break;
        case MarketActivationRuleLogicOperator::Or: logicOperatorStr = "Or"; break;
    }
    json["LogicOperator"] = logicOperatorStr;
    
    return json;
}

// TimeActivationRule implementation
QJsonObject TimeActivationRule::toJson() const {
    QJsonObject json;
    json["TimeUtc"] = timeUtc;
    return json;
}

// TrailingStop implementation
QJsonObject TrailingStop::toJson() const {
    QJsonObject json;
    if (amount) json["Amount"] = *amount;
    if (percent) json["Percent"] = *percent;
    return json;
}

// AdvancedOptions implementation
QJsonObject AdvancedOptions::toJson() const {
    QJsonObject json;
    
    if (addLiquidity) json["AddLiquidity"] = *addLiquidity;
    if (allOrNone) json["AllOrNone"] = *allOrNone;
    if (bookOnly) json["BookOnly"] = *bookOnly;
    if (discretionaryPrice) json["DiscretionaryPrice"] = *discretionaryPrice;
    
    if (!marketActivationRules.empty()) {
        QJsonArray rulesArray;
        for (const auto& rule : marketActivationRules) {
            rulesArray.append(rule.toJson());
        }
        json["MarketActivationRules"] = rulesArray;
    }
    
    if (nonDisplay) json["NonDisplay"] = *nonDisplay;
    
    if (pegValue) {
        QString pegValueStr;
        switch (*pegValue) {
            case PegValue::Best: pegValueStr = "BEST"; break;
            case PegValue::Mid: pegValueStr = "MID"; break;
        }
        json["PegValue"] = pegValueStr;
    }
    
    if (showOnlyQuantity) json["ShowOnlyQuantity"] = *showOnlyQuantity;
    
    if (!timeActivationRules.empty()) {
        QJsonArray rulesArray;
        for (const auto& rule : timeActivationRules) {
            rulesArray.append(rule.toJson());
        }
        json["TimeActivationRules"] = rulesArray;
    }
    
    if (trailingStop) json["TrailingStop"] = trailingStop->toJson();
    
    return json;
}
