#include "OrderEmulator.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#include "Assume.h"
#include "Logging.h"
#include "MainApp.h"
#include "MarketDepthQuote.h"

#define LOGGING_CATEGORY OrderEmulatorLog
Q_LOGGING_CATEGORY(OrderEmulatorLog, "OrderEmulator");

namespace
{
    // Convert TradeAction enum to string for JSON
    QString tradeActionToString(TradeAction p_action)
    {
        switch (p_action)
        {
        case TradeAction::Buy:
            return "BUY";
        case TradeAction::Sell:
            return "SELL";
        case TradeAction::BuyToCover:
            return "BUYTOCOVER";
        case TradeAction::SellShort:
            return "SELLSHORT";
        case TradeAction::BuyToOpen:
            return "BUYTOOPEN";
        case TradeAction::BuyToClose:
            return "BUYTOCLOSE";
        case TradeAction::SellToOpen:
            return "SELLTOOPEN";
        case TradeAction::SellToClose:
            return "SELLTOCLOSE";
        }
        return "UNKNOWN";
    }

    // Get best bid price from market depth
    double getBestBid(const MarketDepthQuote& p_depth)
    {
        const auto& bids = p_depth.getBids();
        if (bids.isEmpty())
        {
            return 0.0;
        }
        return bids.first().getPrice().toDouble();
    }

    // Get best ask price from market depth
    double getBestAsk(const MarketDepthQuote& p_depth)
    {
        const auto& asks = p_depth.getAsks();
        if (asks.isEmpty())
        {
            return 0.0;
        }
        return asks.first().getPrice().toDouble();
    }
} // anonymous namespace

OrderEmulator::OrderEmulator(QObject* p_parent) : QObject(p_parent)
{
    setObjectName("OrderEmulator");

    m_receptionTimer.setSingleShot(true);
    m_executionTimer.setSingleShot(true);

    connect(&m_receptionTimer, &QTimer::timeout, this, &OrderEmulator::onReceptionDelayElapsed);
    connect(&m_executionTimer, &QTimer::timeout, this, &OrderEmulator::onExecutionDelayElapsed);

    INFO << "OrderEmulator created with starting balance:" << m_balance;
}

OrderEmulator::~OrderEmulator()
{
    DEBUG << "OrderEmulator destroyed";
}

void OrderEmulator::placeOrder(const PlaceOrderRequest& p_request, const QString& p_requestID)
{
    DEBUG << "placeOrder called:" << p_request.getSymbol() << "qty:" << p_request.getQuantity()
          << "type:" << OrderType::toString(p_request.getOrderType().type) << "requestID:" << p_requestID;

    // Validate order
    QString errorMessage;
    if (!validateOrder(p_request, errorMessage))
    {
        WARNING << "Order validation failed:" << errorMessage;

        // Create rejected order JSON
        QJsonObject rejectJson = createOrderJson(generateOrderID(), p_request, Order::Status::REJ);
        rejectJson["StatusDescription"] = errorMessage;

        Order rejectedOrder(rejectJson);
        m_filledOrders.insert(rejectedOrder.getOrderID(), rejectedOrder);
        emit orderStatusUpdate(orderToJson(rejectedOrder));
        return;
    }

    // Add to pending orders
    PendingOrder pending;
    pending.request = p_request;
    pending.requestID = p_requestID;
    pending.submitTimeMs = QDateTime::currentMSecsSinceEpoch();
    m_pendingOrders.append(pending);

    // Start timer if not already running
    if (!m_receptionTimer.isActive() && !m_pendingOrders.isEmpty())
    {
        m_receptionTimer.start(calculateReceptionDelay());
    }

    DEBUG << "Order queued, pending count:" << m_pendingOrders.size();
}

void OrderEmulator::cancelOrder(const QString& p_orderID, const QString& p_requestID)
{
    Q_UNUSED(p_requestID)
    DEBUG << "cancelOrder called:" << p_orderID;

    // Find in open orders
    auto it = m_openOrders.find(p_orderID);
    if (it == m_openOrders.end())
    {
        WARNING << "Order not found for cancellation:" << p_orderID;
        return;
    }

    // Get the existing order and create cancelled version
    Order& order = it.value();

    // Create updated order JSON with CAN status
    QJsonObject cancelJson;
    cancelJson["OrderID"] = order.getOrderID();
    cancelJson["AccountID"] = order.getAccountID();
    cancelJson["Status"] = "CAN";
    cancelJson["StatusDescription"] = Order::getStatusDescriptionForStatus(Order::Status::CAN);
    cancelJson["Symbol"] = order.getSymbol();
    cancelJson["Quantity"] = order.getQuantity();
    cancelJson["TradeAction"] = order.getTradeAction();
    cancelJson["ClosedDateTime"] = MainApp::currentAppReplayTime.toString(Qt::ISODate);

    Order cancelledOrder(cancelJson);

    // Move to filled orders
    m_filledOrders.insert(p_orderID, cancelledOrder);
    m_openOrders.remove(p_orderID);

    emit orderStatusUpdate(orderToJson(cancelledOrder));
    INFO << "Order cancelled:" << p_orderID;
}

