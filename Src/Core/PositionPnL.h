#pragma once

#include "Order.h"
#include "Position.h"
#include <QVector>
#include <optional>

namespace PositionPnL
{
    struct Result
    {
        std::optional<double> grossProfit;
        QString reason;
        std::optional<double> peakQuantity;
    };

    [[nodiscard]] Result calculateClosedPosition(const Position& p_position, const QVector<Order>& p_orders);
} // namespace PositionPnL
