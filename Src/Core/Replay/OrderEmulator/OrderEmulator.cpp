#include "OrderEmulator.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#include "Assume.h"
#include "Logging.h"
#include "MainApp.h"
#include "Level2.h"

#define LOGGING_CATEGORY OrderEmulatorLog
Q_LOGGING_CATEGORY(OrderEmulatorLog, "OrderEmulator");

namespace
{
    [[nodiscard]] bool canFillMarketOrderInCurrentSession(const Order& p_order)
    {
        return p_order.getOrderType().type == OrderType::Type::Market &&
               MainApp::getCurrentSession() == TradingSession::Regular;
    }

    [[nodiscard]] QString normalizedTradeAction(const QString& p_tradeAction)
    {
        return p_tradeAction.trimmed().toUpper();
    }

    [[nodiscard]] bool isBuySideTradeAction(const QString& p_tradeAction)
    {
        const QString action = normalizedTradeAction(p_tradeAction);
        return action == "BUY" || action == "BUYTOCOVER" || action == "BUYTOOPEN" || action == "BUYTOCLOSE";
    }

    [[nodiscard]] bool isSellSideTradeAction(const QString& p_tradeAction)
    {
        const QString action = normalizedTradeAction(p_tradeAction);
        return action == "SELL" || action == "SELLSHORT" || action == "SELLTOOPEN" || action == "SELLTOCLOSE";
    }

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
    double getBestBid(const Level2& p_depth)
    {
        return p_depth.m_bids[0].m_price;
    }