void OrderEmulator::updateMarketDepth(const QString& p_symbol, const MarketDepthQuote& p_depth)
{
    m_depthSnapshots.insert(p_symbol, p_depth);

    // Check if any open limit orders can now fill
    for (auto it = m_openOrders.begin(); it != m_openOrders.end();)
    {
        Order& order = it.value();
        if (order.getSymbol() != p_symbol)
        {
            ++it;
            continue;
        }

        if (canFillLimitOrder(order, p_depth))
        {
            // Fill at market price: a buy limit crossing the ask fills at the ask,
            // not the (higher) limit price. Same logic as a market order.
            double fillPrice = calculateMarketOrderFillPrice(order, p_depth);

            // Convert order to JSON for executing queue - copy all fields
            QJsonObject orderJson = orderToJsonObject(order);

            // Add to executing orders with delay
            ExecutingOrder executing;
            executing.orderJson = orderJson;
            executing.fillPrice = fillPrice;
            m_executingOrders.append(executing);

            it = m_openOrders.erase(it);

            if (!m_executionTimer.isActive() && !m_executingOrders.isEmpty())
            {
                m_executionTimer.start(calculateExecutionDelay());
            }

            DEBUG << "Limit order can now fill:" << order.getOrderID() << "at" << fillPrice;
        }
        else
        {
            ++it;
        }
    }

    // Update position P&L if we have an active position for this symbol
    if (m_symbolToActivePosition.contains(p_symbol))
    {
        recalculatePositionPnL(p_symbol);
    }
}

void OrderEmulator::updateBarClose(const QString& p_symbol, double p_close)
{
    m_latestBarClose.insert(p_symbol, p_close);
}

void OrderEmulator::updateQuote(const QString& p_symbol, const Quote& p_quote)
{
    m_quoteSnapshots.insert(p_symbol, p_quote);

    // Only check limit orders if we don't have Level 2 data for this symbol
    // (Level 2 data is preferred and handled by updateMarketDepth)
    if (m_depthSnapshots.contains(p_symbol))
    {
        return;
    }

    // Check if any open limit orders can now fill using Level 1 data
    for (auto it = m_openOrders.begin(); it != m_openOrders.end();)
    {
        Order& order = it.value();
        if (order.getSymbol() != p_symbol)
        {
            ++it;
            continue;
        }

        if (canFillLimitOrderFromQuote(order, p_quote))
        {
            // Fill at market price: a buy limit crossing the ask fills at the ask,
            // not the (higher) limit price. Same logic as a market order.
            double fillPrice = calculateMarketOrderFillPriceFromQuote(order, p_quote);

            // Convert order to JSON for executing queue
            QJsonObject orderJson = orderToJsonObject(order);

            // Add to executing orders with delay
            ExecutingOrder executing;
            executing.orderJson = orderJson;
            executing.fillPrice = fillPrice;
            m_executingOrders.append(executing);

            it = m_openOrders.erase(it);

            if (!m_executionTimer.isActive() && !m_executingOrders.isEmpty())
            {
                m_executionTimer.start(calculateExecutionDelay());
            }

            DEBUG << "Limit order can now fill (L1):" << order.getOrderID() << "at" << fillPrice;
        }
        else
        {
            ++it;
        }
    }

    // Update position P&L if we have an active position for this symbol
    // (only when Level 2 data is not available, as it would handle P&L updates)
    if (m_symbolToActivePosition.contains(p_symbol))
    {
        recalculatePositionPnL(p_symbol);
    }
}

QVector<Order> OrderEmulator::getOrders() const
{
    QVector<Order> result;
    result.reserve(m_openOrders.size() + m_filledOrders.size());

    for (const Order& order: m_openOrders)
    {
        result.append(order);
    }
    for (const Order& order: m_filledOrders)
    {
        result.append(order);
    }

    return result;
}

QVector<Position> OrderEmulator::getPositions() const
{
    QVector<Position> result;
    result.reserve(m_positions.size());

    for (const Position& position: m_positions)
    {
        result.append(position);
    }

    return result;
}

void OrderEmulator::setReplaySpeed(int p_speedPercent)
{
    m_replaySpeedPercent = p_speedPercent;
    DEBUG << "Replay speed set to:" << p_speedPercent << "%";
}

void OrderEmulator::clear()
{
    m_receptionTimer.stop();
    m_executionTimer.stop();
    m_pendingOrders.clear();
    m_executingOrders.clear();
    m_openOrders.clear();
    m_filledOrders.clear();
    m_positions.clear();
    m_positionData.clear();
    m_symbolToActivePosition.clear();
    m_closedPositionPnL.clear();
    m_totalClosedPnL = 0.0;
    m_depthSnapshots.clear();
    m_quoteSnapshots.clear();
    m_latestBarClose.clear();
    m_nextOrderID = 1000;
    m_nextPositionID = 60000000;
    m_balance = 100000.0;
    m_realizedProfitLoss = 0.0;

    INFO << "OrderEmulator cleared";
}

