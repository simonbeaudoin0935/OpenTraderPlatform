// Order visualization functionality for StockPriceChart
// Contains: getExactIndexForTimestamp, createOrderMarker, createBuyMarker, createSellMarker,
// createCancelledMarker, updateMarkerState, moveMarkerToPrice, removeOrderMarker,
// clampIndexToValidRange, createPositionLine, updateOpenPositionDynamicLine,
// ensureOpenPositionPLBox, updateOpenPositionPLBox, hideOpenPositionPLBox,
// createClosedPositionPLLabel, calculateDCAPrice, finalizeClosedPosition,
// clearOrderVisualizations, loadHistoricalOrders, loadHistoricalPositions,
// updateOrderVisualizationsVisibility, cullOrderVisualizationsToVisibleRange,
// onOrderPlaced, onOrderFilled, onOrderCancelled, onOrderAmended,
// onPositionOpened, onPositionUpdated, onPositionClosed, setOrderVisualizationsVisible

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>

#include "StockPriceChart.h"
#include "IndexToTimeTicker.h"
#include "Misc/Settings.h"
#include "Logging.h"
#include "Assume.h"
#include "SQL/StockPriceChartQueries.h"
#include "BarCache.h"
#include "MainApp.h"
#include "Order.h"
#include "Position.h"
#include "OrdersDatabase.h"
#include "PositionsDatabase.h"
#define LOGGING_CATEGORY ChartLog

double StockPriceChart::getExactIndexForTimestamp(const QDateTime& timestamp) const
{
    if (timestampToIndex.isEmpty())
    {
        return 0.0;
    }

    // Find the closest bar timestamp
    auto it = timestampToIndex.lowerBound(timestamp);

    // With open-time bar convention, bar[i].timestamp is the LEFT edge of candle i in
    // QCPFinancial (which centers the candle at integer index i, so left edge = i - 0.5).
    // Subtract 0.5 from all returned values so that a timestamp exactly at bar[i]'s open
    // maps to chart coordinate i - 0.5, matching the white time-line's own -0.5 offset.

    if (it == timestampToIndex.end())
    {
        // Timestamp is after all bars, use last bar
        --it;
        int lastIndex = it.value();
        QDateTime lastBarTime = it.key();
        qint64 msDiff = lastBarTime.msecsTo(timestamp);
        // Assume 1-minute bars: 60000ms per index
        return lastIndex + (msDiff / 60000.0) - 0.5;
    }

    if (it == timestampToIndex.begin())
    {
        // Timestamp is before all bars
        int firstIndex = it.value();
        QDateTime firstBarTime = it.key();
        qint64 msDiff = timestamp.msecsTo(firstBarTime);
        return firstIndex - (msDiff / 60000.0) - 0.5;
    }

    // Interpolate between two bars
    QDateTime upperTime = it.key();
    int upperIndex = it.value();
    --it;
    QDateTime lowerTime = it.key();
    int lowerIndex = it.value();

    qint64 totalMs = lowerTime.msecsTo(upperTime);
    qint64 elapsedMs = lowerTime.msecsTo(timestamp);

    if (totalMs <= 0)
    {
        return lowerIndex - 0.5;
    }

    double fraction = static_cast<double>(elapsedMs) / static_cast<double>(totalMs);
    return lowerIndex + fraction * (upperIndex - lowerIndex) - 0.5;
}

OrderMarker*
StockPriceChart::createOrderMarker(const QString& orderID, double index, double price, bool isBuy, bool filled)
{
    auto* marker = new OrderMarker();
    marker->orderID = orderID;
    marker->price = price;
    marker->isBuy = isBuy;
    marker->state = filled ? OrderMarker::State::Filled : OrderMarker::State::Pending;

    auto* triangle = new QCPItemTriangle(m_customPlot, isBuy);
    triangle->tip->setCoords(index, price);
    triangle->setPixelSize(12, 10);

    const QColor color = isBuy ? (filled ? ORDER_VIZ_GREEN : ORDER_VIZ_GREEN.lighter(130))
                               : (filled ? ORDER_VIZ_RED : ORDER_VIZ_RED.lighter(130));
    triangle->setColor(color);

    marker->markerItem = triangle;
    marker->isTriangleMarker = true;
    m_orderMarkers.insert(orderID, marker);
    return marker;
}

