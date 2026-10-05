#include <QJsonDocument>
#include <QJsonArray>

#include "Order.h"
#include "Logging.h"
#include "CONSTANTS.h"
#include "Assume.h"

OrderNS::AdvancedOptions::AdvancedOptions(const QString& str)
{
    if (str == "CND")
    {
        type = Type::CND;
    }
    else if (str == "AON")
    {
        type = Type::AON;
    }
    else if (str == "TRL")
    {
        type = Type::TRL;
    }
    else if (str.startsWith("SHWQTY="))
    {
        type = Type::SHWQTY;
        shwqty = 0;
    }
    else if (str.startsWith("DSCPR="))
    {
        type = Type::DSCPR;
        dscpr = 0;
    }
    else if (str == "NON")
    {
        type = Type::NON;
    }
    else if (str.startsWith("PEGVAL="))
    {
        type = Type::PEGVAL;
        pegval = 0;
    }
    else if (str == "BKO")
    {
        type = Type::BKO;
    }
    else if (str == "PSO")
    {
        type = Type::PSO;
    }
}

Order::Order(const QJsonObject& jsonObj, bool isUpdate_) : m_isUpdate(isUpdate_)
{
    m_accountID = jsonObj["AccountID"].toString();

    if (jsonObj.contains("AdvancedOptions"))
    {
        // TODO parse advanced options properly
        qCritical() << "AdvancedOptions parsing not implemented yet";
    }

    if (jsonObj.contains("ClosedDateTime"))
    {
        m_closedDateTime = QDateTime::fromString(jsonObj["ClosedDateTime"].toString(), Qt::ISODate)
                               .toTimeZone(TradingHours::MARKET_TIMEZONE);
    }
    if (jsonObj.contains("OpenedDateTime"))
    {
        m_openedDateTime = QDateTime::fromString(jsonObj["OpenedDateTime"].toString(), Qt::ISODate)
                               .toTimeZone(TradingHours::MARKET_TIMEZONE);
    }

    // Parse numeric fields
    m_commissionsFee = jsonObj["CommissionFee"].toDouble(0.0);
    m_conversionRate = jsonObj["ConversionRate"].toDouble(1.0);

    // FilledPrice may come as string or number
    QJsonValue filledPriceVal = jsonObj["FilledPrice"];
    if (filledPriceVal.isString())
    {
        m_filledPrice = filledPriceVal.toString().toDouble();
    }
    else
    {
        m_filledPrice = filledPriceVal.toDouble(0.0);
    }

    // Parse optional limit price
    if (jsonObj.contains("LimitPrice"))
    {
        const QJsonValue limitPriceVal = jsonObj["LimitPrice"];
        const double limitPriceValue =
            limitPriceVal.isString() ? limitPriceVal.toString().toDouble() : limitPriceVal.toDouble();
        m_limitPrice = limitPriceValue;
    }

    m_priceUsedForBuyingPower = jsonObj["PriceUsedForBuyingPower"].toDouble(0.0);
    m_showOnlyQuantity = jsonObj["ShowOnlyQuantity"].toDouble(0.0);
    m_spread = jsonObj["Spread"].toDouble(0.0);

    // Parse optional stop price
    if (jsonObj.contains("StopPrice"))
    {
        const QJsonValue stopPriceVal = jsonObj["StopPrice"];
        m_stopPrice = stopPriceVal.isString() ? stopPriceVal.toString().toDouble() : stopPriceVal.toDouble();
    }

    m_unbundledRouteFee = jsonObj["UnbundledRouteFee"].toDouble(0.0);

    // Parse string fields
    m_currency = jsonObj["Currency"].toString();
    m_duration = jsonObj["Duration"].toString();
    m_orderID = jsonObj["OrderID"].toString();
    m_routing = jsonObj["Routing"].toString();
    m_statusDescription = jsonObj["StatusDescription"].toString();

    // Parse status code and convert to enum
    QString statusCode = jsonObj["Status"].toString();

    m_orderStatus = QtEnum::fromString<Order::Status>(statusCode);
    if (m_statusDescription.isEmpty())
    {
        m_statusDescription = getStatusDescriptionForStatus(m_orderStatus);
    }

    // Parse display fields from Legs array
    if (jsonObj.contains("Legs") && jsonObj["Legs"].isArray())
    {
        QJsonArray legsArray = jsonObj["Legs"].toArray();
        if (!legsArray.isEmpty())
        {
            // For now, we take the first leg (most orders have only one leg)
            QJsonObject firstLeg = legsArray[0].toObject();

            m_symbol = firstLeg["Symbol"].toString();
            m_quantity = firstLeg["QuantityOrdered"].toString();

            // Construct trade action from BuyOrSell and OpenOrClose (TS returns multiple variants).
            const QString buyOrSell = firstLeg["BuyOrSell"].toString();
            const QString openOrClose = firstLeg["OpenOrClose"].toString();
            QString normalizedBuyOrSell = buyOrSell.trimmed().toUpper();
            normalizedBuyOrSell.remove(' ');
            QString normalizedOpenOrClose = openOrClose.trimmed().toUpper();

            if ((normalizedBuyOrSell == "BUY" || normalizedBuyOrSell == "BUYTOOPEN") && normalizedOpenOrClose == "OPEN")
            {
                m_tradeAction = "Buy";
            }
            else if ((normalizedBuyOrSell == "BUY" || normalizedBuyOrSell == "BUYTOCOVER" ||
                      normalizedBuyOrSell == "BUYTOCLOSE") &&
                     normalizedOpenOrClose == "CLOSE")
            {
                m_tradeAction = "Buy to Cover";
            }
            else if ((normalizedBuyOrSell == "SELL" || normalizedBuyOrSell == "SELLSHORT" ||
                      normalizedBuyOrSell == "SELLTOOPEN") &&
                     normalizedOpenOrClose == "OPEN")
            {
                m_tradeAction = "Sell Short";
            }
            else if ((normalizedBuyOrSell == "SELL" || normalizedBuyOrSell == "SELLTOCLOSE") &&
                     normalizedOpenOrClose == "CLOSE")
            {
                m_tradeAction = "Sell";
            }
            else if (normalizedOpenOrClose.isEmpty())
            {
                // No OpenOrClose field — normalize API strings to display form.
                if (normalizedBuyOrSell == "BUYTOCOVER" || normalizedBuyOrSell == "BUYTOCLOSE")
                {
                    m_tradeAction = "Buy to Cover";
                }
                else if (normalizedBuyOrSell == "SELLSHORT" || normalizedBuyOrSell == "SELLTOOPEN")
                {
                    m_tradeAction = "Sell Short";
                }
                else if (normalizedBuyOrSell == "SELLTOCLOSE")
                {
                    m_tradeAction = "Sell";
                }
                else if (normalizedBuyOrSell == "BUYTOOPEN")
                {
                    m_tradeAction = "Buy";
                }
                else
                {
                    m_tradeAction = buyOrSell; // "Buy" or "Sell" pass through as-is
                }
            }
            else
            {
                // Unknown combination - this should not happen with valid TradeStation API data
                qWarning() << "Order: Unknown BuyOrSell/OpenOrClose combination:"
                           << "BuyOrSell=" << buyOrSell << "OpenOrClose=" << openOrClose;
                m_tradeAction = buyOrSell + " " + openOrClose;
                // ASSERT: This combination should be recognized
                ASSUME_TRUE(false); // Force crash to expose unknown combinations
            }
        }
    }
    else
    {
        // Fallback to old parsing logic if no Legs array
        QJsonObject orderObj = jsonObj;

        // Check if the order data is nested under an "Order" key
        if (jsonObj.contains("Order") && jsonObj["Order"].isObject())
        {
            orderObj = jsonObj["Order"].toObject();
            qDebug() << "Found nested Order object";
        }

        m_symbol = orderObj["Symbol"].toString();
        if (m_symbol.isEmpty())
        {
            m_symbol = orderObj["symbol"].toString();
        }
        if (m_symbol.isEmpty())
        {
            m_symbol = orderObj["Instrument"].toString();
        }

        m_quantity = orderObj["Quantity"].toString();
        if (m_quantity.isEmpty())
        {
            m_quantity = orderObj["Qty"].toString();
            if (m_quantity.isEmpty())
            {
                m_quantity = orderObj["quantity"].toString();
                if (m_quantity.isEmpty())
                {
                    m_quantity = orderObj["qty"].toString();
                }
            }
        }

        m_tradeAction = orderObj["TradeAction"].toString();
        if (m_tradeAction.isEmpty())
        {
            m_tradeAction = orderObj["Side"].toString();
            if (m_tradeAction.isEmpty())
            {
                m_tradeAction = orderObj["Action"].toString();
                if (m_tradeAction.isEmpty())
                {
                    m_tradeAction = orderObj["tradeAction"].toString();
                    if (m_tradeAction.isEmpty())
                    {
                        m_tradeAction = orderObj["side"].toString();
                        if (m_tradeAction.isEmpty())
                        {
                            m_tradeAction = orderObj["action"].toString();
                        }
                    }
                }
            }
        }
    }

    // Debug: Log parsed values
    qDebug() << "Order parsed values - OrderID:" << m_orderID << "Symbol:" << m_symbol << "Quantity:" << m_quantity
             << "TradeAction:" << m_tradeAction;

    // Parse optional fields
    if (jsonObj.contains("GoodTillDate"))
    {
        m_goodTillDate = QDateTime::fromString(jsonObj["GoodTillDate"].toString(), Qt::ISODate);
    }
    if (jsonObj.contains("GroupName"))
    {
        m_groupName = jsonObj["GroupName"].toString();
    }
    if (jsonObj.contains("RejectReason"))
    {
        m_rejectReason = jsonObj["RejectReason"].toString();
    }

    // Parse OrderType
    if (jsonObj.contains("OrderType"))
    {
        QString orderTypeStr = jsonObj["OrderType"].toString();
        if (orderTypeStr == "Market")
        {
            m_orderType.type = OrderType::Type::Market;
        }
        else if (orderTypeStr == "Limit")
        {
            m_orderType.type = OrderType::Type::Limit;
        }
        else if (orderTypeStr == "StopMarket")
        {
            m_orderType.type = OrderType::Type::StopMarket;
        }
        else if (orderTypeStr == "StopLimit")
        {
            m_orderType.type = OrderType::Type::StopLimit;
        }
        else
        {
            // Default to Market for unknown types
            qWarning() << "Unknown OrderType:" << orderTypeStr << "- defaulting to Market";
            m_orderType.type = OrderType::Type::Market;
        }
    }
    else
    {
        // Default to Market if not specified
        m_orderType.type = OrderType::Type::Market;
    }
}