void OrderEmulator::pause()
{
    // Store remaining time for pending timers
    if (m_receptionTimer.isActive())
    {
        for (PendingOrder& pending: m_pendingOrders)
        {
            pending.remainingDelayMs = m_receptionTimer.remainingTime();
        }
        m_receptionTimer.stop();
    }

    if (m_executionTimer.isActive())
    {
        for (ExecutingOrder& executing: m_executingOrders)
        {
            executing.remainingDelayMs = m_executionTimer.remainingTime();
        }
        m_executionTimer.stop();
    }

    DEBUG << "OrderEmulator paused";
}

void OrderEmulator::resume()
{
    // Resume pending orders timer
    if (!m_pendingOrders.isEmpty())
    {
        int remainingMs = static_cast<int>(m_pendingOrders.first().remainingDelayMs);
        if (remainingMs > 0)
        {
            m_receptionTimer.start(remainingMs);
        }
        else
        {
            m_receptionTimer.start(calculateReceptionDelay());
        }
    }

    // Resume executing orders timer
    if (!m_executingOrders.isEmpty())
    {
        int remainingMs = static_cast<int>(m_executingOrders.first().remainingDelayMs);
        if (remainingMs > 0)
        {
            m_executionTimer.start(remainingMs);
        }
        else
        {
            m_executionTimer.start(calculateExecutionDelay());
        }
    }

    DEBUG << "OrderEmulator resumed";
}

QString OrderEmulator::generateOrderID()
{
    // Generate 9-digit numeric order IDs like TradeStation (e.g., "935936728")
    // Start at 900000000 to match TradeStation format and avoid conflicts
    return QString::number(m_nextOrderID++);
}

int OrderEmulator::calculateReceptionDelay() const
{
    if (m_replaySpeedPercent == -1) // As fast as possible
    {
        return 0;
    }

    int baseDelay = QRandomGenerator::global()->bounded(MIN_RECEPTION_DELAY_MS, MAX_RECEPTION_DELAY_MS + 1);

    // Scale by speed: at 200% speed, delays are halved
    return (baseDelay * 100) / qMax(1, m_replaySpeedPercent);
}

int OrderEmulator::calculateExecutionDelay() const
{
    if (m_replaySpeedPercent == -1) // As fast as possible
    {
        return 0;
    }

    int baseDelay = QRandomGenerator::global()->bounded(MIN_EXECUTION_DELAY_MS, MAX_EXECUTION_DELAY_MS + 1);

    return (baseDelay * 100) / qMax(1, m_replaySpeedPercent);
}

bool OrderEmulator::canFillLimitOrder(const Order& p_order, const MarketDepthQuote& p_depth) const
{
    if (!p_order.getLimitPrice().has_value())
    {
        return false;
    }

    double limitPrice = p_order.getLimitPrice().value();
    QString tradeAction = p_order.getTradeAction();

    // BUY limit: fills if ask <= limit price
    if (tradeAction == "BUY" || tradeAction == "BUYTOCOVER" || tradeAction == "Buy" || tradeAction == "Buy to Cover")
    {
        double bestAsk = getBestAsk(p_depth);
        return bestAsk > 0 && bestAsk <= limitPrice;
    }

    // SELL limit: fills if bid >= limit price
    if (tradeAction == "SELL" || tradeAction == "SELLSHORT" || tradeAction == "Sell" || tradeAction == "Sell Short")
    {
        double bestBid = getBestBid(p_depth);
        return bestBid > 0 && bestBid >= limitPrice;
    }

    return false;
}

double OrderEmulator::calculateMarketOrderFillPrice(const Order& p_order, const MarketDepthQuote& p_depth) const
{
    QString tradeAction = p_order.getTradeAction();

    // For now, simplified: use best bid/ask
    // TODO: Implement full book walking algorithm
    if (tradeAction == "BUY" || tradeAction == "BUYTOCOVER" || tradeAction == "Buy" || tradeAction == "Buy to Cover")
    {
        return getBestAsk(p_depth);
    }

    // SELL
    return getBestBid(p_depth);
}

bool OrderEmulator::canFillLimitOrderFromQuote(const Order& p_order, const Quote& p_quote) const
{
    if (!p_order.getLimitPrice().has_value())
    {
        return false;
    }

    double limitPrice = p_order.getLimitPrice().value();
    QString tradeAction = p_order.getTradeAction();

    // BUY limit: fills if ask <= limit price
    if (tradeAction == "BUY" || tradeAction == "BUYTOCOVER" || tradeAction == "Buy" || tradeAction == "Buy to Cover")
    {
        double ask = p_quote.getAsk();
        return ask > 0 && ask <= limitPrice;
    }

    // SELL limit: fills if bid >= limit price
    if (tradeAction == "SELL" || tradeAction == "SELLSHORT" || tradeAction == "Sell" || tradeAction == "Sell Short")
    {
        double bid = p_quote.getBid();
        return bid > 0 && bid >= limitPrice;
    }

    return false;
}

double OrderEmulator::calculateMarketOrderFillPriceFromQuote(const Order& p_order, const Quote& p_quote) const
{
    QString tradeAction = p_order.getTradeAction();

    if (tradeAction == "BUY" || tradeAction == "BUYTOCOVER" || tradeAction == "Buy" || tradeAction == "Buy to Cover")
    {
        return p_quote.getAsk();
    }

    // SELL
    return p_quote.getBid();
}