OrderMarker* StockPriceChart::createBuyMarker(const QString& orderID, double index, double price, bool filled)
{
    return createOrderMarker(orderID, index, price, true, filled);
}

OrderMarker* StockPriceChart::createSellMarker(const QString& orderID, double index, double price, bool filled)
{
    return createOrderMarker(orderID, index, price, false, filled);
}

OrderMarker* StockPriceChart::createCancelledMarker(const QString& orderID, double index, double price)
{
    auto* marker = new OrderMarker();
    marker->orderID = orderID;
    marker->price = price;
    marker->state = OrderMarker::State::Cancelled;

    QCPAxis* yAxis = m_customPlot->axisRect()->axis(QCPAxis::atRight);
    QCPItemText* textLabel = new QCPItemText(m_customPlot);
    textLabel->position->setAxes(m_customPlot->xAxis, yAxis);
    textLabel->position->setCoords(index, price);
    textLabel->setText("✖");
    textLabel->setFont(QFont("Arial", 14, QFont::Bold));
    textLabel->setColor(ORDER_VIZ_GRAY);
    textLabel->setPadding(QMargins(4, 4, 4, 4));
    textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    marker->markerItem = textLabel;

    m_orderMarkers.insert(orderID, marker);
    return marker;
}

void StockPriceChart::updateMarkerState(OrderMarker* marker, OrderMarker::State newState)
{
    if (!marker || !marker->markerItem)
    {
        return;
    }

    marker->state = newState;

    if (newState == OrderMarker::State::Cancelled || newState == OrderMarker::State::Rejected)
    {
        // Replace triangle with a grey ✖ text label
        const QPointF coords = [&]() -> QPointF
        {
            if (marker->isTriangleMarker)
                return static_cast<QCPItemTriangle*>(marker->markerItem)->tip->coords();
            return static_cast<QCPItemText*>(marker->markerItem)->position->coords();
        }();

        m_customPlot->removeItem(marker->markerItem);
        marker->isTriangleMarker = false;

        QCPAxis* yAxis = m_customPlot->axisRect()->axis(QCPAxis::atRight);
        auto* textLabel = new QCPItemText(m_customPlot);
        textLabel->position->setAxes(m_customPlot->xAxis, yAxis);
        textLabel->position->setCoords(coords);
        textLabel->setText("✖");
        textLabel->setFont(QFont("Arial", 14, QFont::Bold));
        textLabel->setColor(ORDER_VIZ_GRAY);
        textLabel->setPadding(QMargins(4, 4, 4, 4));
        textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        marker->markerItem = textLabel;
        return;
    }

    // Update triangle color for pending ↔ filled transitions
    if (marker->isTriangleMarker)
    {
        auto* tri = static_cast<QCPItemTriangle*>(marker->markerItem);
        const bool isPending = (newState == OrderMarker::State::Pending);
        const QColor color = marker->isBuy ? (isPending ? ORDER_VIZ_GREEN.lighter(130) : ORDER_VIZ_GREEN)
                                           : (isPending ? ORDER_VIZ_RED.lighter(130) : ORDER_VIZ_RED);
        tri->setColor(color);
    }
}

void StockPriceChart::moveMarkerToPrice(OrderMarker* marker, double newPrice)
{
    if (!marker || !marker->markerItem)
    {
        return;
    }

    marker->price = newPrice;

    if (marker->isTriangleMarker)
    {
        auto* tri = static_cast<QCPItemTriangle*>(marker->markerItem);
        const QPointF coords = tri->tip->coords();
        tri->tip->setCoords(coords.x(), newPrice);
    }
    else
    {
        auto* txt = static_cast<QCPItemText*>(marker->markerItem);
        const QPointF coords = txt->position->coords();
        txt->position->setCoords(coords.x(), newPrice);
    }
}

void StockPriceChart::removeOrderMarker(const QString& orderID)
{
    auto it = m_orderMarkers.find(orderID);
    if (it == m_orderMarkers.end())
    {
        return;
    }

    OrderMarker* marker = it.value();

    // Remove marker item from plot
    if (marker->markerItem)
    {
        m_customPlot->removeItem(marker->markerItem);
    }

    delete marker;
    m_orderMarkers.erase(it);
}

