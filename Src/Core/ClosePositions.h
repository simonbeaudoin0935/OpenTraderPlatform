#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "PlaceOrder.h"

enum class ClosePositionsExecutionMode
{
    AggressiveMarketable,
    PassiveResting,
};

[[nodiscard]] inline QString closePositionsExecutionModeToString(const ClosePositionsExecutionMode p_mode)
{
    switch (p_mode)
    {
    case ClosePositionsExecutionMode::AggressiveMarketable:
        return "aggressive";
    case ClosePositionsExecutionMode::PassiveResting:
        return "passive";
    }

    return "unknown";
}

struct ClosePositionsRequest
{
    QString accountId;
    QStringList symbols; // Empty means "all open positions for the account"
    double aggressivityOffsetCents = 0.0;
    ClosePositionsExecutionMode executionMode = ClosePositionsExecutionMode::AggressiveMarketable;
};

struct ClosePositionItemResult
{
    QString positionId;
    QString accountId;
    QString symbol;
    QString longShort;
    int quantity = 0;
    TradeAction tradeAction = TradeAction::Sell;
    OrderType::Type orderType = OrderType::Type::Market;
    OrderDuration duration = OrderDuration::Day;
    std::optional<double> limitPrice;
    bool submitted = false;
    bool placementSucceeded = false;
    QStringList orderIds;
    QStringList brokerMessages;
    QStringList brokerErrors;
    std::optional<QString> failureCode;
    QString failureMessage;

    [[nodiscard]] bool isSuccessful() const
    {
        return failureMessage.isEmpty() && placementSucceeded;
    }

    [[nodiscard]] bool hasFailure() const
    {
        return !failureMessage.isEmpty() || (submitted && !placementSucceeded);
    }
};

struct ClosePositionsResult
{
    QString accountId;
    QStringList requestedSymbols;
    QString session;
    ClosePositionsExecutionMode executionMode = ClosePositionsExecutionMode::AggressiveMarketable;
    bool usesLimitOrders = false;
    bool usesAggressiveLimitOrders = false;
    bool forcedDayPlus = false;
    double aggressivityOffsetCents = 0.0;
    int matchedPositionCount = 0;
    int submittedOrderCount = 0;
    QVector<ClosePositionItemResult> items;

    [[nodiscard]] int successCount() const
    {
        int count = 0;
        for (const ClosePositionItemResult& item: items)
        {
            if (item.isSuccessful())
            {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] int failureCount() const
    {
        int count = 0;
        for (const ClosePositionItemResult& item: items)
        {
            if (item.hasFailure())
            {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] bool hasFailures() const
    {
        return failureCount() > 0;
    }
};