QJsonObject OrderEmulator::createOrderJson(const QString& p_orderID,
                                           const PlaceOrderRequest& p_request,
                                           Order::Status p_status) const
{
    QJsonObject json;
    json["OrderID"] = p_orderID;
    json["AccountID"] = p_request.getAccountID();
    json["Symbol"] = p_request.getSymbol();
    json["Quantity"] = QString::number(p_request.getQuantity());
    json["TradeAction"] = tradeActionToString(p_request.getTradeAction());
    json["Status"] = QtEnum::toString(p_status);
    json["StatusDescription"] = Order::getStatusDescriptionForStatus(p_status);

    json["OpenedDateTime"] = MainApp::currentAppReplayTime.toString(Qt::ISODate);
    json["FilledPrice"] = "0.00"; // Default for non-filled orders
    json["CommissionFee"] = "0.00";
    json["ConversionRate"] = "1.00";
    json["Currency"] = "USD";
    json["Duration"] = "DAY"; //FIME: Map from TimeInForce
    json["Routing"] = "Intelligent";
    json["PriceUsedForBuyingPower"] = "0.00";
    json["ShowOnlyQuantity"] = "0";
    json["Spread"] = "0.00";
    json["UnbundledRouteFee"] = "0.00";

    // Order type
    OrderType ot = p_request.getOrderType();
    json["OrderType"] = OrderType::toString(ot.type);

    // Optional limit price
    if (p_request.getLimitPrice().has_value())
    {
        json["LimitPrice"] = QString::number(p_request.getLimitPrice().value(), 'f', 4);
    }

    // Optional stop price
    if (p_request.getStopPrice().has_value())
    {
        json["StopPrice"] = QString::number(p_request.getStopPrice().value(), 'f', 4);
    }

    return json;
}

void OrderEmulator::fillOrder(const QJsonObject& p_orderJson, double p_fillPrice)
{
    // Create filled order JSON
    QJsonObject fillJson = p_orderJson;
    fillJson["Status"] = "FLL";
    fillJson["StatusDescription"] = Order::getStatusDescriptionForStatus(Order::Status::FLL);
    fillJson["FilledPrice"] = QString::number(p_fillPrice, 'f', 4);
    fillJson["ClosedDateTime"] = MainApp::currentAppReplayTime.toString(Qt::ISODate);

    Order filledOrder(fillJson);
    m_filledOrders.insert(filledOrder.getOrderID(), filledOrder);

    emit orderStatusUpdate(orderToJson(filledOrder));

    // Update position
    updatePosition(filledOrder, p_fillPrice);

    INFO << "Order filled:" << filledOrder.getOrderID() << "at" << p_fillPrice;
}

