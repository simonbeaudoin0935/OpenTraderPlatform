#include "DumpPatternStrategy.h"

#include "Strategy/StrategySDK.h"
#include "Order.h"
#include "Bar.h"
#include "Trade.h"
#include "Position.h"
#include "PlaceOrder.h"

#include <QDebug>

// ──────────────────────────────────────────────────────────────────────────────
// Factory functions required by the strategy plugin interface
// ──────────────────────────────────────────────────────────────────────────────

extern "C"
{

    StrategyBase* createStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk)
    {
        return new DumpPatternStrategy(p_config, p_sdk);
    }

    void destroyStrategy(StrategyBase* p_strategy)
    {
        delete p_strategy;
    }

    const char* getStrategyAPIVersion()
    {
        return "1.0.0";
    }

} // extern "C"

// ──────────────────────────────────────────────────────────────────────────────
// Constructor
// ──────────────────────────────────────────────────────────────────────────────

DumpPatternStrategy::DumpPatternStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk)
    : QObject(nullptr), m_sdk(p_sdk), m_config(p_config)
{
    // Read config params (with defaults)
    auto it = p_config.customParams.find("entryOffsetCents");
    if (it != p_config.customParams.end())
    {
        m_entryOffsetDollars = it->second.toDouble() / 100.0;
    }
    it = p_config.customParams.find("cycleDurationSeconds");
    if (it != p_config.customParams.end())
    {
        m_cycleDurationSec = it->second.toInt();
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────────────────────────────────────

void DumpPatternStrategy::onStart(StrategySDK* p_sdk)
{
    m_sdk = p_sdk;
    m_sdk->log(QString("DumpPatternStrategy started — offset=$%1, cycle=%2s")
                   .arg(m_entryOffsetDollars, 0, 'f', 2)
                   .arg(m_cycleDurationSec));

    if (m_config.symbols.isEmpty())
    {
        m_sdk->log("ERROR: no symbols configured — stopping", LogLevel::Error);
        return;
    }

    const QString& symbol = m_config.symbols.first();
    m_sdk->log(QString("Subscribing to %1 ...").arg(symbol));

    // Subscribe to data feed; once confirmed, set cycle start time
    m_sdk->subscribeToSymbol(symbol).then(
        this,
        [this, symbol](bool accepted)
        {
            if (!accepted)
            {
                m_sdk->log(QString("ERROR: no replay data for %1 — strategy idle").arg(symbol), LogLevel::Error);
                return;
            }
            m_sdk->log(QString("Subscribed to %1 — waiting for first price ...").arg(symbol));
            m_cycleStartTime = m_sdk->getCurrentTime();
            m_state = State::Idle;
        });
}

void DumpPatternStrategy::onStop()
{
    m_sdk->log("DumpPatternStrategy stopping ...");

    // Cancel any open limit entry order
    if (!m_entryOrderID.isEmpty() && m_state == State::WaitingLimitEntry)
    {
        m_sdk->log(QString("Cancelling open entry order %1 on stop").arg(m_entryOrderID));
        std::ignore = m_sdk->cancelOrder(m_entryOrderID);
        m_entryOrderID.clear();
    }

    m_sdk->log(QString("Summary: cycles completed=%1, cycles cancelled=%2, total P&L=$%3")
                   .arg(m_cyclesCompleted)
                   .arg(m_cyclesCancelled)
                   .arg(m_totalPnL, 0, 'f', 2));
}

// ──────────────────────────────────────────────────────────────────────────────
// Data callbacks
// ──────────────────────────────────────────────────────────────────────────────

void DumpPatternStrategy::onBar(const Bar& p_bar)
{
    // Use bar close as fallback price (when no recent trades)
    if (m_lastPrice <= 0.0)
    {
        updatePrice(p_bar.getClose());
    }
    // Also tick the cycle check on every bar
    checkCycle();
}

void DumpPatternStrategy::onTrade(const Trade& p_trade)
{
    // Trade prints give us the most precise current price
    updatePrice(p_trade.m_price);
}

void DumpPatternStrategy::onOrderUpdated(const Order& p_order)
{
    const QString orderID = p_order.getOrderID();
    const Order::Status status = p_order.getOrderStatus();

    if (orderID == m_entryOrderID)
    {
        if (status == Order::Status::FLL || status == Order::Status::FLP)
        {
            m_sdk->log(QString("Entry order %1 filled at %2").arg(orderID).arg(p_order.getFilledPrice(), 0, 'f', 2));
            m_entryFillPrice = p_order.getFilledPrice();

            if (m_state == State::WaitingLimitEntry)
            {
                placeCloseOrder();
            }
        }
        else if (status == Order::Status::CAN || status == Order::Status::TSC)
        {
            m_sdk->log(QString("Entry order %1 cancelled").arg(orderID));
            if (m_state == State::WaitingLimitEntry)
            {
                m_cyclesCancelled++;
                resetCycle();
            }
        }
        else if (status == Order::Status::REJ)
        {
            auto reason = p_order.getRejectReason();
            m_sdk->log(QString("Entry order %1 rejected: %2")
                           .arg(orderID)
                           .arg(reason.has_value() ? reason.value() : "unknown"),
                       LogLevel::Warning);
            if (m_state == State::WaitingLimitEntry)
            {
                m_cyclesCancelled++;
                resetCycle();
            }
        }
    }
    else if (orderID == m_closeOrderID)
    {
        if (status == Order::Status::FLL || status == Order::Status::FLP)
        {
            double closeFill = p_order.getFilledPrice();
            double pnl = m_isLong ? (closeFill - m_entryFillPrice) : (m_entryFillPrice - closeFill);
            m_totalPnL += pnl;
            m_cyclesCompleted++;

            m_sdk->log(QString("Close order %1 filled at %2 | cycle P&L=$%3 | total P&L=$%4")
                           .arg(orderID)
                           .arg(closeFill, 0, 'f', 2)
                           .arg(pnl, 0, 'f', 2)
                           .arg(m_totalPnL, 0, 'f', 2));

            if (m_state == State::WaitingClose)
            {
                resetCycle();
            }
        }
    }
}

void DumpPatternStrategy::onOrderFilled(const Order& p_order)
{
    Q_UNUSED(p_order)
}

void DumpPatternStrategy::onOrderCancelled(const Order& p_order, const std::string& /*p_reason*/)
{
    Q_UNUSED(p_order)
}

void DumpPatternStrategy::onOrderRejected(const Order& p_order, const std::string& p_reason)
{
    m_sdk->log(QString("Order rejected: %1 — %2").arg(p_order.getOrderID()).arg(QString::fromStdString(p_reason)),
               LogLevel::Warning);
}

void DumpPatternStrategy::onPositionUpdated(const Position& p_position)
{
    Q_UNUSED(p_position)
}

void DumpPatternStrategy::onBalanceUpdated(double p_balance)
{
    Q_UNUSED(p_balance)
}

void DumpPatternStrategy::onError(const std::string& p_error)
{
    m_sdk->log(QString("Error: %1").arg(QString::fromStdString(p_error)), LogLevel::Error);
}

// ──────────────────────────────────────────────────────────────────────────────
// Private helpers
// ──────────────────────────────────────────────────────────────────────────────

void DumpPatternStrategy::updatePrice(double p_price)
{
    if (p_price > 0.0)
    {
        m_lastPrice = p_price;
    }
    checkCycle();
}

void DumpPatternStrategy::checkCycle()
{
    if (m_cycleStartTime.isNull())
    {
        return; // Not yet subscribed
    }

    const QDateTime now = m_sdk->getCurrentTime();
    const qint64 elapsedSec = m_cycleStartTime.secsTo(now);

    if (elapsedSec < m_cycleDurationSec)
    {
        return; // Phase not yet complete
    }

    switch (m_state)
    {
    case State::Idle:
        startEntryPhase();
        break;

    case State::WaitingLimitEntry:
        evaluateEntryPhase();
        break;

    case State::WaitingClose:
        evaluateClosePhase();
        break;
    }
}

void DumpPatternStrategy::startEntryPhase()
{
    if (m_lastPrice <= 0.0)
    {
        m_sdk->log("No price available yet — skipping cycle");
        m_cycleStartTime = m_sdk->getCurrentTime();
        return;
    }

    placeEntryOrder();
    m_cycleStartTime = m_sdk->getCurrentTime();
    m_state = State::WaitingLimitEntry;
}

void DumpPatternStrategy::evaluateEntryPhase()
{
    // Check if we already transitioned to WaitingClose (fill arrived before timeout)
    if (m_state == State::WaitingClose)
    {
        m_cycleStartTime = m_sdk->getCurrentTime();
        return;
    }

    // Not filled — cancel
    m_sdk->log(QString("Entry order not filled after %1s — cancelling").arg(m_cycleDurationSec));
    cancelEntryOrder();
    m_cyclesCancelled++;
    resetCycle();
}

void DumpPatternStrategy::evaluateClosePhase()
{
    // Close order should have been handled in onOrderUpdated
    // If we reach here it hasn't been filled yet — log warning and reset
    m_sdk->log(QString("Close order not confirmed after %1s — resetting cycle (position may remain open)")
                   .arg(m_cycleDurationSec),
               LogLevel::Warning);
    m_closeOrderID.clear();
    resetCycle();
}

void DumpPatternStrategy::resetCycle()
{
    m_isLong = !m_isLong; // Flip direction
    m_entryOrderID.clear();
    m_closeOrderID.clear();
    m_entryFillPrice = 0.0;
    m_state = State::Idle;
    m_cycleStartTime = m_sdk->getCurrentTime();
    m_sdk->log(QString("Cycle reset — next direction: %1").arg(m_isLong ? "LONG" : "SHORT"));
}

void DumpPatternStrategy::placeEntryOrder()
{
    const QString& symbol = m_config.symbols.first();
    QString accountID = "SIM123456";
    auto it = m_config.customParams.find("accountID");
    if (it != m_config.customParams.end())
    {
        accountID = it->second.toString();
    }

    // Long: buy at price + offset; Short: sell short at price - offset
    const double limitPrice = m_isLong ? (m_lastPrice + m_entryOffsetDollars) : (m_lastPrice - m_entryOffsetDollars);

    PlaceOrderRequest req;
    req.setSymbol(symbol);
    req.setAccountID(accountID);
    req.setQuantity(1);
    req.setOrderType(OrderType::Type::Limit);
    req.setTradeAction(m_isLong ? TradeAction::Buy : TradeAction::SellShort);
    req.setLimitPrice(limitPrice);
    req.setTimeInForce(TimeInForce(OrderDuration::Day));

    m_sdk->log(QString("Placing %1 LIMIT order for %2 @ $%3 (last=$%4)")
                   .arg(m_isLong ? "BUY" : "SELL SHORT")
                   .arg(symbol)
                   .arg(limitPrice, 0, 'f', 2)
                   .arg(m_lastPrice, 0, 'f', 2));

    m_sdk->placeOrder(req).then(
        this,
        [this](std::expected<PlaceOrderResult, TSClient::Error> result)
        {
            if (!result)
            {
                m_sdk->log(QString("Failed to place entry order: error %1").arg(static_cast<int>(result.error())),
                           LogLevel::Error);
                m_cyclesCancelled++;
                resetCycle();
                return;
            }

            const auto& orders = result->getOrders();
            if (orders.isEmpty() || orders.first().isError())
            {
                QString errMsg = orders.isEmpty() ? "no orders in result" : orders.first().getMessage();
                m_sdk->log(QString("Entry order rejected by server: %1").arg(errMsg), LogLevel::Error);
                m_cyclesCancelled++;
                resetCycle();
                return;
            }

            m_entryOrderID = orders.first().getOrderID();
            m_sdk->log(QString("Entry order placed: %1").arg(m_entryOrderID));
        });
}

void DumpPatternStrategy::placeCloseOrder()
{
    const QString& symbol = m_config.symbols.first();
    QString accountID = "SIM123456";
    auto it = m_config.customParams.find("accountID");
    if (it != m_config.customParams.end())
    {
        accountID = it->second.toString();
    }

    PlaceOrderRequest req;
    req.setSymbol(symbol);
    req.setAccountID(accountID);
    req.setQuantity(1);
    req.setOrderType(OrderType::Type::Market);
    // Long was filled → market sell; Short was filled → buy to cover
    req.setTradeAction(m_isLong ? TradeAction::Sell : TradeAction::BuyToCover);
    req.setTimeInForce(TimeInForce(OrderDuration::Day));

    m_sdk->log(
        QString("Placing %1 MARKET order for %2 (close position)").arg(m_isLong ? "SELL" : "BUY TO COVER").arg(symbol));

    m_sdk->placeOrder(req).then(
        this,
        [this](std::expected<PlaceOrderResult, TSClient::Error> result)
        {
            if (!result)
            {
                m_sdk->log(QString("Failed to place close order: error %1").arg(static_cast<int>(result.error())),
                           LogLevel::Error);
                resetCycle();
                return;
            }

            const auto& orders = result->getOrders();
            if (orders.isEmpty() || orders.first().isError())
            {
                QString errMsg = orders.isEmpty() ? "no orders in result" : orders.first().getMessage();
                m_sdk->log(QString("Close order rejected by server: %1").arg(errMsg), LogLevel::Error);
                resetCycle();
                return;
            }

            m_closeOrderID = orders.first().getOrderID();
            m_state = State::WaitingClose;
            m_cycleStartTime = m_sdk->getCurrentTime();
            m_sdk->log(QString("Close order placed: %1").arg(m_closeOrderID));
        });
}

void DumpPatternStrategy::cancelEntryOrder()
{
    if (m_entryOrderID.isEmpty())
    {
        return;
    }

    m_sdk->log(QString("Cancelling entry order %1").arg(m_entryOrderID));
    std::ignore = m_sdk->cancelOrder(m_entryOrderID);
    m_entryOrderID.clear();
}
