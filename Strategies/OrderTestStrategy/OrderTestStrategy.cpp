#include "OrderTestStrategy.h"
#include "Strategy/StrategySDK.h"
#include "PlaceOrder.h"
#include <QTimer>
#include <QJsonValue>

// ─── Factory functions ────────────────────────────────────────────────────────

extern "C"
{
    const char* getStrategyAPIVersion()
    {
        return "1.0.0";
    }

    StrategyBase* createStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk)
    {
        return new OrderTestStrategy(p_config, p_sdk);
    }

    void destroyStrategy(StrategyBase* p_strategy)
    {
        delete p_strategy;
    }
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────

OrderTestStrategy::OrderTestStrategy(const StrategyConfig& p_config, StrategySDK* /*p_sdk*/) : m_config(p_config) {}

OrderTestStrategy::~OrderTestStrategy()
{
    onStop();
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void OrderTestStrategy::onStart(StrategySDK* p_sdk)
{
    m_sdk = p_sdk;

    // Read configuration
    auto params = m_config.customParams;
    m_accountID = params.count("accountID") ? params.at("accountID").toString() : QString("SIM123456");
    m_limitPrice = params.count("limitPrice") ? params.at("limitPrice").toDouble(1.0) : 1.0;
    int timeoutSecs = params.count("timeoutSecs") ? params.at("timeoutSecs").toInt(60) : 60;

    m_symbol = m_config.symbols.isEmpty() ? "SPY" : m_config.symbols[0];

    m_sdk->log(QString("=== OrderTestStrategy starting ==="), LogLevel::Info);
    m_sdk->log(QString("  account : %1").arg(m_accountID), LogLevel::Info);
    m_sdk->log(QString("  symbol  : %1").arg(m_symbol), LogLevel::Info);
    m_sdk->log(QString("  limitPx : %1").arg(m_limitPrice), LogLevel::Info);
    m_sdk->log(QString("  timeout : %1s").arg(timeoutSecs), LogLevel::Info);

    // Log initial state
    double balance = m_sdk->getAccountBalance();
    QVector<Order> orders = m_sdk->getOrders();
    QVector<Position> positions = m_sdk->getPositions();
    m_sdk->log(QString("[Init] balance=$%1  openOrders=%2  positions=%3")
                   .arg(balance, 0, 'f', 2)
                   .arg(orders.size())
                   .arg(positions.size()),
               LogLevel::Info);

    // Arm watchdog
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setInterval(timeoutSecs * 1000);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &OrderTestStrategy::onTimeout);
    m_timeoutTimer->start();

    // Kick off test
    placeMarketOrder();
}

void OrderTestStrategy::onStop()
{
    if (m_timeoutTimer)
    {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }

    if (m_sdk)
    {
        m_sdk->log("=== OrderTestStrategy stopped ===", LogLevel::Info);
        m_sdk = nullptr;
    }
}

// ─── Test steps ───────────────────────────────────────────────────────────────

void OrderTestStrategy::placeMarketOrder()
{
    m_sdk->log(QString("[Step 1] Placing market BUY 1 %1 …").arg(m_symbol), LogLevel::Info);

    PlaceOrderRequest req;
    req.setAccountID(m_accountID);
    req.setSymbol(m_symbol);
    req.setOrderType(OrderType::Type::Market);
    req.setQuantity(1);
    req.setTradeAction(TradeAction::Buy);
    req.setTimeInForce(TimeInForce(OrderDuration::Day));

    auto future = m_sdk->placeOrder(req);
    future.then(
        this,
        [this](std::expected<PlaceOrderResult, TSClient::Error> result)
        {
            if (!result.has_value())
            {
                m_sdk->log(
                    QString("[Step 1] FAIL — placeOrder returned error: %1").arg(static_cast<int>(result.error())),
                    LogLevel::Error);
                m_state = State::Complete;
                printSummary();
                return;
            }

            const PlaceOrderResult& pr = result.value();
            if (pr.hasErrors() || pr.getOrders().isEmpty())
            {
                m_sdk->log(QString("[Step 1] FAIL — PlaceOrderResult has errors or no orders"), LogLevel::Error);
                m_state = State::Complete;
                printSummary();
                return;
            }

            m_marketOrderID = pr.getOrders().first().getOrderID();
            m_marketOrderPlaced = true;
            m_state = State::WaitMarketFill;
            m_sdk->log(QString("[Step 1] Market order placed — OrderID=%1, waiting for fill …").arg(m_marketOrderID),
                       LogLevel::Info);
        });
}