void OrderEmulator::updatePosition(const Order& p_filledOrder, double p_fillPrice)
{
    QString symbol = p_filledOrder.getSymbol();
    QString tradeAction = p_filledOrder.getTradeAction();
    int quantity = p_filledOrder.getQuantity().toInt();

    // Check if there's an active position for this symbol
    QString positionID;

    if (m_symbolToActivePosition.contains(symbol))
    {
        positionID = m_symbolToActivePosition[symbol];
    }
    else
    {
        // Create new position with unique ID
        positionID = QString::number(m_nextPositionID++);

        PositionData newPos;
        newPos.positionID = positionID;
        newPos.symbol = symbol;
        newPos.quantity = 0;
        newPos.averagePrice = 0;
        newPos.realizedPnL = 0;
        newPos.isLong = true; // Will be determined by first trade
        m_positionData.insert(positionID, newPos);
        m_symbolToActivePosition.insert(symbol, positionID);
    }

    PositionData& posData = m_positionData[positionID];
    int currentQty = posData.quantity;
    double currentAvgPrice = posData.averagePrice;

    // Calculate new position
    int newQty = currentQty;
    double newAvgPrice = currentAvgPrice;
    double realizedPnL = 0.0;

    bool isBuy =
        (tradeAction == "BUY" || tradeAction == "BUYTOCOVER" || tradeAction == "Buy" || tradeAction == "Buy to Cover");

    if (isBuy)
    {
        if (currentQty >= 0)
        {
            // Long position: average up
            double totalCost = (currentQty * currentAvgPrice) + (quantity * p_fillPrice);
            newQty = currentQty + quantity;
            newAvgPrice = (newQty > 0) ? (totalCost / newQty) : 0;
            posData.isLong = true; // Opening or adding to long position
        }
        else
        {
            // Covering short - realize P&L on covered shares
            int closedQty = qMin(quantity, -currentQty);
            realizedPnL = closedQty * (currentAvgPrice - p_fillPrice);

            newQty = currentQty + quantity;
            if (newQty > 0)
            {
                newAvgPrice = p_fillPrice;
                posData.isLong = true; // Flipped to long
            }
            else if (newQty == 0)
            {
                newAvgPrice = 0;
                // Keep isLong = false (was short)
            }
        }

        m_balance -= quantity * p_fillPrice;
    }
    else
    {
        if (currentQty <= 0)
        {
            // Short position: average down
            double totalCost = ((-currentQty) * currentAvgPrice) + (quantity * p_fillPrice);
            newQty = currentQty - quantity;
            newAvgPrice = (newQty < 0) ? (totalCost / (-newQty)) : 0;
            posData.isLong = false; // Opening or adding to short position
        }
        else
        {
            // Selling long - realize P&L on sold shares
            int closedQty = qMin(quantity, currentQty);
            realizedPnL = closedQty * (p_fillPrice - currentAvgPrice);

            newQty = currentQty - quantity;
            if (newQty < 0)
            {
                newAvgPrice = p_fillPrice;
                posData.isLong = false; // Flipped to short
            }
            else if (newQty == 0)
            {
                newAvgPrice = 0;
                // Keep isLong = true (was long)
            }
        }

        m_balance += quantity * p_fillPrice;
    }

    // Track realized P&L per position
    if (std::abs(realizedPnL) > 0.0001)
    {
        posData.realizedPnL += realizedPnL;
        m_realizedProfitLoss += realizedPnL;
    }

    // If position will be closed (qty == 0), capture the average price and direction NOW before updating posData
    double closedAvgPrice = 0.0;
    bool wasLong = true;
    if (newQty == 0)
    {
        closedAvgPrice = posData.averagePrice; // Capture BEFORE it gets set to 0
        wasLong = posData.isLong;              // Preserve whether it was Long or Short
    }

    posData.quantity = newQty;
    posData.averagePrice = newAvgPrice;

    // If position is closed (qty == 0), store final P&L and remove from active tracking
    if (newQty == 0)
    {
        // Store the closed position's realized P&L for balance calculation
        double finalPnL = posData.realizedPnL;

        m_closedPositionPnL.insert(positionID, finalPnL);
        m_totalClosedPnL += finalPnL;

        // Create a final position update with qty=0 showing REALIZED P&L (not mark-to-market)
        QJsonObject closedJson;
        closedJson["PositionID"] = positionID;
        closedJson["AccountID"] = getSimulatedAccountID();
        closedJson["Symbol"] = symbol;
        closedJson["Quantity"] = "0";
        closedJson["AveragePrice"] = QString::number(closedAvgPrice, 'f', 4);
        closedJson["Last"] = QString::number(p_fillPrice, 'f', 4);
        closedJson["AssetType"] = "STOCK";
        closedJson["LongShort"] = wasLong ? "Long" : "Short"; // Preserve historical direction
        closedJson["Timestamp"] = MainApp::currentAppReplayTime.toString(Qt::ISODate);
        closedJson["Bid"] = QString::number(p_fillPrice, 'f', 4);
        closedJson["Ask"] = QString::number(p_fillPrice, 'f', 4);
        closedJson["ConversionRate"] = "1.0";
        closedJson["DayTradeRequirement"] = "0";
        closedJson["InitialRequirement"] = "0";
        closedJson["MaintenanceMargin"] = "0";
        closedJson["MarkToMarketPrice"] = QString::number(p_fillPrice, 'f', 4);
        closedJson["MarketValue"] = "0.00";
        closedJson["TotalCost"] = "0.00";
        // Use REALIZED P&L for the final display (matches balance P&L)
        closedJson["TodaysProfitLoss"] = QString::number(finalPnL, 'f', 2);
        closedJson["UnrealizedProfitLoss"] = QString::number(finalPnL, 'f', 2);
        closedJson["UnrealizedProfitLossPercent"] = "0";
        closedJson["UnrealizedProfitLossQty"] = "0";
        closedJson["IsUpdate"] = true;

        // Emit the final position update with realized P&L
        QByteArray closedJsonBytes = QJsonDocument(closedJson).toJson(QJsonDocument::Compact);
        emit positionUpdate(closedJsonBytes);

        m_positions.remove(positionID);
        m_positionData.remove(positionID);
        m_symbolToActivePosition.remove(symbol);

        INFO << "Position closed:" << positionID << symbol << "realized P&L:" << finalPnL;
        return;
    }

    // Create Position JSON
    QJsonObject posJson;
    posJson["PositionID"] = positionID;
    posJson["AccountID"] = getSimulatedAccountID();
    posJson["Symbol"] = symbol;
    posJson["Quantity"] = QString::number(newQty);
    posJson["AveragePrice"] = QString::number(newAvgPrice, 'f', 4);
    posJson["Last"] = QString::number(p_fillPrice, 'f', 4);
    posJson["AssetType"] = "STOCK";
    posJson["LongShort"] = (newQty > 0) ? "Long" : "Short";
    posJson["Timestamp"] = MainApp::currentAppReplayTime.toString(Qt::ISODate);

    // Calculate P&L
    double unrealizedPL = (p_fillPrice - newAvgPrice) * newQty;

    // Required fields
    posJson["Bid"] = QString::number(p_fillPrice, 'f', 4);
    posJson["Ask"] = QString::number(p_fillPrice, 'f', 4);
    posJson["ConversionRate"] = "1.0";
    posJson["DayTradeRequirement"] = "0";
    posJson["InitialRequirement"] = "0";
    posJson["MaintenanceMargin"] = "0";
    posJson["MarkToMarketPrice"] = QString::number(p_fillPrice, 'f', 4);
    posJson["MarketValue"] = QString::number(newQty * p_fillPrice, 'f', 2);
    posJson["TodaysProfitLoss"] = QString::number(unrealizedPL, 'f', 2);
    posJson["TotalCost"] = QString::number(newQty * newAvgPrice, 'f', 2);
    posJson["UnrealizedProfitLoss"] = QString::number(unrealizedPL, 'f', 2);
    posJson["UnrealizedProfitLossPercent"] = "0";
    posJson["UnrealizedProfitLossQty"] = QString::number(quantity);

    Position position(posJson);
    m_positions.insert(positionID, position);

    emit positionUpdate(positionToJson(position));

    INFO << "Position updated:" << positionID << symbol << "qty:" << newQty << "avgPrice:" << newAvgPrice;
}