double StockPriceChart::clampIndexToValidRange(double index, int barCount) const
{
    if (index < FIRST_CANDLE_WINDOW)
    {
        DEBUG << "Index too far negative (" << index << "), clamping to" << FIRST_CANDLE_WINDOW;
        return FIRST_CANDLE_WINDOW;
    }

    if (index > barCount + FUTURE_BAR_TOLERANCE)
    {
        DEBUG << "Index too far in future (" << index << "), clamping to" << (barCount - 1);
        return barCount - 1;
    }

    return index;
}

QCPItemLine* StockPriceChart::createPositionLine(double x1, double y1, double x2, double y2, bool profitable)
{
    auto* line = new QCPItemLine(m_customPlot);
    QCPAxis* yAxis = m_customPlot->axisRect()->axis(QCPAxis::atRight);
    line->start->setAxes(m_customPlot->xAxis, yAxis);
    line->end->setAxes(m_customPlot->xAxis, yAxis);
    QColor color = profitable ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
    QPen pen(color, ORDER_VIZ_LINE_WIDTH - 1, Qt::DotLine); // Thin dotted line
    line->setPen(pen);
    line->start->setCoords(x1, y1);
    line->end->setCoords(x2, y2);
    return line;
}

void StockPriceChart::updateOpenPositionDynamicLine(double currentPrice, double currentIndex)
{
    if (!m_currentOpenPosition || m_currentOpenPosition->isClosed)
    {
        return;
    }

    // Get the last entry marker position
    if (m_currentOpenPosition->entryMarkers.isEmpty())
    {
        return;
    }

    OrderMarker* lastEntry = m_currentOpenPosition->entryMarkers.last();
    double entryIndex = getExactIndexForTimestamp(lastEntry->timestamp);
    double entryPrice = lastEntry->price;

    // Calculate if position is currently profitable
    bool profitable;
    if (m_currentOpenPosition->isShort)
    {
        profitable = currentPrice < m_currentOpenPosition->avgEntryPrice;
    }
    else
    {
        profitable = currentPrice > m_currentOpenPosition->avgEntryPrice;
    }

    // Create or update dynamic line
    if (!m_currentOpenPosition->dynamicLine)
    {
        m_currentOpenPosition->dynamicLine =
            createPositionLine(entryIndex, entryPrice, currentIndex, currentPrice, profitable);
    }
    else
    {
        QColor color = profitable ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
        m_currentOpenPosition->dynamicLine->setPen(QPen(color, ORDER_VIZ_LINE_WIDTH - 1, Qt::DotLine));
        m_currentOpenPosition->dynamicLine->start->setCoords(entryIndex, entryPrice);
        m_currentOpenPosition->dynamicLine->end->setCoords(currentIndex, currentPrice);
    }
}

void StockPriceChart::ensureOpenPositionPLBox()
{
    if (!m_openPositionPLBox)
    {
        // Create text label (has its own background via setBrush)
        m_openPositionPLBox = new QCPItemText(m_customPlot);
        m_openPositionPLBox->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
        m_openPositionPLBox->position->setType(QCPItemPosition::ptPlotCoords);
        m_openPositionPLBox->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_openPositionPLBox->setFont(QFont(font().family(), ORDER_VIZ_PL_FONT_SIZE, QFont::Bold));
        m_openPositionPLBox->setPadding(QMargins(6, 4, 6, 4));
        m_openPositionPLBox->setBrush(QBrush(QColor(40, 40, 40, 190)));
    }
}

void StockPriceChart::updateOpenPositionPLBox(double currentPrice)
{
    if (!m_currentOpenPosition || m_currentOpenPosition->isClosed)
    {
        hideOpenPositionPLBox();
        return;
    }

    ensureOpenPositionPLBox();

    // Calculate unrealized P&L
    double avgEntry = m_currentOpenPosition->avgEntryPrice;
    int quantity = m_currentOpenPosition->currentQuantity;
    // Signed quantity: positive for long, negative for short.
    // (currentPrice - avgEntry) * signedQty gives the correct P&L for both directions.
    double unrealizedPL = (currentPrice - avgEntry) * quantity;

    // Position label one bar to the right of current bar, above current price line
    m_openPositionPLBox->position->setCoords(m_latestBarIndex + 1, currentPrice);

    // Format and display
    QString plText = QString("%1$%2").arg(unrealizedPL >= 0 ? "+" : "").arg(QString::number(unrealizedPL, 'f', 2));

    QColor textColor = unrealizedPL >= 0 ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
    m_openPositionPLBox->setColor(textColor);
    m_openPositionPLBox->setText(plText);
    m_openPositionPLBox->setVisible(m_orderVisualizationsVisible);
}

