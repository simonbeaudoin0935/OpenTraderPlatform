#include "OrderEmulator.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#include "Assume.h"
#include "Logging.h"
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

OrderEmulator::OrderEmulator(QObject* p_parent)
    : QObject(p_parent)
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
          << "type:" << OrderType::toString(p_request.getOrderType().type)
          << "requestID:" << p_requestID;

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
    cancelJson["ClosedDateTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);

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
            double fillPrice = order.getLimitPrice().value_or(0.0);

            // Convert order to JSON for executing queue
            QJsonObject orderJson;
            orderJson["OrderID"] = order.getOrderID();
            orderJson["AccountID"] = order.getAccountID();
            orderJson["Symbol"] = order.getSymbol();
            orderJson["Quantity"] = order.getQuantity();
            orderJson["TradeAction"] = order.getTradeAction();
            if (order.getLimitPrice().has_value())
            {
                orderJson["LimitPrice"] = QString::number(order.getLimitPrice().value(), 'f', 4);
            }

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

    // Update position P&L if we have a position for this symbol
    if (m_positions.contains(p_symbol))
    {
        recalculatePositionPnL(p_symbol);
    }
}

void OrderEmulator::updateBarClose(const QString& p_symbol, double p_close)
{
    m_latestBarClose.insert(p_symbol, p_close);
}

QVector<Order> OrderEmulator::getOrders() const
{
    QVector<Order> result;
    result.reserve(m_openOrders.size() + m_filledOrders.size());

    for (const Order& order : m_openOrders)
    {
        result.append(order);
    }
    for (const Order& order : m_filledOrders)
    {
        result.append(order);
    }

    return result;
}

QVector<Position> OrderEmulator::getPositions() const
{
    QVector<Position> result;
    result.reserve(m_positions.size());

    for (const Position& position : m_positions)
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
    m_depthSnapshots.clear();
    m_nextOrderID = 1000;
    m_balance = 100000.0;

    INFO << "OrderEmulator cleared";
}