void OrderEmulator::recalculatePositionPnL(const QString& p_symbol)
{
    // Ensure we have an active position for this symbol
    if (!m_symbolToActivePosition.contains(p_symbol))
    {
        return;
    }

    QString positionID = m_symbolToActivePosition[p_symbol];

    if (!m_positionData.contains(positionID))
    {
        return;
    }

    // Need either Level 2 or Level 1 data for bid/ask
    const bool hasLevel2 = m_depthSnapshots.contains(p_symbol);
    const bool hasLevel1 = m_quoteSnapshots.contains(p_symbol);
    if (!hasLevel2 && !hasLevel1)
    {
        return;
    }

    if (!m_latestBarClose.contains(p_symbol))
    {
        return;
    }

    const PositionData& data = m_positionData[positionID];
    double last = m_latestBarClose[p_symbol];

    // Get bid/ask from Level 2 (preferred) or Level 1
    double bid;
    double ask;
    if (hasLevel2)
    {
        const MarketDepthQuote& depth = m_depthSnapshots[p_symbol];
        bid = getBestBid(depth);
        ask = getBestAsk(depth);
    }
    else
    {
        const Quote& quote = m_quoteSnapshots[p_symbol];
        bid = quote.getBid();
        ask = quote.getAsk();
    }

    // TradeStation mark-to-market price calculation
    double markToMarketPrice;
    if (last >= bid && last <= ask)
    {
        markToMarketPrice = last;
    }
    else
    {
        markToMarketPrice = (std::abs(last - bid) < std::abs(last - ask)) ? bid : ask;
    }

    // Calculate P&L metrics
    double marketValue = markToMarketPrice * data.quantity;
    double costBasis = data.averagePrice * data.quantity;
    double unrealizedPnL = (markToMarketPrice - data.averagePrice) * data.quantity;
    double unrealizedPnLPercent = (std::abs(costBasis) > 0.01) ? (unrealizedPnL / costBasis) * 100.0 : 0.0;

    // Rebuild Position JSON with updated P&L
    QJsonObject posJson;
    posJson["PositionID"] = positionID;
    posJson["AccountID"] = getSimulatedAccountID();
    posJson["Symbol"] = p_symbol;
    posJson["Quantity"] = QString::number(data.quantity);
    posJson["AveragePrice"] = QString::number(data.averagePrice, 'f', 4);
    posJson["Last"] = QString::number(last, 'f', 4);
    posJson["Bid"] = QString::number(bid, 'f', 4);
    posJson["Ask"] = QString::number(ask, 'f', 4);
    posJson["LongShort"] = (data.quantity > 0) ? "Long" : "Short";
    posJson["AssetType"] = "STOCK";
    posJson["Timestamp"] = MainApp::currentAppReplayTime.toString(Qt::ISODate);
    posJson["ConversionRate"] = "1.0";
    posJson["DayTradeRequirement"] = "0";
    posJson["InitialRequirement"] = "0";
    posJson["MaintenanceMargin"] = "0";
    posJson["MarkToMarketPrice"] = QString::number(markToMarketPrice, 'f', 4);
    posJson["MarketValue"] = QString::number(marketValue, 'f', 2);
    posJson["TodaysProfitLoss"] = QString::number(unrealizedPnL, 'f', 2);
    posJson["TotalCost"] = QString::number(costBasis, 'f', 2);
    posJson["UnrealizedProfitLoss"] = QString::number(unrealizedPnL, 'f', 2);
    posJson["UnrealizedProfitLossPercent"] = QString::number(unrealizedPnLPercent, 'f', 2);
    posJson["UnrealizedProfitLossQty"] = QString::number(data.quantity);
    posJson["IsUpdate"] = true;

    // Update stored position
    Position position(posJson);
    m_positions.insert(positionID, position);

    emit positionUpdate(positionToJson(position));
}

QByteArray OrderEmulator::orderToJson(const Order& p_order) const
{
    QJsonObject obj;
    obj["OrderID"] = p_order.getOrderID();
    obj["AccountID"] = p_order.getAccountID();
    obj["Symbol"] = p_order.getSymbol();
    obj["Quantity"] = p_order.getQuantity();
    obj["TradeAction"] = p_order.getTradeAction();
    obj["OrderType"] = OrderType::toString(p_order.getOrderType().type);
    obj["Status"] = QtEnum::toString(p_order.getOrderStatus());
    obj["StatusDescription"] = p_order.getStatusDescription();
    obj["FilledPrice"] = QString::number(p_order.getFilledPrice(), 'f', 4);

    if (p_order.getLimitPrice().has_value())
    {
        obj["LimitPrice"] = QString::number(p_order.getLimitPrice().value(), 'f', 4);
    }

    obj["OpenedDateTime"] = p_order.getOpenedDateTime().toString(Qt::ISODate);
    if (p_order.getClosedDateTime().isValid())
    {
        obj["ClosedDateTime"] = p_order.getClosedDateTime().toString(Qt::ISODate);
    }

    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact) + "\n";
}