void OrderTestStrategy::placeLimitOrder()
{
    m_sdk->log(QString("[Step 2] Placing limit BUY 1 %1 @ $%2 …").arg(m_symbol).arg(m_limitPrice), LogLevel::Info);

    PlaceOrderRequest req;
    req.setAccountID(m_accountID);
    req.setSymbol(m_symbol);
    req.setOrderType(OrderType::Type::Limit);
    req.setQuantity(1);
    req.setTradeAction(TradeAction::Buy);
    req.setTimeInForce(TimeInForce(OrderDuration::Day));
    req.setLimitPrice(m_limitPrice);

    auto future = m_sdk->placeOrder(req);
    future.then(
        this,
        [this](std::expected<PlaceOrderResult, TSClient::Error> result)
        {
            if (!result.has_value())
            {
                m_sdk->log(
                    QString("[Step 2] FAIL — placeOrder returned error: %1").arg(static_cast<int>(result.error())),
                    LogLevel::Error);
                m_state = State::Complete;
                printSummary();
                return;
            }

            const PlaceOrderResult& pr = result.value();
            if (pr.hasErrors() || pr.getOrders().isEmpty())
            {
                m_sdk->log(QString("[Step 2] FAIL — PlaceOrderResult has errors or no orders"), LogLevel::Error);
                m_state = State::Complete;
                printSummary();
                return;
            }

            m_limitOrderID = pr.getOrders().first().getOrderID();
            m_limitOrderPlaced = true;
            m_state = State::WaitLimitOPN;
            m_sdk->log(QString("[Step 2] Limit order placed — OrderID=%1, waiting for OPN …").arg(m_limitOrderID),
                       LogLevel::Info);
        });
}

void OrderTestStrategy::cancelLimitOrder()
{
    m_sdk->log(QString("[Step 3] Cancelling limit order %1 …").arg(m_limitOrderID), LogLevel::Info);

    auto future = m_sdk->cancelOrder(m_limitOrderID);
    m_cancelSent = true;
    m_state = State::WaitCancel;

    future.then(
        this,
        [this](std::expected<CancelOrderResult, TSClient::Error> result)
        {
            if (!result.has_value())
            {
                m_sdk->log(
                    QString("[Step 3] FAIL — cancelOrder returned error: %1").arg(static_cast<int>(result.error())),
                    LogLevel::Error);
                // Don't give up — the CAN status may still arrive via onOrderUpdated
            }
            else
            {
                m_sdk->log(
                    QString("[Step 3] Cancel request acknowledged (OrderID=%1)").arg(result.value().getOrderID()),
                    LogLevel::Info);
            }
        });
}

// ─── Order & position callbacks ───────────────────────────────────────────────

void OrderTestStrategy::onOrderUpdated(const Order& p_order)
{
    m_sdk->log(QString("[Order] id=%1 status=%2")
                   .arg(p_order.getOrderID())
                   .arg(Order::getStatusDescriptionForStatus(p_order.getOrderStatus())),
               LogLevel::Info);

    if (m_state == State::WaitMarketFill)
    {
        if (p_order.getOrderID() == m_marketOrderID)
        {
            Order::Status st = p_order.getOrderStatus();
            if (st == Order::Status::FLL || st == Order::Status::FPR)
            {
                m_marketFillReceived = true;

                // Query SDK state immediately after fill
                QVector<Order> orders = m_sdk->getOrders();
                QVector<Position> positions = m_sdk->getPositions();
                m_sdk->log(QString("[Step 1] PASS — market order filled. SDK: orders=%1 positions=%2")
                               .arg(orders.size())
                               .arg(positions.size()),
                           LogLevel::Info);

                // Proceed to next step
                placeLimitOrder();
            }
            else if (st == Order::Status::REJ)
            {
                m_sdk->log(QString("[Step 1] FAIL — market order rejected"), LogLevel::Error);
                m_state = State::Complete;
                printSummary();
            }
        }
    }
    else if (m_state == State::WaitLimitOPN)
    {
        if (p_order.getOrderID() == m_limitOrderID)
        {
            Order::Status st = p_order.getOrderStatus();
            if (st == Order::Status::OPN || st == Order::Status::DON)
            {
                m_limitOPNReceived = true;
                m_sdk->log(QString("[Step 2] PASS — limit order is open"), LogLevel::Info);
                cancelLimitOrder();
            }
            else if (st == Order::Status::REJ)
            {
                m_sdk->log(QString("[Step 2] FAIL — limit order rejected"), LogLevel::Error);
                m_state = State::Complete;
                printSummary();
            }
        }
    }
    else if (m_state == State::WaitCancel)
    {
        if (p_order.getOrderID() == m_limitOrderID)
        {
            Order::Status st = p_order.getOrderStatus();
            if (st == Order::Status::CAN || st == Order::Status::TSC || st == Order::Status::BRO)
            {
                m_cancelConfirmed = true;
                m_sdk->log(QString("[Step 3] PASS — limit order cancelled"), LogLevel::Info);
                m_state = State::Complete;

                // Stop watchdog — test done
                if (m_timeoutTimer)
                {
                    m_timeoutTimer->stop();
                }
                printSummary();
            }
        }
    }
}