    // Get best ask price from market depth
    double getBestAsk(const Level2& p_depth)
    {
        return p_depth.m_asks[0].m_price;
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

QString OrderEmulator::placeOrder(const PlaceOrderRequest& p_request)
{
    const std::optional<QString> route = p_request.getRoute();
    OBJ_ASSUME_TRUE(route.has_value());
    OBJ_ASSUME_EQUAL(route.value(), QStringLiteral("replay"));

    // Generate the canonical order ID now — this is the single ID used everywhere
    // (in the POST response to the caller AND in all subsequent orderStatusUpdate signals).
    // This mirrors how real TradeStation works: the POST response contains the OrderID
    // that StreamOrders will also use for fills/cancels/updates.
    QString orderID = generateOrderID();

    DEBUG << "placeOrder called:" << p_request.getSymbol() << "qty:" << p_request.getQuantity()
          << "type:" << OrderType::toString(p_request.getOrderType().type) << "orderID:" << orderID;

    // Validate order
    QString errorMessage;
    if (!validateOrder(p_request, errorMessage))
    {
        WARNING << "Order validation failed:" << errorMessage;

        QJsonObject rejectJson = createOrderJson(orderID, p_request, Order::Status::REJ);
        rejectJson["StatusDescription"] = errorMessage;

        Order rejectedOrder(rejectJson);
        m_filledOrders.insert(rejectedOrder.getOrderID(), rejectedOrder);
        emit orderStatusUpdate(orderToJson(rejectedOrder));
        return orderID;
    }

    // Add to pending orders
    PendingOrder pending;
    pending.request = p_request;
    pending.orderID = orderID;
    pending.submitTimeMs = QDateTime::currentMSecsSinceEpoch();
    m_pendingOrders.append(pending);

    // Start timer if not already running
    if (!m_receptionTimer.isActive() && !m_pendingOrders.isEmpty())
    {
        m_receptionTimer.start(calculateReceptionDelay());
    }

    DEBUG << "Order queued, pending count:" << m_pendingOrders.size();
    return orderID;
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

    // Preserve the original order type/prices (Limit/Stop/etc.) and only mutate status fields.
    QJsonObject cancelJson = orderToJsonObject(order);
    cancelJson["Status"] = "CAN";
    cancelJson["StatusDescription"] = Order::getStatusDescriptionForStatus(Order::Status::CAN);
    cancelJson["FilledPrice"] = QString::number(0.0, 'f', 4);
    cancelJson["ClosedDateTime"] = MainApp::currentAppReplayTime.toString(Qt::ISODate);

    Order cancelledOrder(cancelJson);

    // Move to filled orders
    m_filledOrders.insert(p_orderID, cancelledOrder);
    m_openOrders.remove(p_orderID);

    emit orderStatusUpdate(orderToJson(cancelledOrder));
    INFO << "Order cancelled:" << p_orderID;
}

void OrderEmulator::updateMarketDepth(const QString& p_symbol, const Level2& p_depth)
{
    m_depthSnapshots.insert(p_symbol, p_depth);

    // Check if any open orders can now fill. Limit orders can fill in any
    // tradable session, but market orders should remain open until regular hours.
    for (auto it = m_openOrders.begin(); it != m_openOrders.end();)
    {
        Order& order = it.value();
        if (order.getSymbol() != p_symbol)
        {
            ++it;
            continue;
        }

        const bool canFillNow = canFillMarketOrderInCurrentSession(order) || canFillLimitOrder(order, p_depth) ||
                                canFillStopOrder(order, p_depth);
        if (canFillNow)
        {
            // Fill at market price: a buy limit crossing the ask fills at the ask,
            // not the (higher) limit price. Same logic as a market order.
            double fillPrice = calculateMarketOrderFillPrice(order, p_depth);

            // Convert order to JSON for executing queue - copy all fields
            QJsonObject orderJson = orderToJsonObject(order);

            // Log BEFORE erase — order reference becomes dangling after erase
            DEBUG << "Open order can now fill:" << order.getOrderID() << "at" << fillPrice;

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

    // Recalculate unrealized P&L whenever we have a fresh bar close price.
    // Level2 updates also trigger this, but bar-close events are more frequent
    // and guarantee m_latestBarClose is populated before the P&L check runs.
    if (m_symbolToActivePosition.contains(p_symbol))
        recalculatePositionPnL(p_symbol);
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

bool OrderEmulator::canFillLimitOrder(const Order& p_order, const Level2& p_depth) const
{
    const OrderType::Type orderType = p_order.getOrderType().type;
    if (orderType != OrderType::Type::Limit && orderType != OrderType::Type::StopLimit)
    {
        return false;
    }

    if (!p_order.getLimitPrice().has_value())
    {
        return false;
    }

    const double limitPrice = p_order.getLimitPrice().value();
    const QString tradeAction = p_order.getTradeAction();

    // BUY-side limit: fills if ask <= limit price
    if (isBuySideTradeAction(tradeAction))
    {
        const double bestAsk = getBestAsk(p_depth);
        return bestAsk > 0 && bestAsk <= limitPrice;
    }

    // SELL-side limit: fills if bid >= limit price
    if (isSellSideTradeAction(tradeAction))
    {
        const double bestBid = getBestBid(p_depth);
        return bestBid > 0 && bestBid >= limitPrice;
    }

    return false;
}

bool OrderEmulator::canFillStopOrder(const Order& p_order, const Level2& p_depth) const
{
    const OrderType::Type orderType = p_order.getOrderType().type;
    if (orderType != OrderType::Type::StopMarket && orderType != OrderType::Type::StopLimit)
    {
        return false;
    }

    if (!p_order.getStopPrice().has_value())
    {
        return false;
    }

    const double stopPrice = p_order.getStopPrice().value();
    if (stopPrice <= 0.0)
    {
        return false;
    }

    const QString tradeAction = p_order.getTradeAction();
    const bool buySide = isBuySideTradeAction(tradeAction);
    const bool sellSide = isSellSideTradeAction(tradeAction);
    if (!buySide && !sellSide)
    {
        return false;
    }

    const double bestAsk = getBestAsk(p_depth);
    const double bestBid = getBestBid(p_depth);

    // Buy-side stop triggers when ask reaches or rises above stop.
    // Sell-side stop triggers when bid reaches or falls below stop.
    const bool stopTriggered =
        buySide ? (bestAsk > 0.0 && bestAsk >= stopPrice) : (bestBid > 0.0 && bestBid <= stopPrice);
    if (!stopTriggered)
    {
        return false;
    }

    if (orderType == OrderType::Type::StopMarket)
    {
        return true;
    }

    // Stop-limit orders only fill once stop has triggered AND the limit is currently marketable.
    return canFillLimitOrder(p_order, p_depth);
}

double OrderEmulator::calculateMarketOrderFillPrice(const Order& p_order, const Level2& p_depth) const
{
    const QString tradeAction = p_order.getTradeAction();

    // For now, simplified: use best bid/ask
    // TODO: Implement full book walking algorithm
    if (isBuySideTradeAction(tradeAction))
    {
        return getBestAsk(p_depth);
    }

    // SELL
    return getBestBid(p_depth);
}


QJsonObject OrderEmulator::createOrderJson(const QString& p_orderID,
                                           const PlaceOrderRequest& p_request,
                                           Order::Status p_status) const
{
    const QString routing = p_request.getRoute().value_or(QStringLiteral("replay")).trimmed();

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
    json["Routing"] = routing;
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
        DEBUG << "recalculatePnL: no active position for" << p_symbol;
        return;
    }

    QString positionID = m_symbolToActivePosition[p_symbol];

    if (!m_positionData.contains(positionID))
    {
        DEBUG << "recalculatePnL: no positionData for" << positionID;
        return;
    }

    // Need Level 2 data for bid/ask
    if (!m_depthSnapshots.contains(p_symbol))
    {
        DEBUG << "recalculatePnL: no depth snapshot for" << p_symbol;
        return;
    }

    if (!m_latestBarClose.contains(p_symbol))
    {
        DEBUG << "recalculatePnL: no barClose for" << p_symbol;
        return;
    }

    // Throttle: emit at most once per PNL_THROTTLE_MS to avoid flooding the
    // position pipeline with thousands of updates per second during fast replay.
    // NOTE: throttle is checked AFTER data guards so a failed-data attempt doesn't
    // block the next real attempt for 100 ms.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastPnLEmit.contains(p_symbol) && now - m_lastPnLEmit[p_symbol] < PNL_THROTTLE_MS)
        return;
    m_lastPnLEmit.insert(p_symbol, now);

    const PositionData& data = m_positionData[positionID];
    double last = m_latestBarClose[p_symbol];

    // Get bid/ask from Level 2
    double bid;
    double ask;
    const Level2& depth = m_depthSnapshots[p_symbol];
    bid = getBestBid(depth);
    ask = getBestAsk(depth);

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
    obj["Routing"] = p_order.m_routing;
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
    obj["Routing"] = p_order.m_routing;
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
    const std::optional<QString> route = p_request.getRoute();
    if (!route.has_value() || route.value().trimmed().compare(QStringLiteral("replay"), Qt::CaseInsensitive) != 0)
    {
        p_errorMessage = "Replay order route must be 'replay'.";
        return false;
    }

    const QString symbol = p_request.getSymbol();
    const TradeAction tradeAction = p_request.getTradeAction();

    auto actionFromOrderText = [](QString p_actionText) -> QString { return p_actionText.trimmed().toUpper(); };

    auto reservesLongShares = [](const TradeAction p_action) -> bool
    { return p_action == TradeAction::Sell || p_action == TradeAction::SellToClose; };

    auto reservesShortShares = [](const TradeAction p_action) -> bool
    { return p_action == TradeAction::BuyToCover || p_action == TradeAction::BuyToClose; };

    auto reservesLongSharesText = [&actionFromOrderText](const QString& p_actionText) -> bool
    {
        const QString action = actionFromOrderText(p_actionText);
        return action == "SELL" || action == "SELLTOCLOSE";
    };

    auto reservesShortSharesText = [&actionFromOrderText](const QString& p_actionText) -> bool
    {
        const QString action = actionFromOrderText(p_actionText);
        return action == "BUYTOCOVER" || action == "BUYTOCLOSE";
    };

    auto actionConsumesBuyingPower = [](const TradeAction p_action) -> bool
    {
        return p_action == TradeAction::Buy || p_action == TradeAction::BuyToCover ||
               p_action == TradeAction::BuyToOpen || p_action == TradeAction::BuyToClose ||
               p_action == TradeAction::SellShort || p_action == TradeAction::SellToOpen;
    };

    auto actionConsumesBuyingPowerText = [&actionFromOrderText](const QString& p_actionText) -> std::optional<bool>
    {
        const QString action = actionFromOrderText(p_actionText);
        if (action == "BUY" || action == "BUYTOCOVER" || action == "BUYTOOPEN" || action == "BUYTOCLOSE")
        {
            return true;
        }
        if (action == "SELLSHORT" || action == "SELLTOOPEN")
        {
            return false;
        }
        return std::nullopt;
    };

    auto estimateReferencePrice = [this](const QString& p_symbol,
                                         const std::optional<double>& p_limitPrice,
                                         const bool p_buySide) -> std::optional<double>
    {
        if (m_depthSnapshots.contains(p_symbol))
        {
            const Level2& depth = m_depthSnapshots.value(p_symbol);
            const double top = p_buySide ? getBestAsk(depth) : getBestBid(depth);
            if (top > 0.0)
            {
                return top;
            }
        }

        if (p_limitPrice.has_value() && p_limitPrice.value() > 0.0)
        {
            return p_limitPrice.value();
        }

        if (m_latestBarClose.contains(p_symbol))
        {
            const double close = m_latestBarClose.value(p_symbol);
            if (close > 0.0)
            {
                return close;
            }
        }

        return std::nullopt;
    };

    int reservedLongShares = 0;
    int reservedShortShares = 0;
    double reservedBuyingPower = 0.0;

    auto reserveFromRequest = [&](const PlaceOrderRequest& p_orderReq)
    {
        if (p_orderReq.getSymbol() != symbol)
        {
            return;
        }

        const int qty = p_orderReq.getQuantity();
        if (qty <= 0)
        {
            return;
        }

        const TradeAction action = p_orderReq.getTradeAction();
        if (reservesLongShares(action))
        {
            reservedLongShares += qty;
        }
        if (reservesShortShares(action))
        {
            reservedShortShares += qty;
        }
    };

    auto reserveFromOrder = [&](const Order& p_order)
    {
        if (p_order.getSymbol() != symbol)
        {
            return;
        }

        bool ok = false;
        const int qty = p_order.getQuantity().toInt(&ok);
        if (!ok || qty <= 0)
        {
            return;
        }

        if (reservesLongSharesText(p_order.getTradeAction()))
        {
            reservedLongShares += qty;
        }
        if (reservesShortSharesText(p_order.getTradeAction()))
        {
            reservedShortShares += qty;
        }
    };

    auto addBuyingPowerReservation = [&](const QString& p_orderSymbol,
                                         const int p_quantity,
                                         const std::optional<double>& p_limitPrice,
                                         const std::optional<bool>& p_consumesWithBuyReference)
    {
        if (!p_consumesWithBuyReference.has_value() || p_quantity <= 0)
        {
            return;
        }

        const auto referencePrice =
            estimateReferencePrice(p_orderSymbol, p_limitPrice, p_consumesWithBuyReference.value());
        if (!referencePrice.has_value())
        {
            return;
        }

        reservedBuyingPower += (referencePrice.value() * static_cast<double>(p_quantity));
    };

    auto reserveBuyingPowerFromRequest = [&](const PlaceOrderRequest& p_orderReq)
    {
        const TradeAction action = p_orderReq.getTradeAction();
        if (!actionConsumesBuyingPower(action))
        {
            return;
        }

        const int qty = p_orderReq.getQuantity();
        if (qty <= 0)
        {
            return;
        }

        const bool buyReference = action == TradeAction::Buy || action == TradeAction::BuyToCover ||
                                  action == TradeAction::BuyToOpen || action == TradeAction::BuyToClose;
        addBuyingPowerReservation(p_orderReq.getSymbol(), qty, p_orderReq.getLimitPrice(), buyReference);
    };

    auto reserveBuyingPowerFromOrder = [&](const Order& p_order)
    {
        bool ok = false;
        const int qty = p_order.getQuantity().toInt(&ok);
        if (!ok || qty <= 0)
        {
            return;
        }

        addBuyingPowerReservation(p_order.getSymbol(),
                                  qty,
                                  p_order.getLimitPrice(),
                                  actionConsumesBuyingPowerText(p_order.getTradeAction()));
    };

    auto reserveFromExecutingOrderJson = [&](const QJsonObject& p_json)
    {
        const QString orderSymbol = p_json.value("Symbol").toString();
        bool ok = false;
        const int qty = p_json.value("Quantity").toString().toInt(&ok);
        if (!ok || qty <= 0)
        {
            return;
        }

        const QString actionText = p_json.value("TradeAction").toString();
        if (orderSymbol == symbol)
        {
            if (reservesLongSharesText(actionText))
            {
                reservedLongShares += qty;
            }
            if (reservesShortSharesText(actionText))
            {
                reservedShortShares += qty;
            }
        }

        const std::optional<double> limitPrice =
            p_json.contains("LimitPrice") ? std::optional<double>(p_json.value("LimitPrice").toString().toDouble())
                                          : std::nullopt;
        addBuyingPowerReservation(orderSymbol, qty, limitPrice, actionConsumesBuyingPowerText(actionText));
    };

    for (const PendingOrder& pending: m_pendingOrders)
    {
        reserveFromRequest(pending.request);
        reserveBuyingPowerFromRequest(pending.request);
    }
    for (auto it = m_openOrders.cbegin(); it != m_openOrders.cend(); ++it)
    {
        reserveFromOrder(it.value());
        reserveBuyingPowerFromOrder(it.value());
    }
    for (const ExecutingOrder& executing: m_executingOrders)
    {
        reserveFromExecutingOrderJson(executing.orderJson);
    }

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
    if (symbol.isEmpty())
    {
        p_errorMessage = "Symbol is required";
        return false;
    }

    const OrderType::Type orderType = p_request.getOrderType().type;
    if ((orderType == OrderType::Type::Limit || orderType == OrderType::Type::StopLimit) &&
        (!p_request.getLimitPrice().has_value() || p_request.getLimitPrice().value() <= 0.0))
    {
        p_errorMessage = "Limit and stop-limit orders require a valid limit price";
        return false;
    }
    if ((orderType == OrderType::Type::StopMarket || orderType == OrderType::Type::StopLimit) &&
        (!p_request.getStopPrice().has_value() || p_request.getStopPrice().value() <= 0.0))
    {
        p_errorMessage = "Stop orders require a valid stop price";
        return false;
    }

    // Check market data exists for symbol (Level 2)
    const bool hasLevel2 = m_depthSnapshots.contains(symbol);
    if (!hasLevel2)
    {
        p_errorMessage = QString("No market data available for symbol: %1").arg(symbol);
        return false;
    }

    // Check account-level buying power including outstanding pending/open/executing orders.
    if (actionConsumesBuyingPower(tradeAction))
    {
        const bool buyReference = tradeAction == TradeAction::Buy || tradeAction == TradeAction::BuyToCover ||
                                  tradeAction == TradeAction::BuyToOpen || tradeAction == TradeAction::BuyToClose;
        const auto requestReferencePrice = estimateReferencePrice(symbol, p_request.getLimitPrice(), buyReference);
        if (!requestReferencePrice.has_value())
        {
            p_errorMessage = QString("Cannot estimate buying power impact for symbol %1").arg(symbol);
            return false;
        }

        const double estimatedCost = p_request.getQuantity() * requestReferencePrice.value();
        const double requiredBuyingPower = reservedBuyingPower + estimatedCost;
        if (requiredBuyingPower > m_balance)
        {
            p_errorMessage = QString("Insufficient buying power: need $%1 total ($%2 already reserved by "
                                     "pending/open orders + $%3 new), have $%4")
                                 .arg(requiredBuyingPower, 0, 'f', 2)
                                 .arg(reservedBuyingPower, 0, 'f', 2)
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
        const int currentQty = pos.quantity;
        const int availableLongForSell = currentQty > 0 ? qMax(0, currentQty - reservedLongShares) : 0;
        const int availableShortForCover = currentQty < 0 ? qMax(0, qAbs(currentQty) - reservedShortShares) : 0;

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

        // BuyToCover but position is long (not short)
        if (currentQty >= 0 && tradeAction == TradeAction::BuyToCover)
        {
            p_errorMessage = QString("EC602: You are short %1 shares!").arg(qAbs(currentQty));
            return false;
        }

        // BuyToCover more shares than the short position
        if (currentQty < 0 && tradeAction == TradeAction::BuyToCover && p_request.getQuantity() > qAbs(currentQty))
        {
            p_errorMessage = QString("EC602: You are short %1 shares (cannot cover %2)")
                                 .arg(qAbs(currentQty))
                                 .arg(p_request.getQuantity());
            return false;
        }

        // Outstanding buy-to-cover orders reserve short shares.
        if (currentQty < 0 && tradeAction == TradeAction::BuyToCover &&
            p_request.getQuantity() > availableShortForCover)
        {
            p_errorMessage = QString("EC602: Cannot cover %1 shares (%2 already reserved by pending/open orders, "
                                     "%3 currently available to cover)")
                                 .arg(p_request.getQuantity())
                                 .arg(reservedShortShares)
                                 .arg(availableShortForCover);
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

        // Outstanding sell orders reserve long shares.
        if (currentQty > 0 && tradeAction == TradeAction::Sell && p_request.getQuantity() > availableLongForSell)
        {
            p_errorMessage = QString("EC401: Cannot sell %1 shares (%2 already reserved by pending/open orders, "
                                     "%3 currently available to sell). Use 'Sell Short' to open a short position.")
                                 .arg(p_request.getQuantity())
                                 .arg(reservedLongShares)
                                 .arg(availableLongForSell);
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

        // No position exists - can't BuyToCover when not short
        if (tradeAction == TradeAction::BuyToCover)
        {
            p_errorMessage = QString("EC602: You are short 0.00 shares!");
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

    // Use the order ID that was assigned when placeOrder() was called (pending.orderID).
    // This is the same ID returned to the caller in the POST response, so the strategy,
    // the emulator, and StreamOrders all refer to the same canonical ID.
    QString orderID = pending.orderID;
    QJsonObject orderJson = createOrderJson(orderID, pending.request, Order::Status::OPN);
    Order order(orderJson);

    // Emit OPN status first
    emit orderStatusUpdate(orderToJson(order));

    const QString& symbol = order.getSymbol();
    const bool hasLevel2 = m_depthSnapshots.contains(symbol);
    OBJ_ASSUME_TRUE(hasLevel2);

    const bool isMarketOrder = (order.getOrderType().type == OrderType::Type::Market);
    bool canFill = false;
    double fillPrice = 0.0;

    const Level2& depth = m_depthSnapshots.value(symbol);
    canFill =
        canFillMarketOrderInCurrentSession(order) || canFillLimitOrder(order, depth) || canFillStopOrder(order, depth);

    if (canFill)
    {
        fillPrice = calculateMarketOrderFillPrice(order, depth);
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
        // Market orders submitted outside regular hours stay open and fill on the
        // first regular-session depth update, matching live TradeStation behavior.
        m_openOrders.insert(order.getOrderID(), order);
        if (isMarketOrder)
        {
            DEBUG << "Market order queued, waiting for regular session:" << order.getOrderID();
        }
        else if (order.getOrderType().type == OrderType::Type::StopMarket ||
                 order.getOrderType().type == OrderType::Type::StopLimit)
        {
            DEBUG << "Stop order queued, waiting for trigger/fill conditions:" << order.getOrderID();
        }
        else
        {
            DEBUG << "Limit order queued, waiting for fill conditions:" << order.getOrderID();
        }
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
    Order order(executing.orderJson);
    const QString symbol = order.getSymbol();

    // Resolve fill price at execution time (not when the order first became marketable)
    // so fast-moving replays use the latest top-of-book for price improvement.
    if (m_depthSnapshots.contains(symbol))
    {
        const Level2& depth = m_depthSnapshots.value(symbol);
        const bool canStillFill = canFillMarketOrderInCurrentSession(order) || canFillLimitOrder(order, depth) ||
                                  canFillStopOrder(order, depth);
        if (canStillFill)
        {
            const double executionFillPrice = calculateMarketOrderFillPrice(order, depth);
            fillOrder(executing.orderJson, executionFillPrice);
        }
        else
        {
            // Limit order slipped out of marketability before execution latency elapsed.
            m_openOrders.insert(order.getOrderID(), order);
            DEBUG << "Order became non-marketable before execution and remains open:" << order.getOrderID();
        }
    }
    else
    {
        // Preserve old behavior as a last-resort fallback when depth snapshots are unavailable.
        fillOrder(executing.orderJson, executing.fillPrice);
    }

    // Schedule next executing order if any
    if (!m_executingOrders.isEmpty())
    {
        m_executionTimer.start(calculateExecutionDelay());
    }
}