QJsonObject OrderEmulator::orderToJsonObject(const Order& p_order) const
{
    QJsonObject obj;
    obj["OrderID"] = p_order.getOrderID();
    obj["AccountID"] = p_order.getAccountID();
    obj["Symbol"] = p_order.getSymbol();
    obj["Quantity"] = p_order.getQuantity();
    obj["TradeAction"] = p_order.getTradeAction();
    obj["OrderType"] = OrderType::toString(p_order.getOrderType().type);
    obj["Status"] = QtEnum::toString(p_order.getOrderStatus());
    obj["StatusDescription"] = p_order.getStatusDescription();
    obj["FilledPrice"] = QString::number(p_order.getFilledPrice(), 'f', 4);

    if (p_order.getLimitPrice().has_value())
    {
        obj["LimitPrice"] = QString::number(p_order.getLimitPrice().value(), 'f', 4);
    }
    if (p_order.getStopPrice().has_value())
    {
        obj["StopPrice"] = QString::number(p_order.getStopPrice().value(), 'f', 4);
    }

    obj["OpenedDateTime"] = p_order.getOpenedDateTime().toString(Qt::ISODate);
    if (p_order.getClosedDateTime().isValid())
    {
        obj["ClosedDateTime"] = p_order.getClosedDateTime().toString(Qt::ISODate);
    }

    return obj;
}

QByteArray OrderEmulator::positionToJson(const Position& p_position) const
{
    QJsonObject obj;
    obj["PositionID"] = p_position.getPositionID();
    obj["AccountID"] = p_position.getAccountID();
    obj["Symbol"] = p_position.getSymbol();
    obj["Quantity"] = p_position.getQuantity();
    obj["AveragePrice"] = p_position.getAveragePrice();
    obj["Last"] = p_position.getLast();
    obj["Bid"] = p_position.getBid();
    obj["Ask"] = p_position.getAsk();
    obj["UnrealizedProfitLoss"] = p_position.getUnrealizedProfitLoss();
    obj["UnrealizedProfitLossPercent"] = p_position.getUnrealizedProfitLossPercent();
    obj["UnrealizedProfitLossQty"] = p_position.getUnrealizedProfitLossQty();
    obj["LongShort"] = p_position.getLongShort();
    obj["AssetType"] = p_position.getAssetType();
    obj["Timestamp"] = p_position.getTimestamp().toString(Qt::ISODate);
    obj["ConversionRate"] = p_position.getConversionRate();
    obj["DayTradeRequirement"] = p_position.getDayTradeRequirement();
    obj["InitialRequirement"] = p_position.getInitialRequirement();
    obj["MaintenanceMargin"] = p_position.getMaintenanceMargin();
    obj["MarkToMarketPrice"] = p_position.getMarkToMarketPrice();
    obj["MarketValue"] = p_position.getMarketValue();
    obj["TodaysProfitLoss"] = p_position.getTodaysProfitLoss();
    obj["TotalCost"] = p_position.getTotalCost();

    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact) + "\n";
}

