#include "PlaceOrder.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QJsonArray>

#include "TSClient.h"
#include "Assume.h"

QString OrderType::toString(OrderType::Type type)
{
    switch (type)
    {
    case Type::Market:
        return "Market";
    case Type::Limit:
        return "Limit";
    case Type::StopMarket:
        return "StopMarket";
    case Type::StopLimit:
        return "StopLimit";
    default:
        ASSUME_FALSE(true); // Should never reach here
    }
}

OrderType OrderType::fromString(const QString& str)
{
    OrderType orderType;

    if (str == "Market")
        orderType.type = Type::Market;
    else if (str == "Limit")
        orderType.type = Type::Limit;
    else if (str == "StopMarket")
        orderType.type = Type::StopMarket;
    else if (str == "StopLimit")
        orderType.type = Type::StopLimit;
    else
        ASSUME_FALSE(true); // Invalid order type string

    return orderType;
}

// TimeInForce implementation
TimeInForce::TimeInForce(OrderDuration p_duration) : duration(p_duration) {}

void TimeInForce::setDuration(OrderDuration value)
{
    duration = value;
}
void TimeInForce::setExpiration(const std::optional<QString>& value)
{
    expiration = value;
}

OrderDuration TimeInForce::getDuration() const
{
    return duration;
}
std::optional<QString> TimeInForce::getExpiration() const
{
    return expiration;
}

QJsonObject TimeInForce::toJson() const
{
    QJsonObject json;

    // Convert duration to string
    QString durationStr;
    switch (duration)
    {
    case OrderDuration::Day:
        durationStr = "DAY";
        break;
    case OrderDuration::DayPlus:
        durationStr = "DYP";
        break;
    case OrderDuration::GTC:
        durationStr = "GTC";
        break;
    case OrderDuration::GTCPlus:
        durationStr = "GCP";
        break;
    case OrderDuration::GTD:
        durationStr = "GTD";
        break;
    case OrderDuration::GTDPlus:
        durationStr = "GDP";
        break;
    case OrderDuration::Opening:
        durationStr = "OPG";
        break;
    case OrderDuration::OnClose:
        durationStr = "CLO";
        break;
    case OrderDuration::IOC:
        durationStr = "IOC";
        break;
    case OrderDuration::FOK:
        durationStr = "FOK";
        break;
    case OrderDuration::OneMinute:
        durationStr = "1";
        break;
    case OrderDuration::ThreeMinutes:
        durationStr = "3";
        break;
    case OrderDuration::FiveMinutes:
        durationStr = "5";
        break;
    }
    json["Duration"] = durationStr;

    if (expiration)
    {
        json["Expiration"] = *expiration;
    }

    return json;
}

