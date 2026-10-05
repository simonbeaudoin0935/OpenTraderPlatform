#pragma once

#include <QString>
#include "PlaceOrder.h"

/// @brief Validates strategy orders before they are routed to TSClient
/// Performs pre-flight checks such as symbol validation, position limits, and risk constraints.
/// Currently uses mocked-true implementations to allow all valid orders through.
/// This class is populated as risk management features are implemented.
class StrategyOrderValidator
{
  public:
    /// @brief Validate an entire order request
    /// @param p_strategyID ID of the strategy placing the order
    /// @param p_orderRequest The order details to validate
    /// @return true if order passes validation, false otherwise
    /// @note Currently returns true for all orders (mocked validation)
    [[nodiscard]] static bool validateOrder(const QString& p_strategyID, const PlaceOrderRequest& p_orderRequest);

    /// @brief Check if a strategy can trade a specific symbol
    /// @param p_strategyID ID of the strategy
    /// @param p_symbol The symbol to trade
    /// @return true if symbol is tradable, false otherwise
    /// @note Currently returns true for all symbols (mocked validation)
    [[nodiscard]] static bool canTradeSymbol(const QString& p_strategyID, const QString& p_symbol);

    /// @brief Check if a position size is within limits
    /// @param p_strategyID ID of the strategy
    /// @param p_quantity Quantity to validate
    /// @return true if quantity is acceptable, false otherwise
    /// @note Currently returns true for all quantities (mocked validation)
    [[nodiscard]] static bool checkPositionSize(const QString& p_strategyID, int p_quantity);

    /// @brief Check if order respects risk limits
    /// @param p_strategyID ID of the strategy
    /// @param p_orderRequest The order to check
    /// @return true if order is within risk limits, false otherwise
    /// @note Currently returns true for all orders (mocked validation)
    [[nodiscard]] static bool checkRiskLimit(const QString& p_strategyID, const PlaceOrderRequest& p_orderRequest);
};