bool OrderEmulator::validateOrder(const PlaceOrderRequest& p_request, QString& p_errorMessage) const
{
    // Check account ID
    if (p_request.getAccountID() != getSimulatedAccountID())
    {
        p_errorMessage =
            QString("Invalid account ID: %1 (expected %2)").arg(p_request.getAccountID(), getSimulatedAccountID());
        return false;
    }

    // Check quantity
    if (p_request.getQuantity() <= 0)
    {
        p_errorMessage = "Quantity must be greater than 0";
        return false;
    }

    // Check symbol
    if (p_request.getSymbol().isEmpty())
    {
        p_errorMessage = "Symbol is required";
        return false;
    }

    // Check market data exists for symbol (Level 2 or Level 1)
    const QString& symbol = p_request.getSymbol();
    const bool hasLevel2 = m_depthSnapshots.contains(symbol);
    const bool hasLevel1 = m_quoteSnapshots.contains(symbol);
    if (!hasLevel2 && !hasLevel1)
    {
        p_errorMessage = QString("No market data available for symbol: %1").arg(symbol);
        return false;
    }

    // Check balance for buy orders
    TradeAction tradeAction = p_request.getTradeAction();
    if (tradeAction == TradeAction::Buy || tradeAction == TradeAction::BuyToCover)
    {
        // Get best ask from Level 2 if available, otherwise Level 1
        double bestAsk;
        if (hasLevel2)
        {
            bestAsk = getBestAsk(m_depthSnapshots.value(symbol));
        }
        else
        {
            bestAsk = m_quoteSnapshots.value(symbol).getAsk();
        }
        double estimatedCost = p_request.getQuantity() * bestAsk;
        if (estimatedCost > m_balance)
        {
            p_errorMessage = QString("Insufficient funds: need $%1, have $%2")
                                 .arg(estimatedCost, 0, 'f', 2)
                                 .arg(m_balance, 0, 'f', 2);
            return false;
        }
    }

    // Check boxing prevention and sell validation
    if (m_symbolToActivePosition.contains(symbol))
    {
        QString positionID = m_symbolToActivePosition[symbol];
        const PositionData& pos = m_positionData[positionID];
        int currentQty = pos.quantity;

        // Long position but trying to short
        if (currentQty > 0 && tradeAction == TradeAction::SellShort)
        {
            p_errorMessage = "EC803: Boxed positions are not permitted (long position exists)";
            return false;
        }

        // Short position but trying to buy (not cover)
        if (currentQty < 0 && tradeAction == TradeAction::Buy)
        {
            p_errorMessage = "EC803: Boxed positions are not permitted (short position exists)";
            return false;
        }

        // Trying to sell more than owned (not short selling)
        if (tradeAction == TradeAction::Sell && p_request.getQuantity() > currentQty)
        {
            p_errorMessage = QString("EC401: Cannot sell %1 shares (current position: %2 shares). "
                                     "Use 'Sell Short' to open a short position.")
                                 .arg(p_request.getQuantity())
                                 .arg(currentQty);
            return false;
        }
    }
    else
    {
        // No position exists - can't sell what we don't own
        if (tradeAction == TradeAction::Sell)
        {
            p_errorMessage = QString("EC401: Cannot sell %1 shares of %2 (no position held). "
                                     "Use 'Sell Short' to open a short position.")
                                 .arg(p_request.getQuantity())
                                 .arg(symbol);
            return false;
        }
    }

    return true;
}

void OrderEmulator::onReceptionDelayElapsed()
{
    if (m_pendingOrders.isEmpty())
    {
        return;
    }

    // Process the first pending order
    PendingOrder pending = m_pendingOrders.takeFirst();

    // Create Order from request with OPN status
    QString orderID = generateOrderID();
    QJsonObject orderJson = createOrderJson(orderID, pending.request, Order::Status::OPN);
    Order order(orderJson);

    // Emit OPN status first
    emit orderStatusUpdate(orderToJson(order));

    // Determine data source: Level 2 preferred, Level 1 fallback
    const QString& symbol = order.getSymbol();
    const bool hasLevel2 = m_depthSnapshots.contains(symbol);
    const bool hasLevel1 = m_quoteSnapshots.contains(symbol);
    OBJ_ASSUME_TRUE(hasLevel2 || hasLevel1);

    bool isMarketOrder = (order.getOrderType().type == OrderType::Type::Market);
    bool canFill = false;
    double fillPrice = 0.0;

    if (hasLevel2)
    {
        // Use Level 2 (book walking for more accurate fills)
        const MarketDepthQuote& depth = m_depthSnapshots.value(symbol);
        canFill = isMarketOrder || canFillLimitOrder(order, depth);

        if (canFill)
        {
            // Market and limit orders both fill at the current market price (ask for buy,
            // bid for sell). The limit price is a ceiling/floor, not the execution price.
            fillPrice = calculateMarketOrderFillPrice(order, depth);
            DEBUG << "Order" << orderID << "using Level 2 data for" << symbol;
        }
    }
    else
    {
        // Use Level 1 (best bid/ask only)
        const Quote& quote = m_quoteSnapshots.value(symbol);
        canFill = isMarketOrder || canFillLimitOrderFromQuote(order, quote);

        if (canFill)
        {
            // Market and limit orders both fill at the current market price (ask for buy,
            // bid for sell). The limit price is a ceiling/floor, not the execution price.
            fillPrice = calculateMarketOrderFillPriceFromQuote(order, quote);
            DEBUG << "Order" << orderID << "using Level 1 data for" << symbol;
        }
    }

    if (canFill)
    {
        // Add to executing orders with delay (store JSON since Order has no default ctor)
        ExecutingOrder executing;
        executing.orderJson = orderJson;
        executing.fillPrice = fillPrice;
        m_executingOrders.append(executing);

        if (!m_executionTimer.isActive())
        {
            m_executionTimer.start(calculateExecutionDelay());
        }
    }
    else
    {
        // Limit order that cannot fill yet - track it
        m_openOrders.insert(order.getOrderID(), order);
        DEBUG << "Limit order queued, waiting for fill conditions:" << order.getOrderID();
    }

    // Schedule next pending order if any
    if (!m_pendingOrders.isEmpty())
    {
        m_receptionTimer.start(calculateReceptionDelay());
    }
}

void OrderEmulator::onExecutionDelayElapsed()
{
    if (m_executingOrders.isEmpty())
    {
        return;
    }

    // Process the first executing order
    ExecutingOrder executing = m_executingOrders.takeFirst();
    fillOrder(executing.orderJson, executing.fillPrice);

    // Schedule next executing order if any
    if (!m_executingOrders.isEmpty())
    {
        m_executionTimer.start(calculateExecutionDelay());
    }
}
