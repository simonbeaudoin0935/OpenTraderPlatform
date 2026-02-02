#include "ExampleStrategy.h"

ExampleStrategy::ExampleStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk) : m_barCount(0), m_lastBalance(0.0)
{
    Q_UNUSED(p_config);
    Q_UNUSED(p_sdk);
}

void ExampleStrategy::onStart(StrategySDK* p_sdk)
{
    Q_UNUSED(p_sdk);
    log(QString("[ExampleStrategy] Strategy started"));
    m_barCount = 0;
}

void ExampleStrategy::onStop()
{
    log(QString("[ExampleStrategy] Strategy stopped. Processed %1 bars.").arg(m_barCount));
}

void ExampleStrategy::onPause()
{
    log(QString("[ExampleStrategy] Strategy paused."));
}

void ExampleStrategy::onBar(const Bar& p_bar)
{
    m_barCount++;

    // Log every 10th bar to avoid spam
    if (m_barCount % 10 == 0)
    {
        log(QString("[ExampleStrategy] Bar #%1 Close: %2").arg(m_barCount).arg(p_bar.getClose()));
    }
}

void ExampleStrategy::onMarketDepth(const MarketDepthQuote& p_quote)
{
    // Log market depth update
    if (!p_quote.isEmpty())
    {
        auto bids = p_quote.getBids();
        auto asks = p_quote.getAsks();
        if (!bids.isEmpty() && !asks.isEmpty())
        {
            log(QString("[ExampleStrategy] Market depth bid: %1 ask: %2")
                    .arg(bids[0].getPrice())
                    .arg(asks[0].getPrice()));
        }
    }
}

void ExampleStrategy::onOrderUpdated(const Order& p_order)
{
    log(QString("[ExampleStrategy] Order updated: %1 Status: %2 Qty: %3 Price: %4")
            .arg(p_order.getOrderID())
            .arg(QString::number(static_cast<int>(p_order.getOrderStatus())))
            .arg(p_order.getQuantity())
            .arg(p_order.getLimitPrice().has_value() ? QString::number(p_order.getLimitPrice().value()) : "N/A"));
}

void ExampleStrategy::onOrderFilled(const Order& p_order)
{
    log(QString("[ExampleStrategy] Order filled: %1 Qty: %2").arg(p_order.getOrderID()).arg(p_order.getQuantity()));
}

void ExampleStrategy::onOrderCancelled(const Order& p_order, const std::string& p_reason)
{
    log(QString("[ExampleStrategy] Order cancelled: %1 Reason: %2").arg(p_order.getOrderID()).arg(p_reason.c_str()));
}

void ExampleStrategy::onOrderRejected(const Order& p_order, const std::string& p_reason)
{
    log(QString("[ExampleStrategy] Order rejected: %1 Reason: %2").arg(p_order.getOrderID()).arg(p_reason.c_str()));
}

void ExampleStrategy::onPositionUpdated(const Position& p_position)
{
    log(QString("[ExampleStrategy] Position updated: %1 Qty: %2 Avg Price: %3")
            .arg(p_position.getPositionID())
            .arg(p_position.getQuantity())
            .arg(p_position.getAveragePrice()));
}

void ExampleStrategy::onBalanceUpdated(double p_balance)
{
    if (m_lastBalance != p_balance)
    {
        log(QString("[ExampleStrategy] Balance updated: %1").arg(p_balance));
        m_lastBalance = p_balance;
    }
}

void ExampleStrategy::onError(const std::string& p_error)
{
    log(QString("[ExampleStrategy] Error: %1").arg(p_error.c_str()));
}

// Factory functions for plugin loading
extern "C"
{
    const char* getStrategyAPIVersion()
    {
        return "1.0.0";
    }

    StrategyBase* createStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk)
    {
        return new ExampleStrategy(p_config, p_sdk);
    }

    void destroyStrategy(StrategyBase* p_strategy)
    {
        delete p_strategy;
    }
}