void OrderEmulator::pause()
{
    // Store remaining time for pending timers
    if (m_receptionTimer.isActive())
    {
        for (PendingOrder& pending : m_pendingOrders)
        {
            pending.remainingDelayMs = m_receptionTimer.remainingTime();
        }
        m_receptionTimer.stop();
    }

    if (m_executionTimer.isActive())
    {
        for (ExecutingOrder& executing : m_executingOrders)
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

    int baseDelay =
        QRandomGenerator::global()->bounded(MIN_RECEPTION_DELAY_MS, MAX_RECEPTION_DELAY_MS + 1);

    // Scale by speed: at 200% speed, delays are halved
    return (baseDelay * 100) / qMax(1, m_replaySpeedPercent);
}

int OrderEmulator::calculateExecutionDelay() const
{
    if (m_replaySpeedPercent == -1) // As fast as possible
    {
        return 0;
    }

    int baseDelay =
        QRandomGenerator::global()->bounded(MIN_EXECUTION_DELAY_MS, MAX_EXECUTION_DELAY_MS + 1);

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
    if (tradeAction == "BUY" || tradeAction == "BUYTOCOVER" || tradeAction == "Buy" ||
        tradeAction == "Buy to Cover")
    {
        double bestAsk = getBestAsk(p_depth);
        return bestAsk > 0 && bestAsk <= limitPrice;
    }

    // SELL limit: fills if bid >= limit price
    if (tradeAction == "SELL" || tradeAction == "SELLSHORT" || tradeAction == "Sell" ||
        tradeAction == "Sell Short")
    {
        double bestBid = getBestBid(p_depth);
        return bestBid > 0 && bestBid >= limitPrice;
    }

    return false;
}

double OrderEmulator::calculateMarketOrderFillPrice(const Order& p_order,
                                                     const MarketDepthQuote& p_depth) const
{
    QString tradeAction = p_order.getTradeAction();

    // For now, simplified: use best bid/ask
    // TODO: Implement full book walking algorithm
    if (tradeAction == "BUY" || tradeAction == "BUYTOCOVER" || tradeAction == "Buy" ||
        tradeAction == "Buy to Cover")
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
    QJsonObject json;
    json["OrderID"] = p_orderID;
    json["AccountID"] = p_request.getAccountID();
    json["Symbol"] = p_request.getSymbol();
    json["Quantity"] = QString::number(p_request.getQuantity());
    json["TradeAction"] = tradeActionToString(p_request.getTradeAction());
    json["Status"] = QtEnum::toString(p_status);
    json["StatusDescription"] = Order::getStatusDescriptionForStatus(p_status);
    
    json["OpenedDateTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    json["FilledPrice"] = "0.00";  // Default for non-filled orders
    json["CommissionFee"] = "0.00";
    json["ConversionRate"] = "1.00";
    json["Currency"] = "USD";
    json["Duration"] = "DAY";
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
    fillJson["ClosedDateTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);

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

    // Track position internally
    auto it = m_positionData.find(symbol);
    if (it == m_positionData.end())
    {
        PositionData newPos;
        newPos.symbol = symbol;
        newPos.quantity = 0;
        newPos.averagePrice = 0;
        m_positionData.insert(symbol, newPos);
        it = m_positionData.find(symbol);
    }

    PositionData& posData = it.value();
    int currentQty = posData.quantity;
    double currentAvgPrice = posData.averagePrice;

    // Calculate new position
    int newQty = currentQty;
    double newAvgPrice = currentAvgPrice;
    double realizedPnL = 0.0; // Track realized P&L from this trade

    bool isBuy = (tradeAction == "BUY" || tradeAction == "BUYTOCOVER" ||
                  tradeAction == "Buy" || tradeAction == "Buy to Cover");

    if (isBuy)
    {
        // Adding shares
        if (currentQty >= 0)
        {
            // Long position: average up
            double totalCost = (currentQty * currentAvgPrice) + (quantity * p_fillPrice);
            newQty = currentQty + quantity;
            newAvgPrice = (newQty > 0) ? (totalCost / newQty) : 0;
        }
        else
        {
            // Covering short - realize P&L on covered shares
            int closedQty = qMin(quantity, -currentQty); // How many shares are closing
            realizedPnL = closedQty * (currentAvgPrice - p_fillPrice); // Short profit = avgPrice - fillPrice
            
            newQty = currentQty + quantity;
            if (newQty > 0)
            {
                // Position flipped to long
                newAvgPrice = p_fillPrice;
            }
            else if (newQty == 0)
            {
                newAvgPrice = 0; // Position closed
            }
        }

        // Update balance
        m_balance -= quantity * p_fillPrice;
    }
    else
    {
        // Removing shares
        if (currentQty <= 0)
        {
            // Short position: average down
            double totalCost = ((-currentQty) * currentAvgPrice) + (quantity * p_fillPrice);
            newQty = currentQty - quantity;
            newAvgPrice = (newQty < 0) ? (totalCost / (-newQty)) : 0;
        }
        else
        {
            // Selling long - realize P&L on sold shares
            int closedQty = qMin(quantity, currentQty); // How many shares are closing
            realizedPnL = closedQty * (p_fillPrice - currentAvgPrice); // Long profit = fillPrice - avgPrice
            
            newQty = currentQty - quantity;
            if (newQty < 0)
            {
                // Position flipped to short
                newAvgPrice = p_fillPrice;
            }
            else if (newQty == 0)
            {
                newAvgPrice = 0; // Position closed
            }
        }

        // Update balance
        m_balance += quantity * p_fillPrice;
    }

    // Track realized P&L
    if (std::abs(realizedPnL) > 0.0001) // Use epsilon for float comparison
    {
        m_realizedProfitLoss += realizedPnL;
        DEBUG << "Realized P&L:" << realizedPnL << "Total realized:" << m_realizedProfitLoss;
    }

    posData.quantity = newQty;
    posData.averagePrice = newAvgPrice;

    // If position is closed (qty == 0), remove it from tracking
    if (newQty == 0)
    {
        m_positions.remove(symbol);
        m_positionData.remove(symbol);
        DEBUG << "Position closed for" << symbol;
        return; // Don't emit position update for closed position
    }

    // Create Position JSON
    QJsonObject posJson;
    posJson["PositionID"] = QString("POS-%1").arg(symbol);
    posJson["AccountID"] = getSimulatedAccountID();
    posJson["Symbol"] = symbol;
    posJson["Quantity"] = QString::number(newQty);
    posJson["AveragePrice"] = QString::number(newAvgPrice, 'f', 4);
    posJson["Last"] = QString::number(p_fillPrice, 'f', 4);
    posJson["AssetType"] = "STOCK";
    posJson["LongShort"] = (newQty > 0) ? "Long" : (newQty < 0 ? "Short" : "");
    posJson["Timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Calculate P&L
    double unrealizedPL = (p_fillPrice - newAvgPrice) * newQty;
    posJson["UnrealizedPL"] = QString::number(unrealizedPL, 'f', 2);

    // Required fields with defaults
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
    m_positions.insert(symbol, position);

    emit positionUpdate(positionToJson(position));

    DEBUG << "Position updated:" << symbol << "qty:" << newQty << "avgPrice:" << newAvgPrice;
}

void OrderEmulator::recalculatePositionPnL(const QString& p_symbol)
{
    // Ensure we have all necessary data
    if (!m_positionData.contains(p_symbol))
    {
        return;
    }

    if (!m_depthSnapshots.contains(p_symbol))
    {
        return; // No market depth available yet
    }

    if (!m_latestBarClose.contains(p_symbol))
    {
        return; // No bar close price available yet
    }

    const PositionData& data = m_positionData[p_symbol];
    const MarketDepthQuote& depth = m_depthSnapshots[p_symbol];
    double last = m_latestBarClose[p_symbol];

    // Get bid/ask from depth using helper functions
    double bid = getBestBid(depth);
    double ask = getBestAsk(depth);

    // TradeStation mark-to-market price calculation:
    // Use Last if within bid/ask spread, otherwise closest bid/ask
    double markToMarketPrice;
    if (last >= bid && last <= ask)
    {
        markToMarketPrice = last;
    }
    else
    {
        // Use whichever is closer to Last
        markToMarketPrice = (std::abs(last - bid) < std::abs(last - ask)) ? bid : ask;
    }

    // Calculate P&L metrics
    double marketValue = markToMarketPrice * data.quantity;
    double costBasis = data.averagePrice * data.quantity;
    double unrealizedPnL = (markToMarketPrice - data.averagePrice) * data.quantity;
    double unrealizedPnLPercent = (unrealizedPnL / costBasis) * 100.0;

    // Rebuild Position JSON with updated P&L
    QJsonObject posJson;
    posJson["PositionID"] = QString("POS-%1").arg(p_symbol);
    posJson["AccountID"] = "SIM123456";
    posJson["Symbol"] = p_symbol;
    posJson["Quantity"] = QString::number(data.quantity);
    posJson["AveragePrice"] = QString::number(data.averagePrice, 'f', 4);
    posJson["Last"] = QString::number(last, 'f', 4);
    posJson["Bid"] = QString::number(bid, 'f', 4);
    posJson["Ask"] = QString::number(ask, 'f', 4);
    posJson["LongShort"] = (data.quantity > 0) ? "Long" : "Short";
    posJson["AssetType"] = "STOCK";
    posJson["Timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
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
    posJson["IsUpdate"] = true; // Mark as update (not initial creation)

    // Update stored position
    Position position(posJson);
    m_positions.insert(p_symbol, position);

    // Emit updated position
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
        p_errorMessage = QString("Invalid account ID: %1 (expected %2)")
                             .arg(p_request.getAccountID(), getSimulatedAccountID());
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

    // Check market depth exists for symbol
    if (!m_depthSnapshots.contains(p_request.getSymbol()))
    {
        p_errorMessage = QString("No market data available for symbol: %1").arg(p_request.getSymbol());
        return false;
    }

    // Check balance for buy orders
    TradeAction tradeAction = p_request.getTradeAction();
    if (tradeAction == TradeAction::Buy || tradeAction == TradeAction::BuyToCover)
    {
        const MarketDepthQuote& depth = m_depthSnapshots.value(p_request.getSymbol());
        double estimatedCost = p_request.getQuantity() * getBestAsk(depth);
        if (estimatedCost > m_balance)
        {
            p_errorMessage = QString("Insufficient funds: need $%1, have $%2")
                                 .arg(estimatedCost, 0, 'f', 2)
                                 .arg(m_balance, 0, 'f', 2);
            return false;
        }
    }

    // Check boxing prevention
    auto posIt = m_positionData.find(p_request.getSymbol());
    if (posIt != m_positionData.end())
    {
        const PositionData& pos = posIt.value();
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

    // Check if we can fill immediately
    const MarketDepthQuote& depth = m_depthSnapshots.value(order.getSymbol());
    bool isMarketOrder = (order.getOrderType().type == OrderType::Type::Market);
    bool canFill = isMarketOrder || canFillLimitOrder(order, depth);

    if (canFill)
    {
        double fillPrice;
        if (isMarketOrder)
        {
            fillPrice = calculateMarketOrderFillPrice(order, depth);
        }
        else
        {
            fillPrice = order.getLimitPrice().value_or(getBestAsk(depth));
        }

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