bool Order::isValid()
{
    // Check required fields - these should NEVER be empty
    if (m_accountID.isEmpty())
    {
        qDebug() << "Order invalid: accountID empty";
    }
    ASSUME_FALSE(m_accountID.isEmpty()); // ASSERT: accountID must not be empty

    if (m_orderID.isEmpty())
    {
        qDebug() << "Order invalid: orderID empty";
    }
    ASSUME_FALSE(m_orderID.isEmpty()); // ASSERT: orderID must not be empty

    if (m_symbol.isEmpty())
    {
        qDebug() << "Order invalid: symbol empty";
    }
    ASSUME_FALSE(m_symbol.isEmpty()); // ASSERT: symbol must not be empty

    if (m_quantity.isEmpty())
    {
        qDebug() << "Order invalid: quantity empty";
    }
    ASSUME_FALSE(m_quantity.isEmpty()); // ASSERT: quantity must not be empty

    if (m_tradeAction.isEmpty())
    {
        qDebug() << "Order invalid: tradeAction empty";
    }
    ASSUME_FALSE(m_tradeAction.isEmpty()); // ASSERT: tradeAction must not be empty

    // Validate tradeAction - MUST be one of the recognized actions
    bool validTradeAction = (m_tradeAction == "BUY" || m_tradeAction == "SELL" || m_tradeAction == "BUYTOCOVER" ||
                             m_tradeAction == "SELLSHORT" || m_tradeAction == "Buy" || m_tradeAction == "Sell" ||
                             m_tradeAction == "BuyToCover" || m_tradeAction == "SellShort" ||
                             m_tradeAction == "Buy to Cover" || m_tradeAction == "Sell Short");
    if (!validTradeAction)
    {
        qDebug() << "Order invalid: tradeAction not recognized:" << m_tradeAction;
    }
    ASSUME_TRUE(validTradeAction); // ASSERT: tradeAction must be recognized

    return true;
}