void StockPriceChart::hideOpenPositionPLBox()
{
    if (m_openPositionPLBox)
    {
        m_openPositionPLBox->setVisible(false);
    }
}

void StockPriceChart::createClosedPositionPLLabel(PositionVisualization* posViz)
{
    if (!posViz || posViz->exitMarkers.isEmpty())
    {
        return;
    }

    // Position the label near the last exit marker
    OrderMarker* lastExit = posViz->exitMarkers.last();
    double exitIndex = getExactIndexForTimestamp(lastExit->timestamp);
    double exitPrice = lastExit->price;

    // Create label
    posViz->plLabel = new QCPItemText(m_customPlot);
    posViz->plLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    posViz->plLabel->position->setType(QCPItemPosition::ptPlotCoords);
    posViz->plLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    posViz->plLabel->position->setCoords(exitIndex, exitPrice);
    posViz->plLabel->setFont(QFont(font().family(), ORDER_VIZ_PL_FONT_SIZE));

    // Format P&L text
    QString plText =
        QString("%1$%2").arg(posViz->realizedPL >= 0 ? "+" : "").arg(QString::number(posViz->realizedPL, 'f', 2));

    QColor textColor = posViz->realizedPL >= 0 ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
    posViz->plLabel->setColor(textColor);
    posViz->plLabel->setText(plText);
}

double StockPriceChart::calculateDCAPrice(const PositionVisualization* posViz) const
{
    if (!posViz || posViz->entryMarkers.isEmpty())
    {
        return 0.0;
    }

    double totalCost = 0.0;
    int totalQuantity = 0;

    for (const OrderMarker* marker: posViz->entryMarkers)
    {
        totalCost += marker->price * marker->quantity;
        totalQuantity += marker->quantity;
    }

    return totalQuantity > 0 ? totalCost / totalQuantity : 0.0;
}

void StockPriceChart::finalizeClosedPosition(PositionVisualization* posViz)
{
    if (!posViz)
    {
        return;
    }

    posViz->isClosed = true;

    // Remove dynamic line
    if (posViz->dynamicLine)
    {
        m_customPlot->removeItem(posViz->dynamicLine);
        posViz->dynamicLine = nullptr;
    }

    // Create connection lines from last entry to each exit
    if (!posViz->entryMarkers.isEmpty() && !posViz->exitMarkers.isEmpty())
    {
        OrderMarker* lastEntry = posViz->entryMarkers.last();
        double entryIndex = getExactIndexForTimestamp(lastEntry->timestamp);

        for (OrderMarker* exitMarker: posViz->exitMarkers)
        {
            double exitIndex = getExactIndexForTimestamp(exitMarker->timestamp);
            bool profitable = posViz->realizedPL >= 0;
            auto* line = createPositionLine(entryIndex, lastEntry->price, exitIndex, exitMarker->price, profitable);
            posViz->exitLines.append(line);
        }
    }

    // Create P&L label
    createClosedPositionPLLabel(posViz);

    // Clear current open position if this was it
    if (m_currentOpenPosition == posViz)
    {
        m_currentOpenPosition = nullptr;
        hideOpenPositionPLBox();
    }
}