bool TimeInForce::isValidExpiration(const QString& expiration)
{
    // Try to parse the string as RFC3339 date
    QDateTime dateTime = QDateTime::fromString(expiration, Qt::ISODate);
    if (!dateTime.isValid())
    {
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
    : orderType(), quantity(0), timeInForce(OrderDuration::Day), tradeAction(TradeAction::Buy)
{
    orderType.type = OrderType::Type::Market;
}

// Setters
void PlaceOrderRequest::setAccountID(const QString& value)
{
    accountID = value;
}
void PlaceOrderRequest::setOrderType(OrderType::Type value)
{
    orderType.type = value;
}
void PlaceOrderRequest::setQuantity(int value)
{
    quantity = value;
}
void PlaceOrderRequest::setSymbol(const QString& value)
{
    symbol = value;
}
void PlaceOrderRequest::setTimeInForce(const TimeInForce& value)
{
    timeInForce = value;
}
void PlaceOrderRequest::setTradeAction(TradeAction value)
{
    tradeAction = value;
}
void PlaceOrderRequest::setLimitPrice(const double& value)
{
    limitPrice = value;
}
void PlaceOrderRequest::setOrderConfirmID(const QString& value)
{
    // Check length is between 1 and 22 characters
    ASSUME_GTE(value.length(), 1);
    ASSUME_LTE(value.length(), 22);

    // Check that all characters are digits
    bool allDigits = true;
    for (const QChar& c: value)
    {
        if (!c.isDigit())
        {
            allDigits = false;
            break;
        }
    }
    ASSUME_TRUE(allDigits);

    orderConfirmID = value;
}
void PlaceOrderRequest::setRoute(const QString& value)
{
    route = value;
}
void PlaceOrderRequest::setStopPrice(const double& value)
{
    stopPrice = value;
}

// Getters
QString PlaceOrderRequest::getAccountID() const
{
    return accountID;
}
OrderType PlaceOrderRequest::getOrderType() const
{
    return orderType;
}
int PlaceOrderRequest::getQuantity() const
{
    return quantity;
}
QString PlaceOrderRequest::getSymbol() const
{
    return symbol;
}
TimeInForce PlaceOrderRequest::getTimeInForce() const
{
    return timeInForce;
}
TradeAction PlaceOrderRequest::getTradeAction() const
{
    return tradeAction;
}
std::optional<double> PlaceOrderRequest::getLimitPrice() const
{
    return limitPrice;
}
std::optional<QString> PlaceOrderRequest::getOrderConfirmID() const
{
    return orderConfirmID;
}
std::optional<QString> PlaceOrderRequest::getRoute() const
{
    return route;
}
std::optional<double> PlaceOrderRequest::getStopPrice() const
{
    return stopPrice;
}

QJsonObject PlaceOrderRequest::toJson() const
{
    QJsonObject json;

    // Required fields
    json["AccountID"] = accountID;
    json["Symbol"] = symbol;


    json["OrderType"] = OrderType::toString(orderType.type);

    // Convert trade action to string
    QString tradeActionStr;
    switch (tradeAction)
    {
    case TradeAction::Buy:
        tradeActionStr = "BUY";
        break;
    case TradeAction::Sell:
        tradeActionStr = "SELL";
        break;
    case TradeAction::BuyToCover:
        tradeActionStr = "BUYTOCOVER";
        break;
    case TradeAction::SellShort:
        tradeActionStr = "SELLSHORT";
        break;
    case TradeAction::BuyToOpen:
        tradeActionStr = "BUYTOOPEN";
        break;
    case TradeAction::BuyToClose:
        tradeActionStr = "BUYTOCLOSE";
        break;
    case TradeAction::SellToOpen:
        tradeActionStr = "SELLTOOPEN";
        break;
    case TradeAction::SellToClose:
        tradeActionStr = "SELLTOCLOSE";
        break;
    }
    json["TradeAction"] = tradeActionStr;

    json["Quantity"] = QString::number(quantity);
    json["TimeInForce"] = timeInForce.toJson();

    // Optional fields
    if (limitPrice)
        json["LimitPrice"] = QString::number(*limitPrice, 'f', 2);
    if (stopPrice)
        json["StopPrice"] = QString::number(*stopPrice, 'f', 2);

    if (route)
        json["Route"] = *route;
    if (orderConfirmID)
        json["OrderConfirmID"] = *orderConfirmID;

    if (advancedOptions)
    {
        json["AdvancedOptions"] = advancedOptions->toJson();
    }

    return json;
}

QString PlaceOrderRequest::toJsonString() const
{
    QJsonDocument doc(toJson());
    return doc.toJson(QJsonDocument::Indented);
}

bool PlaceOrderRequest::isValid() const
{
    // Check required fields
    if (accountID.isEmpty())
    {
        qWarning() << "AccountID is required but not set";
        return false;
    }

    if (symbol.isEmpty())
    {
        qWarning() << "Symbol is required but not set";
        return false;
    }

    if (quantity <= 0)
    {
        qWarning() << "Quantity must be greater than 0";
        return false;
    }

    // Validate order type specific requirements
    switch (orderType.type)
    {
    case OrderType::Type::Market:
        // Nothing to validate for market
        break;

    case OrderType::Type::Limit:
        if (!limitPrice || *limitPrice <= 0)
        {
            qWarning() << "Limit orders require a valid limit price";
            return false;
        }
        break;

    case OrderType::Type::StopMarket:
    case OrderType::Type::StopLimit:
        if (!stopPrice || *stopPrice <= 0)
        {
            qWarning() << "Stop orders require a valid stop price";
            return false;
        }
        if (orderType.type == OrderType::Type::StopLimit && (!limitPrice || *limitPrice <= 0))
        {
            qWarning() << "Stop limit orders require both a valid stop price and limit price";
            return false;
        }
        break;
    }

    // Validate time in force
    if (timeInForce.getDuration() == OrderDuration::GTD)
    {
        if (!timeInForce.getExpiration())
        {
            qWarning() << "GTD orders require an expiration date";
            return false;
        }
        if (!TimeInForce::isValidExpiration(*timeInForce.getExpiration()))
        {
            qWarning() << "GTD expiration date must be in the future and within 90 days";
            return false;
        }
    }

    // Validate advanced options if present
    if (advancedOptions)
    {
        const auto& options = *advancedOptions;

        // Validate market activation rules if present
        if (!options.getMarketActivationRules().isEmpty())
        {
            for (const auto& rule: options.getMarketActivationRules())
            {
                if (rule.getSymbol().isEmpty())
                {
                    qWarning() << "Market activation rules require a symbol";
                    return false;
                }
                if (rule.getPrice().isEmpty())
                {
                    qWarning() << "Market activation rules require a price";
                    return false;
                }
            }
        }

        // Validate time activation rules if present
        if (!options.getTimeActivationRules().isEmpty())
        {
            for (const auto& rule: options.getTimeActivationRules())
            {
                if (rule.getTimeUtc().isEmpty())
                {
                    qWarning() << "Time activation rules require a time";
                    return false;
                }
            }
        }

        // Validate trailing stop if present
        if (options.getTrailingStop())
        {
            const auto& trailingStop = *options.getTrailingStop();
            if (!trailingStop.getAmount() && !trailingStop.getPercent())
            {
                qWarning() << "Trailing stop requires either an amount or percent";
                return false;
            }
        }
    }

    return true;
}

// PlaceOrderResult implementation
PlaceOrderResult::PlaceOrderResult(const QJsonObject& jsonObj)
{
    // Parse Orders array
    if (jsonObj.contains("Orders"))
    {
        QJsonArray ordersArray = jsonObj["Orders"].toArray();
        for (const auto& orderJson: ordersArray)
        {
            OrderResultItem order(orderJson.toObject());

            // If the order has an error field, it's a failed order in the Orders array
            if (order.isError())
            {
                errors.append(order);
            }
            else
            {
                orders.append(order);
            }
        }
    }

    // Parse Errors array (documented way)
    if (jsonObj.contains("Errors"))
    {
        QJsonArray errorsArray = jsonObj["Errors"].toArray();
        for (const auto& errorJson: errorsArray)
        {
            errors.append(OrderResultItem(errorJson.toObject()));
        }
    }
}

QString PlaceOrderResult::toJsonString() const
{
    QJsonObject jsonObj;

    // Build Orders array
    QJsonArray ordersArray;
    for (const auto& order: orders)
    {
        ordersArray.append(QJsonDocument::fromJson(order.toJsonString().toUtf8()).object());
    }
    jsonObj["Orders"] = ordersArray;

    // Build Errors array
    QJsonArray errorsArray;
    for (const auto& error: errors)
    {
        errorsArray.append(QJsonDocument::fromJson(error.toJsonString().toUtf8()).object());
    }
    jsonObj["Errors"] = errorsArray;

    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}

// MarketActivationRule implementation
QJsonObject MarketActivationRule::toJson() const
{
    QJsonObject json;
    json["RuleType"] = ruleType;
    json["Symbol"] = symbol;

    // Convert predicate to string
    QString predicateStr;
    switch (predicate)
    {
    case MarketActivationRulePredicate::LessThan:
        predicateStr = "Lt";
        break;
    case MarketActivationRulePredicate::LessThanOrEqual:
        predicateStr = "Lte";
        break;
    case MarketActivationRulePredicate::GreaterThan:
        predicateStr = "Gt";
        break;
    case MarketActivationRulePredicate::GreaterThanOrEqual:
        predicateStr = "Gte";
        break;
    }
    json["Predicate"] = predicateStr;

    // Convert trigger key to string
    QString triggerKeyStr;
    switch (triggerKey)
    {
    case MarketActivationRuleTriggerKey::STT:
        triggerKeyStr = "STT";
        break;
    case MarketActivationRuleTriggerKey::STTN:
        triggerKeyStr = "STTN";
        break;
    case MarketActivationRuleTriggerKey::SBA:
        triggerKeyStr = "SBA";
        break;
    case MarketActivationRuleTriggerKey::SAB:
        triggerKeyStr = "SAB";
        break;
    case MarketActivationRuleTriggerKey::DTT:
        triggerKeyStr = "DTT";
        break;
    case MarketActivationRuleTriggerKey::DTTN:
        triggerKeyStr = "DTTN";
        break;
    case MarketActivationRuleTriggerKey::DBA:
        triggerKeyStr = "DBA";
        break;
    case MarketActivationRuleTriggerKey::DAB:
        triggerKeyStr = "DAB";
        break;
    case MarketActivationRuleTriggerKey::TTT:
        triggerKeyStr = "TTT";
        break;
    case MarketActivationRuleTriggerKey::TTTN:
        triggerKeyStr = "TTTN";
        break;
    case MarketActivationRuleTriggerKey::TBA:
        triggerKeyStr = "TBA";
        break;
    case MarketActivationRuleTriggerKey::TAB:
        triggerKeyStr = "TAB";
        break;
    }
    json["TriggerKey"] = triggerKeyStr;

    json["Price"] = price;

    // Convert logic operator to string
    QString logicOperatorStr;
    switch (logicOperator)
    {
    case MarketActivationRuleLogicOperator::And:
        logicOperatorStr = "And";
        break;
    case MarketActivationRuleLogicOperator::Or:
        logicOperatorStr = "Or";
        break;
    }
    json["LogicOperator"] = logicOperatorStr;

    return json;
}

// TimeActivationRule implementation
QJsonObject TimeActivationRule::toJson() const
{
    QJsonObject json;
    json["TimeUtc"] = timeUtc;
    return json;
}

// TrailingStop implementation
QJsonObject TrailingStop::toJson() const
{
    QJsonObject json;
    if (amount)
        json["Amount"] = *amount;
    if (percent)
        json["Percent"] = *percent;
    return json;
}

// AdvancedOptions implementation
QJsonObject AdvancedOptionsRequest::toJson() const
{
    QJsonObject json;

    if (addLiquidity)
        json["AddLiquidity"] = *addLiquidity;
    if (allOrNone)
        json["AllOrNone"] = *allOrNone;
    if (bookOnly)
        json["BookOnly"] = *bookOnly;
    if (discretionaryPrice)
        json["DiscretionaryPrice"] = *discretionaryPrice;

    if (!marketActivationRules.empty())
    {
        QJsonArray rulesArray;
        for (const auto& rule: marketActivationRules)
        {
            rulesArray.append(rule.toJson());
        }
        json["MarketActivationRules"] = rulesArray;
    }

    if (nonDisplay)
        json["NonDisplay"] = *nonDisplay;

    if (pegValue)
    {
        QString pegValueStr;
        switch (*pegValue)
        {
        case PegValue::Best:
            pegValueStr = "BEST";
            break;
        case PegValue::Mid:
            pegValueStr = "MID";
            break;
        }
        json["PegValue"] = pegValueStr;
    }

    if (showOnlyQuantity)
        json["ShowOnlyQuantity"] = *showOnlyQuantity;

    if (!timeActivationRules.empty())
    {
        QJsonArray rulesArray;
        for (const auto& rule: timeActivationRules)
        {
            rulesArray.append(rule.toJson());
        }
        json["TimeActivationRules"] = rulesArray;
    }

    if (trailingStop)
        json["TrailingStop"] = trailingStop->toJson();

    return json;
}

OrderResultItem::OrderResultItem(const QJsonObject& jsonObj)
{
    orderID = jsonObj["OrderID"].toString();
    message = jsonObj["Message"].toString();

    // Error field is optional
    if (jsonObj.contains("Error"))
    {
        error = jsonObj["Error"].toString();
    }
}

QString OrderResultItem::toJsonString() const
{
    QJsonObject jsonObj;
    jsonObj["OrderID"] = orderID;
    jsonObj["Message"] = message;
    if (error.has_value())
    {
        jsonObj["Error"] = error.value();
    }

    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}