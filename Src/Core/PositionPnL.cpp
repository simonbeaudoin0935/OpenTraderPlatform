#include "PositionPnL.h"

#include <algorithm>
#include <cmath>

PositionPnL::Result PositionPnL::calculateClosedPosition(const Position& p_position, const QVector<Order>& p_orders)
{
    const auto unavailable = [](const QString& p_reason) -> Result { return {std::nullopt, p_reason, std::nullopt}; };
    if (p_position.getAssetType() != QStringLiteral("STOCK"))
    {
        return unavailable(QStringLiteral("Fill-based P&L currently supports stock positions only"));
    }
    if (!p_position.getOpenedDateTime().isValid() || !p_position.getClosedDateTime().isValid())
    {
        return unavailable(QStringLiteral("Position lifecycle timestamps are incomplete"));
    }
    if (p_position.getLongShort() != QStringLiteral("Long") && p_position.getLongShort() != QStringLiteral("Short"))
    {
        return unavailable(QStringLiteral("Position direction is invalid"));
    }
    bool quantityValid = false;
    const double positionQuantity = p_position.getQuantity().toDouble(&quantityValid);
    if (!quantityValid || !std::isfinite(positionQuantity))
    {
        return unavailable(QStringLiteral("Position quantity is invalid"));
    }
    if (!qFuzzyIsNull(positionQuantity))
    {
        return unavailable(QStringLiteral("Position is not closed"));
    }

    QVector<Order> orders;
    for (const Order& order: p_orders)
    {
        if (order.getAccountID() == p_position.getAccountID() && order.getSymbol() == p_position.getSymbol())
        {
            orders.append(order);
        }
    }
    const auto executionTime = [](const Order& p_order)
    { return p_order.getClosedDateTime().isValid() ? p_order.getClosedDateTime() : p_order.getOpenedDateTime(); };
    std::sort(orders.begin(),
              orders.end(),
              [&](const Order& p_left, const Order& p_right)
              {
                  const auto leftTime = executionTime(p_left);
                  const auto rightTime = executionTime(p_right);
                  return leftTime == rightTime ? p_left.getOrderID() < p_right.getOrderID() : leftTime < rightTime;
              });

    double quantity = 0.0;
    double cashFlow = 0.0;
    double peakQuantity = 0.0;
    QDateTime openedAt;
    bool shortCycle = false;
    bool incomplete = false;
    std::optional<double> matchedProfit;
    std::optional<double> matchedPeak;
    for (const Order& order: orders)
    {
        const auto time = executionTime(order);
        if (!time.isValid())
        {
            if (order.getFilledPrice() > 0.0 ||
                (order.m_executedQuantity.has_value() && order.m_executedQuantity.value() > 0.0))
            {
                return unavailable(QStringLiteral("Execution timestamp is missing"));
            }
            continue;
        }
        if (time > p_position.getClosedDateTime())
        {
            continue;
        }
        if (order.m_fillIsSynthetic)
        {
            return unavailable(QStringLiteral("Awaiting broker confirmation of a synthetic fill estimate"));
        }
        const bool filled = order.getOrderStatus() == Order::Status::FLL;
        double executedQuantity = 0.0;
        if (order.m_executedQuantity.has_value())
        {
            executedQuantity = order.m_executedQuantity.value();
        }
        else if (filled)
        {
            bool ok = false;
            executedQuantity = order.getQuantity().toDouble(&ok);
            if (!ok)
            {
                return unavailable(QStringLiteral("Invalid filled order quantity"));
            }
        }
        else if (order.getFilledPrice() > 0.0)
        {
            return unavailable(QStringLiteral("Executed quantity is missing for a partially filled order"));
        }
        else
        {
            continue;
        }
        if (!std::isfinite(executedQuantity) || executedQuantity < 0.0)
        {
            return unavailable(QStringLiteral("Invalid executed quantity"));
        }
        if (qFuzzyIsNull(executedQuantity))
        {
            continue;
        }
        if (!std::isfinite(order.getFilledPrice()) || order.getFilledPrice() <= 0.0)
        {
            return unavailable(QStringLiteral("Execution price is missing"));
        }

        const QString action = order.getTradeAction().toUpper().remove(' ');
        const bool opening = action == QStringLiteral("BUY") || action == QStringLiteral("SELLSHORT");
        const bool closing = action == QStringLiteral("SELL") || action == QStringLiteral("BUYTOCOVER");
        const bool shortOrder = action == QStringLiteral("SELLSHORT") || action == QStringLiteral("BUYTOCOVER");
        if (!opening && !closing)
        {
            return unavailable(QStringLiteral("Unsupported trade action in fill history"));
        }
        if (qFuzzyIsNull(quantity))
        {
            if (!opening)
            {
                // A ledger may start with the liquidation of a position opened before recording began.
                continue;
            }
            openedAt = time;
            shortCycle = shortOrder;
            cashFlow = 0.0;
            peakQuantity = 0.0;
            incomplete = false;
        }
        if (shortOrder != shortCycle)
        {
            return unavailable(QStringLiteral("Conflicting position directions in fill history"));
        }
        const bool sell = action == QStringLiteral("SELL") || action == QStringLiteral("SELLSHORT");
        cashFlow += (sell ? 1.0 : -1.0) * executedQuantity * order.getFilledPrice();
        quantity += opening ? executedQuantity : -executedQuantity;
        peakQuantity = std::max(peakQuantity, quantity);
        if (quantity < 0.0 && !qFuzzyIsNull(quantity))
        {
            incomplete = true;
            quantity = 0.0;
        }
        if (!qFuzzyIsNull(quantity))
        {
            continue;
        }
        quantity = 0.0;
        if (openedAt <= p_position.getOpenedDateTime() && time >= p_position.getOpenedDateTime() &&
            shortCycle == (p_position.getLongShort() == QStringLiteral("Short")))
        {
            if (incomplete || matchedProfit.has_value())
            {
                return unavailable(QStringLiteral("Incomplete or ambiguous position fill history"));
            }
            matchedProfit = cashFlow;
            matchedPeak = peakQuantity;
        }
    }
    if (!matchedProfit.has_value())
    {
        return unavailable(QStringLiteral("Awaiting complete entry and exit fills for this position"));
    }
    return {matchedProfit, {}, matchedPeak};
}