void StockPriceChart::clearOrderVisualizations()
{
    DEBUG << "Clearing" << m_orderMarkers.size() << "order markers and" << m_positionVisualizations.size()
          << "position visualizations";

    // Remove all order markers
    for (auto it = m_orderMarkers.begin(); it != m_orderMarkers.end(); ++it)
    {
        OrderMarker* marker = it.value();
        if (marker->markerItem)
        {
            m_customPlot->removeItem(marker->markerItem);
        }
        delete marker;
    }
    m_orderMarkers.clear();

    // Remove all position visualizations
    for (auto it = m_positionVisualizations.begin(); it != m_positionVisualizations.end(); ++it)
    {
        PositionVisualization* posViz = it.value();

        // Remove connection lines
        for (QCPItemLine* line: posViz->entryConnectionLines)
        {
            m_customPlot->removeItem(line);
        }
        for (QCPItemLine* line: posViz->exitLines)
        {
            m_customPlot->removeItem(line);
        }
        if (posViz->dynamicLine)
        {
            m_customPlot->removeItem(posViz->dynamicLine);
        }
        if (posViz->plLabel)
        {
            m_customPlot->removeItem(posViz->plLabel);
        }

        delete posViz;
    }
    m_positionVisualizations.clear();
    m_currentOpenPosition = nullptr;

    // Hide P&L box
    hideOpenPositionPLBox();

    DEBUG << "Order visualizations cleared";
}

void StockPriceChart::loadHistoricalOrders()
{
    if (m_symbol.isEmpty())
    {
        qCDebug(ChartLog) << "loadHistoricalOrders() - No symbol set";
        return;
    }

    OrdersDatabase* ordersDb = OrdersDatabase::getInstance();
    OBJ_ASSUME_DIFF(ordersDb, nullptr);
    OBJ_ASSUME_TRUE(ordersDb->isOpen());

    QMap<QString, std::tuple<Order, std::optional<qint64>>> allOrders = ordersDb->loadAllOrders();

    // Must have bars loaded before loading historical orders
    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    if (barCount == 0)
    {
        qCDebug(ChartLog) << "loadHistoricalOrders() - No bars loaded yet, deferring";
        return;
    }

    int loadedCount = 0;
    for (auto it = allOrders.begin(); it != allOrders.end(); ++it)
    {
        const Order& order = std::get<0>(it.value());

        // Only process orders for the current symbol
        if (order.getSymbol() != m_symbol)
        {
            continue;
        }

        Order::Status status = order.getOrderStatus();

        if (status == Order::Status::OPN || status == Order::Status::ACK)
        {
            // Pending order
            onOrderPlaced(order);
            loadedCount++;
        }
        else if (status == Order::Status::FLL || status == Order::Status::FLP || status == Order::Status::FPR)
        {
            // Filled order
            onOrderFilled(order);
            loadedCount++;
        }
        else if (status == Order::Status::CAN || status == Order::Status::UCN || status == Order::Status::TSC)
        {
            // Cancelled order
            onOrderCancelled(order);
            loadedCount++;
        }
    }

    qCDebug(ChartLog) << "loadHistoricalOrders() - Loaded" << loadedCount << "orders for symbol" << m_symbol;
}

void StockPriceChart::loadHistoricalPositions()
{
    if (m_symbol.isEmpty())
    {
        qCDebug(ChartLog) << "loadHistoricalPositions() - No symbol set";
        return;
    }

    PositionsDatabase* positionsDb = PositionsDatabase::getInstance();
    if (!positionsDb || !positionsDb->isOpen())
    {
        WARNING << "loadHistoricalPositions() - PositionsDatabase not available";
        return;
    }

    QMap<QString, Position> allPositions = positionsDb->loadAllPositions();

    int loadedCount = 0;
    for (auto it = allPositions.begin(); it != allPositions.end(); ++it)
    {
        const Position& position = it.value();

        // Only process positions for the current symbol
        if (position.getSymbol() != m_symbol)
        {
            continue;
        }

        int quantity = position.getQuantity().toInt();

        if (quantity == 0)
        {
            // Closed position
            onPositionClosed(position);
        }
        else
        {
            // Open position
            onPositionUpdated(position);
        }
        loadedCount++;
    }

    qCDebug(ChartLog) << "loadHistoricalPositions() - Loaded" << loadedCount << "positions for symbol" << m_symbol;
}