void OrderTestStrategy::onOrderFilled(const Order& /*p_order*/)
{
    // Handled via onOrderUpdated
}

void OrderTestStrategy::onOrderCancelled(const Order& /*p_order*/, const std::string& /*p_reason*/)
{
    // Handled via onOrderUpdated
}

void OrderTestStrategy::onOrderRejected(const Order& p_order, const std::string& p_reason)
{
    m_sdk->log(
        QString("[Order] REJECTED id=%1 reason=%2").arg(p_order.getOrderID()).arg(QString::fromStdString(p_reason)),
        LogLevel::Warning);
}

void OrderTestStrategy::onPositionUpdated(const Position& p_position)
{
    m_sdk->log(QString("[Position] id=%1 qty=%2 avgPx=%3")
                   .arg(p_position.getPositionID())
                   .arg(p_position.getQuantity())
                   .arg(p_position.getAveragePrice()),
               LogLevel::Info);
}

void OrderTestStrategy::onBalanceUpdated(double p_balance)
{
    m_sdk->log(QString("[Balance] $%1").arg(p_balance, 0, 'f', 2), LogLevel::Debug);
}

void OrderTestStrategy::onBar(const Bar& /*p_bar*/)
{
    // Not needed for order testing
}

void OrderTestStrategy::onError(const std::string& p_error)
{
    if (m_sdk)
    {
        m_sdk->log(QString("[Error] %1").arg(QString::fromStdString(p_error)), LogLevel::Error);
    }
}

// ─── Watchdog ─────────────────────────────────────────────────────────────────

void OrderTestStrategy::onTimeout()
{
    if (!m_sdk || m_state == State::Complete)
    {
        return;
    }
    m_sdk->log(QString("[Timeout] Test timed out in state %1").arg(static_cast<int>(m_state)), LogLevel::Error);
    m_state = State::Complete;
    printSummary();
}

// ─── Summary ─────────────────────────────────────────────────────────────────

void OrderTestStrategy::printSummary()
{
    if (!m_sdk)
    {
        return;
    }

    m_sdk->log("", LogLevel::Info);
    m_sdk->log("════════════════════════════════════════", LogLevel::Info);
    m_sdk->log("         ORDER TEST RESULTS", LogLevel::Info);
    m_sdk->log("════════════════════════════════════════", LogLevel::Info);

    auto pass = [](bool ok) -> QString { return ok ? "PASS ✓" : "FAIL ✗"; };

    m_sdk->log(QString("  [1] Market order placed   : %1").arg(pass(m_marketOrderPlaced)), LogLevel::Info);
    m_sdk->log(QString("  [2] Market order filled   : %1").arg(pass(m_marketFillReceived)), LogLevel::Info);
    m_sdk->log(QString("  [3] Limit order placed    : %1").arg(pass(m_limitOrderPlaced)), LogLevel::Info);
    m_sdk->log(QString("  [4] Limit order OPN       : %1").arg(pass(m_limitOPNReceived)), LogLevel::Info);
    m_sdk->log(QString("  [5] Cancel request sent   : %1").arg(pass(m_cancelSent)), LogLevel::Info);
    m_sdk->log(QString("  [6] Cancel confirmed      : %1").arg(pass(m_cancelConfirmed)), LogLevel::Info);
    m_sdk->log("════════════════════════════════════════", LogLevel::Info);

    bool allPassed = m_marketOrderPlaced && m_marketFillReceived && m_limitOrderPlaced && m_limitOPNReceived &&
                     m_cancelSent && m_cancelConfirmed;

    m_sdk->log(allPassed ? "  OVERALL: ALL TESTS PASSED ✓" : "  OVERALL: SOME TESTS FAILED ✗", LogLevel::Info);
    m_sdk->log("════════════════════════════════════════", LogLevel::Info);
    m_sdk->log("", LogLevel::Info);

    // Final SDK state snapshot
    m_sdk->log(QString("[Final State] orders=%1  positions=%2  balance=$%3")
                   .arg(m_sdk->getOrders().size())
                   .arg(m_sdk->getPositions().size())
                   .arg(m_sdk->getAccountBalance(), 0, 'f', 2),
               LogLevel::Info);
}