void StockPriceChart::updateOrderVisualizationsVisibility()
{
    // Update visibility of all markers
    for (OrderMarker* marker: m_orderMarkers)
    {
        if (marker->markerItem)
        {
            marker->markerItem->setVisible(m_orderVisualizationsVisible);
        }
    }

    // Update visibility of all position lines
    for (PositionVisualization* posViz: m_positionVisualizations)
    {
        for (QCPItemLine* line: posViz->entryConnectionLines)
        {
            line->setVisible(m_orderVisualizationsVisible);
        }
        for (QCPItemLine* line: posViz->exitLines)
        {
            line->setVisible(m_orderVisualizationsVisible);
        }
        if (posViz->dynamicLine)
        {
            posViz->dynamicLine->setVisible(m_orderVisualizationsVisible);
        }
        if (posViz->plLabel)
        {
            posViz->plLabel->setVisible(m_orderVisualizationsVisible);
        }
    }

    // Update P&L box
    if (m_openPositionPLBox)
    {
        m_openPositionPLBox->setVisible(m_orderVisualizationsVisible && m_currentOpenPosition != nullptr);
    }

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::cullOrderVisualizationsToVisibleRange()
{
    QCPRange xRange = m_customPlot->xAxis->range();

    // Cull order markers
    for (OrderMarker* marker: m_orderMarkers)
    {
        double index = getExactIndexForTimestamp(marker->timestamp);
        bool inRange = (index >= xRange.lower - 1 && index <= xRange.upper + 1);
        bool visible = m_orderVisualizationsVisible && inRange;

        if (marker->markerItem)
        {
            marker->markerItem->setVisible(visible);
        }
    }

    // Cull position lines (more complex - check if any part of line is visible)
    for (PositionVisualization* posViz: m_positionVisualizations)
    {
        if (posViz->plLabel)
        {
            double labelX = posViz->plLabel->position->coords().x();
            posViz->plLabel->setVisible(m_orderVisualizationsVisible && labelX >= xRange.lower &&
                                        labelX <= xRange.upper);
        }
    }
}

// ========== Order Event Slots ==========

void StockPriceChart::onOrderPlaced(const Order& order)
{
    if (order.getSymbol() != m_symbol)
    {
        return;
    }

    QString orderID = order.getOrderID();
    if (m_orderMarkers.contains(orderID))
    {
        return;
    }

    std::optional<double> limitPrice = order.getLimitPrice();
    std::optional<double> stopPrice = order.getStopPrice();

    if (!limitPrice.has_value() && !stopPrice.has_value())
    {
        return;
    }

    double price = limitPrice.has_value() ? limitPrice.value() : stopPrice.value();
    OBJ_ASSUME_GT(price, 0.01);

    double index = getExactIndexForTimestamp(order.getOpenedDateTime());
    bool isBuy = order.getTradeAction().toUpper().contains("BUY");

    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    OBJ_ASSUME_GT(barCount, 0);

    index = clampIndexToValidRange(index, barCount);

    DEBUG << "Order placed marker: symbol=" << order.getSymbol() << "ID=" << orderID << "price=" << price
          << "index=" << index;

    OrderMarker* marker;
    if (isBuy)
    {
        marker = createBuyMarker(orderID, index, price, false);
    }
    else
    {
        marker = createSellMarker(orderID, index, price, false);
    }

    marker->symbol = order.getSymbol();
    marker->timestamp = order.getOpenedDateTime();
    marker->quantity = order.getQuantity().toInt();
    marker->accountID = order.getAccountID();

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onOrderFilled(const Order& order)
{
    if (order.getSymbol() != m_symbol)
    {
        return;
    }

    QString orderID = order.getOrderID();
    auto it = m_orderMarkers.find(orderID);

    if (it != m_orderMarkers.end())
    {
        OrderMarker* marker = it.value();
        updateMarkerState(marker, OrderMarker::State::Filled);

        double fillPrice = order.getFilledPrice();
        if (qAbs(marker->price - fillPrice) > 0.001)
        {
            moveMarkerToPrice(marker, fillPrice);
        }

        marker->timestamp = order.getClosedDateTime();
    }
    else
    {
        double price = order.getFilledPrice();
        OBJ_ASSUME_GT(price, 0.01);

        double index = getExactIndexForTimestamp(order.getClosedDateTime());
        bool isBuy = order.getTradeAction().toUpper().contains("BUY");

        int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
        OBJ_ASSUME_GT(barCount, 0);

        index = clampIndexToValidRange(index, barCount);

        DEBUG << "Order filled marker: symbol=" << order.getSymbol() << "ID=" << orderID << "price=" << price
              << "index=" << index;

        OrderMarker* marker;
        if (isBuy)
        {
            marker = createBuyMarker(orderID, index, price, true);
        }
        else
        {
            marker = createSellMarker(orderID, index, price, true);
        }

        marker->symbol = order.getSymbol();
        marker->timestamp = order.getClosedDateTime();
        marker->quantity = order.getQuantity().toInt();
        marker->accountID = order.getAccountID();
    }

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onOrderCancelled(const Order& order)
{
    if (order.getSymbol() != m_symbol)
    {
        return;
    }

    QString orderID = order.getOrderID();
    auto it = m_orderMarkers.find(orderID);

    if (it != m_orderMarkers.end())
    {
        // Convert existing marker to cancelled state
        OrderMarker* marker = it.value();
        updateMarkerState(marker, OrderMarker::State::Cancelled);

        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
        qCDebug(ChartLog) << "Order cancelled marker updated:" << orderID;
    }
}

void StockPriceChart::onOrderAmended(const Order& order)
{
    if (order.getSymbol() != m_symbol)
    {
        return;
    }

    QString orderID = order.getOrderID();
    auto it = m_orderMarkers.find(orderID);

    if (it != m_orderMarkers.end())
    {
        OrderMarker* marker = it.value();
        double newPrice = order.getLimitPrice().value_or(order.getStopPrice().value_or(marker->price));

        if (qAbs(marker->price - newPrice) > 0.001)
        {
            moveMarkerToPrice(marker, newPrice);
            m_customPlot->replot(QCustomPlot::rpQueuedReplot);
            qCDebug(ChartLog) << "Order amended marker moved:" << orderID << "to" << newPrice;
        }
    }
}

void StockPriceChart::onPositionOpened(const Position& position)
{
    if (position.getSymbol() != m_symbol)
    {
        Q_UNREACHABLE();
        return;
    }

    QString positionID = position.getPositionID();

    // Create new position visualization
    auto* posViz = new PositionVisualization();
    posViz->positionID = positionID;
    posViz->symbol = position.getSymbol();
    posViz->isShort = position.getLongShort().toUpper() == "SHORT";
    posViz->currentQuantity = position.getQuantity().toInt();
    posViz->avgEntryPrice = position.getAveragePrice().toDouble();

    m_positionVisualizations.insert(positionID, posViz);
    m_currentOpenPosition = posViz;

    qCDebug(ChartLog) << "Position opened:" << positionID << "qty:" << posViz->currentQuantity;
}

void StockPriceChart::onPositionUpdated(const Position& position)
{
    if (position.getSymbol() != m_symbol)
    {
        return;
    }

    QString positionID = position.getPositionID();
    auto it = m_positionVisualizations.find(positionID);

    if (it == m_positionVisualizations.end())
    {
        // Position doesn't exist yet, create it
        onPositionOpened(position);
        return;
    }

    PositionVisualization* posViz = it.value();

    // Update position data
    int newQuantity = position.getQuantity().toInt();
    posViz->avgEntryPrice = position.getAveragePrice().toDouble();
    posViz->currentQuantity = newQuantity;

    // Update dynamic line with latest bar price
    if (m_latestBarIndex >= 0)
    {
        double currentPrice = m_latestBar.getClose();
        updateOpenPositionDynamicLine(currentPrice, m_latestBarIndex);
        updateOpenPositionPLBox(currentPrice);
    }

    qCDebug(ChartLog) << "Position updated:" << positionID << "qty:" << newQuantity;
}

void StockPriceChart::onPositionClosed(const Position& position)
{
    if (position.getSymbol() != m_symbol)
    {
        return;
    }

    QString positionID = position.getPositionID();
    auto it = m_positionVisualizations.find(positionID);

    if (it == m_positionVisualizations.end())
    {
        return;
    }

    PositionVisualization* posViz = it.value();

    // Calculate realized P&L from position data
    posViz->realizedPL = position.getTodaysProfitLoss().toDouble();
    posViz->currentQuantity = 0;

    // Finalize the position visualization
    finalizeClosedPosition(posViz);

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    qCDebug(ChartLog) << "Position closed:" << positionID << "P&L:" << posViz->realizedPL;
}

void StockPriceChart::setOrderVisualizationsVisible(bool visible)
{
    m_orderVisualizationsVisible = visible;
    updateOrderVisualizationsVisibility();
}
