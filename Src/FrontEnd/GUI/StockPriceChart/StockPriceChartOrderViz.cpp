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
#include <algorithm>

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

namespace
{
    [[nodiscard]] QString normalizeTradeAction(const QString& p_tradeAction)
    {
        QString normalized = p_tradeAction.trimmed().toUpper();
        normalized.remove(' ');
        return normalized;
    }

    [[nodiscard]] bool isEntryTradeAction(const QString& p_tradeAction)
    {
        const QString action = normalizeTradeAction(p_tradeAction);
        return action == "BUY" || action == "SELLSHORT" || action == "BUYTOOPEN" || action == "SELLTOOPEN" ||
               action.contains("OPEN");
    }

    bool isPositionProfitable(const PositionVisualization* p_position, double p_price)
    {
        ASSUME_DIFF(p_position, nullptr);
        return p_position->isShort ? p_price < p_position->avgEntryPrice : p_price > p_position->avgEntryPrice;
    }

    void appendFilledMarkerToPosition(PositionVisualization* p_position, OrderMarker* p_marker)
    {
        ASSUME_DIFF(p_position, nullptr);
        ASSUME_DIFF(p_marker, nullptr);

        p_position->fillMarkers.append(p_marker);
        if (p_marker->isEntry)
        {
            p_position->entryMarkers.append(p_marker);
        }
        else
        {
            p_position->exitMarkers.append(p_marker);
        }
    }

    int markerNetDelta(const OrderMarker* p_marker)
    {
        ASSUME_DIFF(p_marker, nullptr);
        const int quantity = qAbs(p_marker->quantity);
        return p_marker->isEntry ? quantity : -quantity;
    }
} // namespace

double StockPriceChart::getExactIndexForTimestamp(const QDateTime& timestamp) const
{
    const double slotMs = chartSecondsPerIndexUnit(m_displayTimeFrame) * 1000.0;

    if (timestampToIndex.isEmpty())
    {
        if (!m_index0Timestamp.isValid())
        {
            return 0.0;
        }

        const int baseIndex = getIndexForTimestamp(timestamp);
        const QDateTime baseTimestamp = getTimestampForIndex(baseIndex);
        ASSUME_TRUE(baseTimestamp.isValid());

        const qint64 elapsedMs = baseTimestamp.msecsTo(timestamp);
        return baseIndex + (static_cast<double>(elapsedMs) / slotMs);
    }

    // Find the closest bar timestamp
    auto it = timestampToIndex.lowerBound(timestamp);

    // With our candle offset fix, candles are drawn with their left edge at the bar's
    // open time. Markers should be placed directly at the computed index without any
    // additional offset.

    if (it == timestampToIndex.end())
    {
        // Timestamp is after all bars, use last bar
        --it;
        int lastIndex = it.value();
        QDateTime lastBarTime = it.key();
        qint64 msDiff = lastBarTime.msecsTo(timestamp);
        return lastIndex + (msDiff / slotMs);
    }

    if (it == timestampToIndex.begin())
    {
        // Timestamp is before all bars
        int firstIndex = it.value();
        QDateTime firstBarTime = it.key();
        qint64 msDiff = timestamp.msecsTo(firstBarTime);
        return firstIndex - (msDiff / slotMs);
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
        return lowerIndex;
    }

    double fraction = static_cast<double>(elapsedMs) / static_cast<double>(totalMs);
    return lowerIndex + fraction * (upperIndex - lowerIndex);
}

void StockPriceChart::refreshOpenPositionVisualsFromLatestBar()
{
    if (m_currentOpenPosition == nullptr || m_currentOpenPosition->isClosed || m_latestBarIndex < 0)
    {
        return;
    }

    const double currentPrice = m_latestBar.getClose();
    updateOpenPositionDynamicLine(currentPrice, m_latestBarIndex);
    updateOpenPositionPLBox(currentPrice);
}

OrderMarker* StockPriceChart::createOrderMarker(const QString& orderID,
                                                double index,
                                                double price,
                                                bool isBuy,
                                                bool isEntry,
                                                bool filled)
{
    auto* marker = new OrderMarker();
    marker->orderID = orderID;
    marker->price = price;
    marker->isBuy = isBuy;
    marker->isEntry = isEntry;
    marker->state = filled ? OrderMarker::State::Filled : OrderMarker::State::Pending;

    if (filled)
    {
        // Buys point down (▼), sells point up (▲) — regardless of entry/exit.
        // BUY ▼, SELL ▲, SELLSHORT ▲, BUYTOCOVER ▼.
        auto* triangle = new QCPItemTriangle(m_customPlot, !isBuy);
        triangle->tip->setCoords(index, price);
        triangle->setPixelSize(12, 10);
        triangle->setColor(isEntry ? ORDER_VIZ_GREEN : ORDER_VIZ_RED);
        marker->markerItem = triangle;
        marker->isTriangleMarker = true;
    }
    else
    {
        auto* pendingRect = new QCPItemText(m_customPlot);
        pendingRect->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        pendingRect->position->setCoords(index, price);
        pendingRect->setText(QStringLiteral("▭"));
        pendingRect->setFont(QFont("Arial", 12, QFont::Bold));
        pendingRect->setColor(ORDER_VIZ_YELLOW);
        pendingRect->setPadding(QMargins(2, 2, 2, 2));
        pendingRect->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        marker->markerItem = pendingRect;
        marker->isTriangleMarker = false;
    }

    m_orderMarkers.insert(orderID, marker);

    const QString directionStr = isBuy ? "BUY" : "SELL";
    const QString stateStr = filled ? "Filled" : "Pending";
    const QString tooltipText = QString("%1 %2  |  price: %3").arg(stateStr, directionStr).arg(price, 0, 'f', 2);
    registerOrderMarkerTooltip(marker, tooltipText);

    return marker;
}

OrderMarker*
StockPriceChart::createBuyMarker(const QString& orderID, double index, double price, bool isEntry, bool filled)
{
    return createOrderMarker(orderID, index, price, true, isEntry, filled);
}

OrderMarker*
StockPriceChart::createSellMarker(const QString& orderID, double index, double price, bool isEntry, bool filled)
{
    return createOrderMarker(orderID, index, price, false, isEntry, filled);
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

    // Register for hover tooltip
    {
        QString tooltipText = QString("Cancelled  |  price: %1").arg(price, 0, 'f', 2);
        registerTooltip(
            textLabel,
            [textLabel]() { return textLabel->position->pixelPosition(); },
            tooltipText);
    }

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
        const QPointF coords = getOrderMarkerCoords(marker);

        unregisterTooltip(marker->markerItem);
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

        QString tooltipText = QString("Cancelled  |  price: %1").arg(coords.y(), 0, 'f', 2);
        registerOrderMarkerTooltip(marker, tooltipText);
        return;
    }

    if (newState == OrderMarker::State::Filled && !marker->isTriangleMarker)
    {
        const QPointF coords = getOrderMarkerCoords(marker);
        unregisterTooltip(marker->markerItem);
        m_customPlot->removeItem(marker->markerItem);

        auto* triangle = new QCPItemTriangle(m_customPlot, !marker->isBuy);
        triangle->tip->setCoords(coords);
        triangle->setPixelSize(12, 10);
        triangle->setColor(marker->isEntry ? ORDER_VIZ_GREEN : ORDER_VIZ_RED);
        marker->markerItem = triangle;
        marker->isTriangleMarker = true;
        return;
    }

    // Update triangle color for pending ↔ filled transitions
    if (marker->isTriangleMarker)
    {
        auto* tri = static_cast<QCPItemTriangle*>(marker->markerItem);
        const bool isPending = (newState == OrderMarker::State::Pending);
        const QColor color = isPending ? ORDER_VIZ_YELLOW : (marker->isEntry ? ORDER_VIZ_GREEN : ORDER_VIZ_RED);
        tri->setColor(color);
    }
}

void StockPriceChart::moveMarkerToCoords(OrderMarker* marker, double newIndex, double newPrice)
{
    if (!marker || !marker->markerItem)
    {
        return;
    }

    marker->price = newPrice;

    if (marker->isTriangleMarker)
    {
        auto* tri = static_cast<QCPItemTriangle*>(marker->markerItem);
        tri->tip->setCoords(newIndex, newPrice);
    }
    else
    {
        auto* txt = static_cast<QCPItemText*>(marker->markerItem);
        txt->position->setCoords(newIndex, newPrice);
    }
}

void StockPriceChart::moveMarkerToPrice(OrderMarker* marker, double newPrice)
{
    if (!marker || !marker->markerItem)
    {
        return;
    }

    const QPointF coords = getOrderMarkerCoords(marker);
    moveMarkerToCoords(marker, coords.x(), newPrice);
}

QPointF StockPriceChart::getOrderMarkerCoords(const OrderMarker* marker) const
{
    ASSUME_DIFF(marker, nullptr);
    ASSUME_DIFF(marker->markerItem, nullptr);

    if (marker->isTriangleMarker)
    {
        return static_cast<QCPItemTriangle*>(marker->markerItem)->tip->coords();
    }

    return static_cast<QCPItemText*>(marker->markerItem)->position->coords();
}

void StockPriceChart::registerOrderMarkerTooltip(OrderMarker* marker, const QString& tooltip)
{
    if (marker == nullptr || marker->markerItem == nullptr)
    {
        return;
    }

    if (marker->isTriangleMarker)
    {
        auto* triangle = static_cast<QCPItemTriangle*>(marker->markerItem);
        registerTooltip(
            marker->markerItem,
            [triangle]() { return triangle->tip->pixelPosition(); },
            tooltip);
        return;
    }

    auto* label = static_cast<QCPItemText*>(marker->markerItem);
    registerTooltip(
        marker->markerItem,
        [label]() { return label->position->pixelPosition(); },
        tooltip);
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
        unregisterTooltip(marker->markerItem);
        m_customPlot->removeItem(marker->markerItem);
    }

    delete marker;
    m_orderMarkers.erase(it);
}

double StockPriceChart::clampIndexToValidRange(double index, const QDateTime& timestamp) const
{
    OBJ_ASSUME_TRUE(m_index0Timestamp.isValid());
    OBJ_ASSUME_TRUE(timestamp.isValid());

    const QDateTime normalizedTimestamp = timestamp.toTimeZone(TradingHours::MARKET_TIMEZONE);
    OBJ_ASSUME_TRUE(normalizedTimestamp.isValid());

    const QDate date = normalizedTimestamp.date();
    const double minIndex = static_cast<double>(getIndexForTimestamp(
        QDateTime(date, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION, TradingHours::MARKET_TIMEZONE)));
    const double maxIndex =
        static_cast<double>(getIndexForTimestamp(
            QDateTime(date, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION, TradingHours::MARKET_TIMEZONE))) +
        1.0 + FUTURE_BAR_TOLERANCE;

    if (index < minIndex + FIRST_CANDLE_WINDOW)
    {
        DEBUG << "Index too far before session start (" << index << "), clamping to"
              << (minIndex + FIRST_CANDLE_WINDOW);
        return minIndex + FIRST_CANDLE_WINDOW;
    }

    if (index > maxIndex)
    {
        DEBUG << "Index too far after session end (" << index << "), clamping to" << maxIndex;
        return maxIndex;
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
    line->setVisible(m_orderVisualizationsVisible);
    return line;
}

void StockPriceChart::updateOpenPositionDynamicLine(double currentPrice, double currentIndex)
{
    if (!m_currentOpenPosition || m_currentOpenPosition->isClosed)
    {
        return;
    }

    if (m_currentOpenPosition->fillMarkers.isEmpty())
    {
        return;
    }

    OrderMarker* lastFill = m_currentOpenPosition->fillMarkers.last();
    const QPointF fillCoords = getOrderMarkerCoords(lastFill);
    const double effectiveCurrentIndex = (m_currentTimeLine != nullptr && m_currentTimeLine->visible())
                                             ? m_currentTimeLine->start->coords().x()
                                             : currentIndex;

    const bool profitable = isPositionProfitable(m_currentOpenPosition, currentPrice);
    const QColor color = profitable ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;

    for (QCPItemLine* segment: m_currentOpenPosition->traceSegments)
    {
        segment->setPen(QPen(color, ORDER_VIZ_LINE_WIDTH - 1, Qt::DotLine));
    }

    // Create or update dynamic line
    if (!m_currentOpenPosition->dynamicLine)
    {
        m_currentOpenPosition->dynamicLine =
            createPositionLine(fillCoords.x(), fillCoords.y(), effectiveCurrentIndex, currentPrice, profitable);
    }
    else
    {
        m_currentOpenPosition->dynamicLine->setPen(QPen(color, ORDER_VIZ_LINE_WIDTH - 1, Qt::DotLine));
        m_currentOpenPosition->dynamicLine->start->setCoords(fillCoords);
        m_currentOpenPosition->dynamicLine->end->setCoords(effectiveCurrentIndex, currentPrice);
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
        m_openPositionPLBox->setLayer("overlay");
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

    const QCPRange xRange = m_customPlot->xAxis->range();
    const QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
    const double ySpan = qMax(0.01, yRange.upper - yRange.lower);
    const double yPadding = qMax(0.01, ySpan * 0.02);

    double rawLabelX = m_latestBarIndex + 1.0;
    if (m_showLevel2DepthOverlay)
    {
        bool hasVisibleDepthBars = false;
        double depthRightEdgeX = 0.0;

        const auto updateDepthRightEdge = [&](const std::array<QCPItemLine*, 10>& lines)
        {
            for (QCPItemLine* line: lines)
            {
                if (line != nullptr && line->visible())
                {
                    depthRightEdgeX =
                        hasVisibleDepthBars ? qMax(depthRightEdgeX, line->end->coords().x()) : line->end->coords().x();
                    hasVisibleDepthBars = true;
                }
            }
        };

        updateDepthRightEdge(m_depthBidLines);
        updateDepthRightEdge(m_depthAskLines);
        if (hasVisibleDepthBars)
        {
            const double depthPadding = qMax(0.2, chartIndexUnitsPerBar(m_displayTimeFrame) * 0.2);
            rawLabelX = qMax(rawLabelX, depthRightEdgeX + depthPadding);
        }
    }

    double labelX = rawLabelX;
    Qt::Alignment horizontalAlignment = Qt::AlignLeft;
    if (rawLabelX >= xRange.upper - 0.2)
    {
        labelX = xRange.upper - 0.2;
        horizontalAlignment = Qt::AlignRight;
    }
    else if (rawLabelX <= xRange.lower + 0.2)
    {
        labelX = xRange.lower + 0.2;
        horizontalAlignment = Qt::AlignLeft;
    }

    double labelY = currentPrice;
    Qt::Alignment verticalAlignment = Qt::AlignVCenter;
    QString edgeSuffix;
    if (currentPrice > yRange.upper - yPadding)
    {
        labelY = yRange.upper - yPadding;
        verticalAlignment = Qt::AlignTop;
        edgeSuffix = QStringLiteral(" ↑");
    }
    else if (currentPrice < yRange.lower + yPadding)
    {
        labelY = yRange.lower + yPadding;
        verticalAlignment = Qt::AlignBottom;
        edgeSuffix = QStringLiteral(" ↓");
    }

    m_openPositionPLBox->setPositionAlignment(horizontalAlignment | verticalAlignment);
    m_openPositionPLBox->position->setCoords(labelX, labelY);

    // Format and display
    QString plText =
        QString("%1$%2%3").arg(unrealizedPL >= 0 ? "+" : "").arg(QString::number(unrealizedPL, 'f', 2)).arg(edgeSuffix);

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
    const QPointF exitCoords = getOrderMarkerCoords(lastExit);

    // Create label
    posViz->plLabel = new QCPItemText(m_customPlot);
    posViz->plLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    posViz->plLabel->position->setType(QCPItemPosition::ptPlotCoords);
    posViz->plLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    posViz->plLabel->position->setCoords(exitCoords);
    posViz->plLabel->setFont(QFont(font().family(), ORDER_VIZ_PL_FONT_SIZE));
    posViz->plLabel->setLayer("overlay");

    // Format P&L text
    QString plText =
        QString("%1$%2").arg(posViz->realizedPL >= 0 ? "+" : "").arg(QString::number(posViz->realizedPL, 'f', 2));

    QColor textColor = posViz->realizedPL >= 0 ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
    posViz->plLabel->setColor(textColor);
    posViz->plLabel->setText(plText);
    posViz->plLabel->setVisible(m_orderVisualizationsVisible);
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

void StockPriceChart::clearPositionTraceVisuals(PositionVisualization* posViz)
{
    if (!posViz)
    {
        return;
    }

    posViz->entryMarkers.clear();
    posViz->exitMarkers.clear();
    posViz->fillMarkers.clear();

    for (QCPItemLine* line: posViz->traceSegments)
    {
        m_customPlot->removeItem(line);
    }
    posViz->traceSegments.clear();

    if (posViz->dynamicLine)
    {
        m_customPlot->removeItem(posViz->dynamicLine);
        posViz->dynamicLine = nullptr;
    }

    if (posViz->plLabel)
    {
        m_customPlot->removeItem(posViz->plLabel);
        posViz->plLabel = nullptr;
    }
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

    const QColor color = posViz->realizedPL >= 0 ? ORDER_VIZ_GREEN : ORDER_VIZ_RED;
    for (QCPItemLine* line: posViz->traceSegments)
    {
        line->setPen(QPen(color, ORDER_VIZ_LINE_WIDTH - 1, Qt::DotLine));
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

void StockPriceChart::rebuildPositionTraces()
{
    m_currentOpenPosition = nullptr;

    QMap<QString, QVector<OrderMarker*>> filledMarkersByAccount;
    for (OrderMarker* marker: m_orderMarkers)
    {
        if (!marker || marker->state != OrderMarker::State::Filled || marker->symbol != m_symbol ||
            marker->accountID.isEmpty())
        {
            continue;
        }
        filledMarkersByAccount[marker->accountID].append(marker);
    }

    for (auto it = filledMarkersByAccount.begin(); it != filledMarkersByAccount.end(); ++it)
    {
        auto& markers = it.value();
        std::sort(markers.begin(),
                  markers.end(),
                  [](const OrderMarker* lhs, const OrderMarker* rhs)
                  {
                      if (lhs->timestamp != rhs->timestamp)
                      {
                          return lhs->timestamp < rhs->timestamp;
                      }
                      return lhs->orderID < rhs->orderID;
                  });
    }

    QMap<QString, QVector<PositionVisualization*>> positionsByAccount;
    for (PositionVisualization* posViz: m_positionVisualizations)
    {
        clearPositionTraceVisuals(posViz);

        if (posViz->symbol != m_symbol || posViz->accountID.isEmpty())
        {
            continue;
        }

        positionsByAccount[posViz->accountID].append(posViz);

        if (!posViz->isClosed && (m_currentOpenPosition == nullptr ||
                                  posViz->lastUpdateTimestamp > m_currentOpenPosition->lastUpdateTimestamp))
        {
            m_currentOpenPosition = posViz;
        }
    }

    for (auto it = positionsByAccount.begin(); it != positionsByAccount.end(); ++it)
    {
        auto& positions = it.value();
        std::sort(positions.begin(),
                  positions.end(),
                  [](const PositionVisualization* lhs, const PositionVisualization* rhs)
                  {
                      if (lhs->lastUpdateTimestamp != rhs->lastUpdateTimestamp)
                      {
                          return lhs->lastUpdateTimestamp < rhs->lastUpdateTimestamp;
                      }
                      return lhs->positionID < rhs->positionID;
                  });

        auto markers = filledMarkersByAccount.value(it.key());
        int markerIndex = 0;

        for (PositionVisualization* posViz: positions)
        {
            if (markerIndex >= markers.size())
            {
                break;
            }

            bool started = false;
            bool sawExit = false;
            int openShares = 0;

            while (markerIndex < markers.size())
            {
                OrderMarker* marker = markers[markerIndex];

                if (!started && posViz->isClosed && marker->timestamp > posViz->lastUpdateTimestamp)
                {
                    break;
                }

                const int delta = markerNetDelta(marker);
                if (!started && delta <= 0)
                {
                    ++markerIndex;
                    continue;
                }

                started = true;
                sawExit = sawExit || !marker->isEntry;
                openShares += delta;
                appendFilledMarkerToPosition(posViz, marker);
                ++markerIndex;

                if (!posViz->isClosed)
                {
                    continue;
                }

                const bool reachedFlat = sawExit && openShares <= 0;
                const bool passedCloseTimestamp = sawExit && markerIndex < markers.size() &&
                                                  markers[markerIndex]->timestamp > posViz->lastUpdateTimestamp;
                if (reachedFlat || passedCloseTimestamp)
                {
                    break;
                }
            }
        }
    }

    const bool haveCurrentPrice = m_latestBarIndex >= 0;
    const double currentPrice = haveCurrentPrice ? static_cast<double>(m_latestBar.getClose()) : 0.0;

    for (PositionVisualization* posViz: m_positionVisualizations)
    {
        const bool profitable = posViz->isClosed ? posViz->realizedPL >= 0
                                                 : (haveCurrentPrice ? isPositionProfitable(posViz, currentPrice)
                                                                     : posViz->realizedPL >= 0);

        for (int i = 1; i < posViz->fillMarkers.size(); ++i)
        {
            const QPointF startCoords = getOrderMarkerCoords(posViz->fillMarkers[i - 1]);
            const QPointF endCoords = getOrderMarkerCoords(posViz->fillMarkers[i]);
            auto* line = createPositionLine(startCoords.x(), startCoords.y(), endCoords.x(), endCoords.y(), profitable);
            posViz->traceSegments.append(line);
        }

        if (posViz->isClosed)
        {
            finalizeClosedPosition(posViz);
            continue;
        }

        if (posViz == m_currentOpenPosition && haveCurrentPrice)
        {
            updateOpenPositionDynamicLine(currentPrice, m_latestBarIndex);
            updateOpenPositionPLBox(currentPrice);
        }
    }

    if (m_currentOpenPosition == nullptr || !haveCurrentPrice)
    {
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
        clearPositionTraceVisuals(posViz);
        delete posViz;
    }
    m_positionVisualizations.clear();
    m_currentOpenPosition = nullptr;

    // Hide P&L box
    hideOpenPositionPLBox();

    // Remove all log markers
    for (LogMarker* lm: m_logMarkers)
    {
        if (lm->markerItem)
        {
            m_customPlot->removeItem(lm->markerItem);
        }
        delete lm;
    }
    m_logMarkers.clear();

    m_activeBracketOverlay.reset();
    resetBracketWheelMode(QStringLiteral("clear-order-visualizations"));
    clearBracketOverlayVisuals();

    // All QCPAbstractItem* in hover targets are now stale — clear them
    m_hoverTargets.clear();

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
        else if (status == Order::Status::CAN || status == Order::Status::UCN || status == Order::Status::TSC ||
                 status == Order::Status::REJ || status == Order::Status::OUT || status == Order::Status::EXP)
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
        for (QCPItemLine* line: posViz->traceSegments)
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

    if (m_bracketBand)
    {
        m_bracketBand->setVisible(m_orderVisualizationsVisible && m_activeBracketOverlay.has_value());
    }
    if (m_bracketStopLine)
    {
        m_bracketStopLine->setVisible(m_orderVisualizationsVisible && m_activeBracketOverlay.has_value());
    }
    if (m_bracketTakeLine)
    {
        m_bracketTakeLine->setVisible(m_orderVisualizationsVisible && m_activeBracketOverlay.has_value());
    }
    if (m_bracketStopLabel)
    {
        m_bracketStopLabel->setVisible(m_orderVisualizationsVisible && m_activeBracketOverlay.has_value());
    }
    if (m_bracketTakeLabel)
    {
        m_bracketTakeLabel->setVisible(m_orderVisualizationsVisible && m_activeBracketOverlay.has_value());
    }
    if (m_bracketTriggeredBadge)
    {
        m_bracketTriggeredBadge->setVisible(m_orderVisualizationsVisible && m_activeBracketOverlay.has_value() &&
                                            m_activeBracketOverlay->triggered);
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
        if (order.getStrategyLog().has_value())
        {
            OrderMarker* marker = m_orderMarkers.value(orderID);
            if (marker != nullptr && marker->markerItem != nullptr)
            {
                const bool isBuy = order.getTradeAction().toUpper().contains("BUY");
                const QString dirStr = isBuy ? "BUY" : "SELL";
                const QString tip = QString("Pending %1  |  %2 shares @ %3")
                                        .arg(dirStr)
                                        .arg(order.getQuantity().toInt())
                                        .arg(marker->price, 0, 'f', 2) +
                                    QString("\n\"%1\"").arg(order.getStrategyLog().value());
                registerOrderMarkerTooltip(marker, tip);
            }
        }
        return;
    }

    std::optional<double> limitPrice = order.getLimitPrice();
    std::optional<double> stopPrice = order.getStopPrice();

    if (!limitPrice.has_value() && !stopPrice.has_value())
    {
        return;
    }

    double price = limitPrice.has_value() ? limitPrice.value() : stopPrice.value();
    OBJ_ASSUME_GTE(price, 0.01);

    // Defer if chart has no bars yet — loadHistoricalOrders() will pick this up once bars arrive
    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    if (barCount == 0)
    {
        return;
    }

    const QDateTime orderOpenedAt = order.getOpenedDateTime();
    double index = getExactIndexForTimestamp(orderOpenedAt);
    bool isBuy = order.getTradeAction().toUpper().contains("BUY");
    bool isEntry = isEntryTradeAction(order.getTradeAction());

    index = clampIndexToValidRange(index, orderOpenedAt);

    DEBUG << "Order placed marker: symbol=" << order.getSymbol() << "ID=" << orderID << "price=" << price
          << "index=" << index;

    OrderMarker* marker;
    if (isBuy)
    {
        marker = createBuyMarker(orderID, index, price, isEntry, false);
    }
    else
    {
        marker = createSellMarker(orderID, index, price, isEntry, false);
    }

    marker->symbol = order.getSymbol();
    marker->timestamp = order.getOpenedDateTime();
    marker->quantity = order.getQuantity().toInt();
    marker->accountID = order.getAccountID();

    // Rebuild tooltip now that quantity is known
    {
        QString dirStr = isBuy ? "BUY" : "SELL";
        QString tip = QString("Pending %1  |  %2 shares @ %3").arg(dirStr).arg(marker->quantity).arg(price, 0, 'f', 2);
        if (order.getStrategyLog().has_value())
        {
            tip += QString("\n\"%1\"").arg(order.getStrategyLog().value());
        }
        registerOrderMarkerTooltip(marker, tip);
    }

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
        marker->accountID = order.getAccountID();

        QDateTime fillTimestamp = order.getClosedDateTime();
        if (!fillTimestamp.isValid())
        {
            fillTimestamp = marker->timestamp.isValid() ? marker->timestamp : order.getOpenedDateTime();
        }
        if (marker->timestamp.isValid() && fillTimestamp.isValid() && fillTimestamp < marker->timestamp)
        {
            // Never move a marker backward in time; stale broker timestamps can regress on add-on fills.
            fillTimestamp = marker->timestamp;
        }
        if (!fillTimestamp.isValid())
        {
            fillTimestamp = MainApp::getCurrentAppTime();
        }
        marker->timestamp = fillTimestamp;

        const int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
        if (barCount > 0)
        {
            const double fillIndex = clampIndexToValidRange(getExactIndexForTimestamp(fillTimestamp), fillTimestamp);
            const QPointF currentCoords = getOrderMarkerCoords(marker);
            if (qAbs(currentCoords.x() - fillIndex) > 0.001 || qAbs(marker->price - fillPrice) > 0.001)
            {
                moveMarkerToCoords(marker, fillIndex, fillPrice);
            }
        }
        else if (qAbs(marker->price - fillPrice) > 0.001)
        {
            moveMarkerToPrice(marker, fillPrice);
        }

        // Update tooltip with final fill price and strategy log
        if (marker->markerItem)
        {
            QString dirStr = marker->isBuy ? "BUY" : "SELL";
            QString tip =
                QString("Filled %1  |  %2 shares @ %3").arg(dirStr).arg(marker->quantity).arg(fillPrice, 0, 'f', 2);
            if (order.getStrategyLog().has_value())
            {
                tip += QString("\n\"%1\"").arg(order.getStrategyLog().value());
            }
            registerOrderMarkerTooltip(marker, tip);
        }
    }
    else
    {
        double price = order.getFilledPrice();
        OBJ_ASSUME_GTE(price, 0.01);

        // Defer if chart has no bars yet — loadHistoricalOrders() will pick this up once bars arrive
        int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
        if (barCount == 0)
        {
            return;
        }

        QDateTime fillTimestamp = order.getClosedDateTime();
        if (!fillTimestamp.isValid())
        {
            fillTimestamp =
                order.getOpenedDateTime().isValid() ? order.getOpenedDateTime() : MainApp::getCurrentAppTime();
        }
        double index = getExactIndexForTimestamp(fillTimestamp);
        bool isBuy = order.getTradeAction().toUpper().contains("BUY");
        bool isEntry = isEntryTradeAction(order.getTradeAction());

        index = clampIndexToValidRange(index, fillTimestamp);

        DEBUG << "Order filled marker: symbol=" << order.getSymbol() << "ID=" << orderID << "price=" << price
              << "index=" << index;

        OrderMarker* marker;
        if (isBuy)
        {
            marker = createBuyMarker(orderID, index, price, isEntry, true);
        }
        else
        {
            marker = createSellMarker(orderID, index, price, isEntry, true);
        }

        marker->symbol = order.getSymbol();
        marker->timestamp = order.getClosedDateTime();
        marker->quantity = order.getQuantity().toInt();
        marker->accountID = order.getAccountID();

        // Build full tooltip with strategy log if present
        {
            QString dirStr = isBuy ? "BUY" : "SELL";
            QString tip =
                QString("Filled %1  |  %2 shares @ %3").arg(dirStr).arg(marker->quantity).arg(price, 0, 'f', 2);
            if (order.getStrategyLog().has_value())
            {
                tip += QString("\n\"%1\"").arg(order.getStrategyLog().value());
            }
            registerOrderMarkerTooltip(marker, tip);
        }
    }

    rebuildPositionTraces();
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
        qCDebug(ChartLog) << "Ignoring position-open update for non-displayed symbol" << position.getSymbol()
                          << "(current:" << m_symbol << ")";
        return;
    }

    QString positionID = position.getPositionID();
    auto it = m_positionVisualizations.find(positionID);
    PositionVisualization* posViz = nullptr;
    if (it == m_positionVisualizations.end())
    {
        posViz = new PositionVisualization();
        posViz->positionID = positionID;
        posViz->symbol = position.getSymbol();
        m_positionVisualizations.insert(positionID, posViz);
    }
    else
    {
        posViz = it.value();
    }

    posViz->accountID = position.getAccountID();
    posViz->lastUpdateTimestamp = position.getTimestamp();
    posViz->isShort = position.getLongShort().toUpper() == "SHORT";
    posViz->currentQuantity = position.getQuantity().toInt();
    posViz->avgEntryPrice = position.getAveragePrice().toDouble();
    posViz->isClosed = false;

    rebuildPositionTraces();
    refreshOpenPositionVisualsFromLatestBar();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);

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
    posViz->accountID = position.getAccountID();
    posViz->lastUpdateTimestamp = position.getTimestamp();
    posViz->isShort = position.getLongShort().toUpper() == "SHORT";
    posViz->avgEntryPrice = position.getAveragePrice().toDouble();
    posViz->currentQuantity = newQuantity;
    posViz->isClosed = false;

    rebuildPositionTraces();
    refreshOpenPositionVisualsFromLatestBar();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);

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
        onPositionOpened(position);
        it = m_positionVisualizations.find(positionID);
    }

    PositionVisualization* posViz = it.value();

    // Calculate realized P&L from position data
    posViz->accountID = position.getAccountID();
    posViz->lastUpdateTimestamp = position.getTimestamp();
    posViz->isShort = position.getLongShort().toUpper() == "SHORT";
    posViz->avgEntryPrice = position.getAveragePrice().toDouble();
    posViz->realizedPL = position.getTodaysProfitLoss().toDouble();
    posViz->currentQuantity = 0;
    posViz->isClosed = true;

    rebuildPositionTraces();
    refreshOpenPositionVisualsFromLatestBar();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    qCDebug(ChartLog) << "Position closed:" << positionID << "P&L:" << posViz->realizedPL;
}

void StockPriceChart::setOrderVisualizationsVisible(bool visible)
{
    m_orderVisualizationsVisible = visible;
    updateOrderVisualizationsVisibility();
}

// ========== Tooltip Registration ==========

void StockPriceChart::registerTooltip(QCPAbstractItem* item, std::function<QPointF()> getPos, const QString& tooltip)
{
    // Replace existing entry for same item, or append a new one
    for (auto& target: m_hoverTargets)
    {
        if (target.item == item)
        {
            target.getPos = std::move(getPos);
            target.tooltip = tooltip;
            return;
        }
    }
    m_hoverTargets.append({item, std::move(getPos), tooltip});
}

void StockPriceChart::unregisterTooltip(QCPAbstractItem* item)
{
    m_hoverTargets.erase(std::remove_if(m_hoverTargets.begin(),
                                        m_hoverTargets.end(),
                                        [item](const HoverTarget& t) { return t.item == item; }),
                         m_hoverTargets.end());
}

// ========== Log Markers ==========

StockPriceChart::LogMarker* StockPriceChart::createLogMarker(const StrategyLogEntry& entry)
{
    const QDateTime normalizedTimestamp = entry.timestamp.toTimeZone(TradingHours::MARKET_TIMEZONE);
    double index = getExactIndexForTimestamp(normalizedTimestamp);
    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    if (barCount == 0)
    {
        return nullptr;
    }
    index = clampIndexToValidRange(index, normalizedTimestamp);

    // Fixed-size dot anchored to the X axis — Y is always a fixed pixel
    // distance above the bottom of the axis rect (see QCPItemLogDot).
    auto* dot = new QCPItemLogDot(m_customPlot, /*radius=*/4, /*bottomOffset=*/12);
    dot->center->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    dot->center->setCoords(index, 0.0); // Y coord unused — dot always draws at bottom

    static constexpr QColor LOG_MARKER_COLOR{100, 140, 255, 220}; // Blue-purple
    dot->setColor(LOG_MARKER_COLOR);

    auto* lm = new LogMarker();
    lm->dbId = entry.id;
    lm->symbol = entry.symbol;
    lm->timestamp = normalizedTimestamp;
    lm->message = entry.message;
    lm->strategyID = entry.strategyID;
    lm->markerItem = dot;
    m_logMarkers.append(lm);

    // Tooltip text
    const QString tip = QString("● Strategy Log  [%1]\n\"%2\"\nStrategy: %3")
                            .arg(normalizedTimestamp.toString("HH:mm:ss"), entry.message, entry.strategyID);
    registerTooltip(
        dot,
        [dot]() { return dot->dotPixelPosition(); },
        tip);

    return lm;
}

void StockPriceChart::loadStrategyLogMarkers()
{
    if (m_symbol.isEmpty())
    {
        return;
    }

    OrdersDatabase* db = OrdersDatabase::getInstance();
    if (!db || !db->isOpen())
    {
        return;
    }

    const QVector<StrategyLogEntry> entries = db->loadStrategyLogs(m_symbol);
    for (const StrategyLogEntry& entry: entries)
    {
        createLogMarker(entry);
    }

    if (!entries.isEmpty())
    {
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    }

    qCDebug(ChartLog) << "loadStrategyLogMarkers() - Loaded" << entries.size() << "log markers for" << m_symbol;
}

void StockPriceChart::onStrategyLogEmitted(const StrategyLogEntry& entry)
{
    if (entry.symbol != m_symbol)
    {
        return;
    }

    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    if (barCount == 0)
    {
        return;
    }

    createLogMarker(entry);
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::onStrategyStatusEmitted(const StrategyStatusEntry& entry)
{
    if (entry.symbol != m_symbol)
    {
        return;
    }

    if (entry.action == StrategyStatusEntry::Action::Clear)
    {
        m_activeStrategyStatus.reset();
        clearStrategyStatusVisual();
        return;
    }

    m_activeStrategyStatus = entry;
    updateStrategyStatusVisual();
}

void StockPriceChart::updateStrategyStatusVisual()
{
    ASSUME_DIFF(m_strategyStatusLabel, nullptr);

    if (!m_activeStrategyStatus.has_value())
    {
        clearStrategyStatusVisual();
        return;
    }

    if (m_activeStrategyStatus->message.isEmpty())
    {
        clearStrategyStatusVisual();
        return;
    }

    QString renderedMessage = m_activeStrategyStatus->message;
    renderedMessage.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    renderedMessage.replace(QChar('\r'), QChar('\n'));
    renderedMessage.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));

    m_strategyStatusLabel->setText(renderedMessage);

    if (!m_strategyStatusPanelVisible)
    {
        m_strategyStatusLabel->setVisible(false);
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    repositionStrategyStatusVisual();
    m_strategyStatusLabel->setVisible(true);
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::repositionStrategyStatusVisual()
{
    ASSUME_DIFF(m_strategyStatusLabel, nullptr);
    ASSUME_DIFF(m_customPlot, nullptr);

    if (!m_strategyStatusPanelVisible)
    {
        m_strategyStatusLabel->setVisible(false);
        return;
    }

    const QRect axisRect = m_customPlot->axisRect()->rect();
    constexpr int kMarginPx = 8;
    constexpr int kMinWidthPx = 220;
    constexpr int kMaxWidthPx = 560;

    const int x = axisRect.left() + kMarginPx;
    const int y = axisRect.top() + kMarginPx;
    const int availableWidth = qMax(0, axisRect.width() - (2 * kMarginPx));
    const int availableHeight = qMax(0, axisRect.height() - (2 * kMarginPx));
    int width = qMin(kMaxWidthPx, qMax(kMinWidthPx, availableWidth / 2));
    width = qMin(width, availableWidth);

    if (width <= 0 || availableHeight <= 0)
    {
        m_strategyStatusLabel->setVisible(false);
        return;
    }

    m_strategyStatusLabel->setFixedWidth(width);
    const int height = qMin(availableHeight, m_strategyStatusLabel->sizeHint().height());

    if (height <= 0)
    {
        m_strategyStatusLabel->setVisible(false);
        return;
    }

    m_strategyStatusLabel->setGeometry(x, y, width, height);
    m_strategyStatusLabel->raise();
}

void StockPriceChart::clearStrategyStatusVisual()
{
    ASSUME_DIFF(m_strategyStatusLabel, nullptr);
    m_strategyStatusLabel->setVisible(false);
    m_strategyStatusLabel->setText(QString());
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::clearBracketOverlayVisuals()
{
    if (m_bracketDragActive)
    {
        m_customPlot->setInteractions(m_interactionsBeforeBracketDrag);
        m_customPlot->unsetCursor();
        m_bracketDragActive = false;
        m_bracketDragTarget = BracketDragTarget::None;
    }

    auto removeItem = [this](auto*& p_item)
    {
        if (p_item == nullptr)
        {
            return;
        }

        unregisterTooltip(p_item);
        m_customPlot->removeItem(p_item);
        p_item = nullptr;
    };

    removeItem(m_bracketBand);
    removeItem(m_bracketStopLine);
    removeItem(m_bracketTakeLine);
    removeItem(m_bracketStopLabel);
    removeItem(m_bracketTakeLabel);
    removeItem(m_bracketTriggeredBadge);
    updateBracketWheelModeBadge();
}

void StockPriceChart::updateBracketOverlayVisuals()
{
    if (!m_activeBracketOverlay.has_value())
    {
        clearBracketOverlayVisuals();
        return;
    }

    const int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    if (barCount == 0)
    {
        clearBracketOverlayVisuals();
        return;
    }

    const auto& state = m_activeBracketOverlay.value();
    const bool isManualArmedPreview = state.isManualArmedPreview;

    if (m_bracketBand == nullptr)
    {
        m_bracketBand = new QCPItemRect(m_customPlot);
        m_bracketBand->topLeft->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketBand->bottomRight->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketBand->setBrush(QColor(180, 180, 210, 30));
        m_bracketBand->setPen(Qt::NoPen);
        m_bracketBand->setLayer("overlay");
    }
    if (m_bracketStopLine == nullptr)
    {
        m_bracketStopLine = new QCPItemLine(m_customPlot);
        m_bracketStopLine->start->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketStopLine->end->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketStopLine->setPen(QPen(ORDER_VIZ_RED, 1, Qt::SolidLine));
        m_bracketStopLine->setLayer("overlay");
    }
    if (m_bracketTakeLine == nullptr)
    {
        m_bracketTakeLine = new QCPItemLine(m_customPlot);
        m_bracketTakeLine->start->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketTakeLine->end->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketTakeLine->setPen(QPen(ORDER_VIZ_GREEN, 1, Qt::SolidLine));
        m_bracketTakeLine->setLayer("overlay");
    }
    if (m_bracketStopLabel == nullptr)
    {
        m_bracketStopLabel = new QCPItemText(m_customPlot);
        m_bracketStopLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketStopLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_bracketStopLabel->setFont(QFont(font().family(), 9, QFont::Bold));
        m_bracketStopLabel->setBrush(QBrush(QColor(0, 0, 0, 140)));
        m_bracketStopLabel->setPadding(QMargins(4, 1, 4, 1));
        m_bracketStopLabel->setColor(ORDER_VIZ_RED);
        m_bracketStopLabel->setLayer("overlay");
    }
    if (m_bracketTakeLabel == nullptr)
    {
        m_bracketTakeLabel = new QCPItemText(m_customPlot);
        m_bracketTakeLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketTakeLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_bracketTakeLabel->setFont(QFont(font().family(), 9, QFont::Bold));
        m_bracketTakeLabel->setBrush(QBrush(QColor(0, 0, 0, 140)));
        m_bracketTakeLabel->setPadding(QMargins(4, 1, 4, 1));
        m_bracketTakeLabel->setColor(ORDER_VIZ_GREEN);
        m_bracketTakeLabel->setLayer("overlay");
    }
    if (m_bracketTriggeredBadge == nullptr)
    {
        m_bracketTriggeredBadge = new QCPItemText(m_customPlot);
        m_bracketTriggeredBadge->position->setAxes(m_customPlot->xAxis,
                                                   m_customPlot->axisRect()->axis(QCPAxis::atRight));
        m_bracketTriggeredBadge->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_bracketTriggeredBadge->setFont(QFont(font().family(), 8, QFont::Bold));
        m_bracketTriggeredBadge->setBrush(QBrush(QColor(0, 0, 0, 160)));
        m_bracketTriggeredBadge->setPadding(QMargins(4, 1, 4, 1));
        m_bracketTriggeredBadge->setColor(QColor(255, 180, 60));
        m_bracketTriggeredBadge->setLayer("overlay");
    }

    const QDateTime armTimestamp = state.armTimestamp.toTimeZone(TradingHours::MARKET_TIMEZONE);
    double startX = clampIndexToValidRange(getExactIndexForTimestamp(armTimestamp), armTimestamp);
    const double nowX = (m_currentTimeLine != nullptr && m_currentTimeLine->visible())
                            ? m_currentTimeLine->start->coords().x()
                            : m_customPlot->xAxis->range().upper;
    const double endX = qMax(startX, nowX);

    const double lowPrice = qMin(state.stopPrice, state.takePrice);
    const double highPrice = qMax(state.stopPrice, state.takePrice);

    m_bracketBand->topLeft->setCoords(startX, highPrice);
    m_bracketBand->bottomRight->setCoords(endX, lowPrice);

    m_bracketStopLine->start->setCoords(startX, state.stopPrice);
    m_bracketStopLine->end->setCoords(endX, state.stopPrice);
    m_bracketTakeLine->start->setCoords(startX, state.takePrice);
    m_bracketTakeLine->end->setCoords(endX, state.takePrice);
    m_bracketBand->setBrush(isManualArmedPreview ? QColor(120, 140, 180, 24) : QColor(180, 180, 210, 30));
    const Qt::PenStyle lineStyle = isManualArmedPreview ? Qt::DashLine : Qt::SolidLine;
    m_bracketStopLine->setPen(
        QPen(ORDER_VIZ_RED, m_bracketDragActive && m_bracketDragTarget == BracketDragTarget::Stop ? 2 : 1, lineStyle));
    m_bracketTakeLine->setPen(QPen(ORDER_VIZ_GREEN,
                                   m_bracketDragActive && m_bracketDragTarget == BracketDragTarget::Take ? 2 : 1,
                                   lineStyle));

    const QCPRange yRange = m_customPlot->axisRect()->axis(QCPAxis::atRight)->range();
    const double ySpan = qMax(0.01, yRange.upper - yRange.lower);
    const double yPadding = qMax(0.01, ySpan * 0.02);

    enum class StickyEdge
    {
        InsideRange,
        AboveRange,
        BelowRange,
    };

    struct StickyPlacement
    {
        double y = 0.0;
        Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter;
        StickyEdge edge = StickyEdge::InsideRange;
    };

    const auto placementForPrice = [&](const double p_price) -> StickyPlacement
    {
        StickyPlacement placement;
        placement.y = p_price;
        if (p_price > yRange.upper - yPadding)
        {
            placement.y = yRange.upper - yPadding;
            placement.alignment = Qt::AlignLeft | Qt::AlignTop;
            placement.edge = StickyEdge::AboveRange;
        }
        else if (p_price < yRange.lower + yPadding)
        {
            placement.y = yRange.lower + yPadding;
            placement.alignment = Qt::AlignLeft | Qt::AlignBottom;
            placement.edge = StickyEdge::BelowRange;
        }
        return placement;
    };

    StickyPlacement stopPlacement = placementForPrice(state.stopPrice);
    StickyPlacement takePlacement = placementForPrice(state.takePrice);

    // Keep both labels legible when they collapse to the same border.
    const double minSeparation = qMax(0.02, ySpan * 0.03);
    if (qAbs(stopPlacement.y - takePlacement.y) < minSeparation)
    {
        if (stopPlacement.y >= takePlacement.y)
        {
            stopPlacement.y = qMin(yRange.upper - yPadding, stopPlacement.y + (minSeparation * 0.5));
            takePlacement.y = qMax(yRange.lower + yPadding, takePlacement.y - (minSeparation * 0.5));
        }
        else
        {
            stopPlacement.y = qMax(yRange.lower + yPadding, stopPlacement.y - (minSeparation * 0.5));
            takePlacement.y = qMin(yRange.upper - yPadding, takePlacement.y + (minSeparation * 0.5));
        }
    }

    const auto edgeLabelSuffix = [](const StickyEdge p_edge) -> QString
    {
        switch (p_edge)
        {
        case StickyEdge::AboveRange:
            return QStringLiteral(" ↑");
        case StickyEdge::BelowRange:
            return QStringLiteral(" ↓");
        case StickyEdge::InsideRange:
            return QString();
        }
        return QString();
    };

    const auto edgeTooltipSuffix = [](const StickyEdge p_edge) -> QString
    {
        switch (p_edge)
        {
        case StickyEdge::AboveRange:
            return QStringLiteral(" (above visible range)");
        case StickyEdge::BelowRange:
            return QStringLiteral(" (below visible range)");
        case StickyEdge::InsideRange:
            return QString();
        }
        return QString();
    };

    const double labelX = qMin(endX + 0.35, m_customPlot->xAxis->range().upper - 0.05);
    const QString stopPrefix = isManualArmedPreview ? QStringLiteral("ARMED STOP") : QStringLiteral("STOP");
    const QString takePrefix = isManualArmedPreview ? QStringLiteral("ARMED TAKE") : QStringLiteral("TAKE");

    std::optional<double> targetEntryPrice;
    int targetSignedQuantity = 0;
    const QString normalizedStateSymbol = state.symbol.trimmed().toUpper();
    const QString normalizedPositionSymbol =
        m_currentOpenPosition != nullptr ? m_currentOpenPosition->symbol.trimmed().toUpper() : QString();
    if (m_currentOpenPosition != nullptr && !m_currentOpenPosition->isClosed &&
        m_currentOpenPosition->currentQuantity != 0 && normalizedPositionSymbol == normalizedStateSymbol)
    {
        targetEntryPrice = m_currentOpenPosition->avgEntryPrice;
        targetSignedQuantity = m_currentOpenPosition->currentQuantity;
    }
    else if (isManualArmedPreview && state.referenceEntryPrice > 0.0 && state.previewQuantity > 0)
    {
        targetEntryPrice = state.referenceEntryPrice;
        targetSignedQuantity =
            (state.side == StrategyBracketOverlayEntry::Side::Short) ? -state.previewQuantity : state.previewQuantity;
    }

    const auto formatSignedMoney = [](const double p_value) -> QString
    { return QString("%1$%2").arg(p_value >= 0.0 ? "+" : "").arg(QString::number(p_value, 'f', 2)); };

    std::optional<double> stopTargetPL;
    std::optional<double> takeTargetPL;
    if (targetEntryPrice.has_value() && targetSignedQuantity != 0)
    {
        const double signedQty = static_cast<double>(targetSignedQuantity);
        stopTargetPL = (state.stopPrice - targetEntryPrice.value()) * signedQty;
        takeTargetPL = (state.takePrice - targetEntryPrice.value()) * signedQty;
    }

    const QString stopAmountSuffix =
        stopTargetPL.has_value() ? QStringLiteral(" (%1)").arg(formatSignedMoney(stopTargetPL.value())) : QString();
    const QString takeAmountSuffix =
        takeTargetPL.has_value() ? QStringLiteral(" (%1)").arg(formatSignedMoney(takeTargetPL.value())) : QString();
    const QString stopText = QString("%1 %2%3%4")
                                 .arg(stopPrefix)
                                 .arg(state.stopPrice, 0, 'f', 2)
                                 .arg(stopAmountSuffix)
                                 .arg(edgeLabelSuffix(stopPlacement.edge));
    const QString takeText = QString("%1 %2%3%4")
                                 .arg(takePrefix)
                                 .arg(state.takePrice, 0, 'f', 2)
                                 .arg(takeAmountSuffix)
                                 .arg(edgeLabelSuffix(takePlacement.edge));
    const QString sideText = (state.side == StrategyBracketOverlayEntry::Side::Short) ? "SHORT" : "LONG";
    const QString ownerText =
        isManualArmedPreview ? QStringLiteral("Armed Bracket") : QStringLiteral("Strategy Bracket");

    m_bracketStopLabel->setPositionAlignment(stopPlacement.alignment);
    m_bracketStopLabel->position->setCoords(labelX, stopPlacement.y);
    m_bracketStopLabel->setColor(stopTargetPL.has_value() && stopTargetPL.value() > 0.0 ? ORDER_VIZ_GREEN
                                                                                        : ORDER_VIZ_RED);
    m_bracketStopLabel->setText(stopText);
    m_bracketTakeLabel->setPositionAlignment(takePlacement.alignment);
    m_bracketTakeLabel->position->setCoords(labelX, takePlacement.y);
    m_bracketTakeLabel->setColor(ORDER_VIZ_GREEN);
    m_bracketTakeLabel->setText(takeText);

    const QString stopTipTarget = stopTargetPL.has_value()
                                      ? QStringLiteral("\nTarget P/L %1").arg(formatSignedMoney(stopTargetPL.value()))
                                      : QString();
    const QString takeTipTarget = takeTargetPL.has_value()
                                      ? QStringLiteral("\nTarget P/L %1").arg(formatSignedMoney(takeTargetPL.value()))
                                      : QString();

    const QString stopTip = QString("%1 (%2)\n%3 %4%5%6")
                                .arg(ownerText)
                                .arg(sideText)
                                .arg(stopPrefix)
                                .arg(state.stopPrice, 0, 'f', 2)
                                .arg(edgeTooltipSuffix(stopPlacement.edge))
                                .arg(stopTipTarget);
    const QString takeTip = QString("%1 (%2)\n%3 %4%5%6")
                                .arg(ownerText)
                                .arg(sideText)
                                .arg(takePrefix)
                                .arg(state.takePrice, 0, 'f', 2)
                                .arg(edgeTooltipSuffix(takePlacement.edge))
                                .arg(takeTipTarget);
    registerTooltip(
        m_bracketStopLabel,
        [this]() { return m_bracketStopLabel->position->pixelPosition(); },
        stopTip);
    registerTooltip(
        m_bracketTakeLabel,
        [this]() { return m_bracketTakeLabel->position->pixelPosition(); },
        takeTip);

    if (state.triggered && !isManualArmedPreview)
    {
        QString triggerText = QStringLiteral("TRIGGERED");
        if (!state.triggerReason.isEmpty())
        {
            triggerText += QString(" (%1)").arg(state.triggerReason);
        }

        const double rawBadgeY = (highPrice + lowPrice) / 2.0;
        const double badgeY = qBound(yRange.lower + yPadding, rawBadgeY, yRange.upper - yPadding);
        Qt::Alignment badgeAlignment = Qt::AlignLeft | Qt::AlignVCenter;
        if (rawBadgeY >= yRange.upper - yPadding)
        {
            badgeAlignment = Qt::AlignLeft | Qt::AlignTop;
        }
        else if (rawBadgeY <= yRange.lower + yPadding)
        {
            badgeAlignment = Qt::AlignLeft | Qt::AlignBottom;
        }

        m_bracketTriggeredBadge->setPositionAlignment(badgeAlignment);
        m_bracketTriggeredBadge->position->setCoords(labelX, badgeY);
        m_bracketTriggeredBadge->setText(triggerText);
        registerTooltip(
            m_bracketTriggeredBadge,
            [this]() { return m_bracketTriggeredBadge->position->pixelPosition(); },
            triggerText);
    }
    else
    {
        unregisterTooltip(m_bracketTriggeredBadge);
    }

    const bool visible = m_orderVisualizationsVisible;
    m_bracketBand->setVisible(visible);
    m_bracketStopLine->setVisible(visible);
    m_bracketTakeLine->setVisible(visible);
    m_bracketStopLabel->setVisible(visible);
    m_bracketTakeLabel->setVisible(visible);
    m_bracketTriggeredBadge->setVisible(visible && state.triggered);
    updateBracketWheelModeBadge();
}

void StockPriceChart::applyBracketOverlayEvent(const StrategyBracketOverlayEntry& p_entry, const bool p_replot)
{
    if (p_entry.action == StrategyBracketOverlayEntry::Action::Clear)
    {
        if (m_activeBracketOverlay.has_value() && m_activeBracketOverlay->isManualArmedPreview)
        {
            // Manual arming preview is locally owned by GUIFrontend and should not be cleared
            // by unrelated managed-bracket clear broadcasts.
            return;
        }
        m_activeBracketOverlay.reset();
        resetBracketWheelMode(QStringLiteral("overlay-cleared"));
        clearBracketOverlayVisuals();
        if (p_replot)
        {
            m_customPlot->replot(QCustomPlot::rpQueuedReplot);
        }
        return;
    }

    BracketOverlayState nextState;
    if (m_activeBracketOverlay.has_value() && m_activeBracketOverlay->symbol == p_entry.symbol &&
        m_activeBracketOverlay->strategyID == p_entry.strategyID)
    {
        nextState.armTimestamp = m_activeBracketOverlay->armTimestamp;
    }
    else
    {
        nextState.armTimestamp = p_entry.timestamp;
    }

    nextState.strategyID = p_entry.strategyID;
    nextState.symbol = p_entry.symbol;
    nextState.side = p_entry.side;
    nextState.stopPrice = p_entry.stopPrice;
    nextState.takePrice = p_entry.takePrice;
    nextState.referenceEntryPrice = p_entry.referenceEntryPrice;
    if (nextState.referenceEntryPrice <= 0.0 && m_activeBracketOverlay.has_value() &&
        m_activeBracketOverlay->symbol == p_entry.symbol && m_activeBracketOverlay->strategyID == p_entry.strategyID)
    {
        nextState.referenceEntryPrice = m_activeBracketOverlay->referenceEntryPrice;
    }
    nextState.triggered = p_entry.triggered;
    nextState.triggerReason = p_entry.triggerReason;
    nextState.isManualArmedPreview = false;
    m_activeBracketOverlay = nextState;
    updateBracketOverlayVisuals();

    if (p_replot)
    {
        m_customPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void StockPriceChart::loadStrategyBracketOverlays()
{
    // Managed bracket overlays are live state from MainAlgo and are re-emitted on symbol focus.
    // Historical DB replay is intentionally disabled to avoid showing stale pre-port overlay rows.
}

void StockPriceChart::onStrategyBracketOverlayEmitted(const StrategyBracketOverlayEntry& entry)
{
    if (entry.symbol != m_symbol)
    {
        return;
    }

    applyBracketOverlayEvent(entry, true);
}

void StockPriceChart::upsertManualArmedBracketOverlay(const QString& symbol,
                                                      const StrategyBracketOverlayEntry::Side side,
                                                      const double stopPrice,
                                                      const double takePrice,
                                                      const QDateTime& armTimestamp,
                                                      const double referenceEntryPrice,
                                                      const int previewQuantity)
{
    const QString normalizedSymbol = symbol.trimmed().toUpper();
    if (normalizedSymbol.isEmpty() || normalizedSymbol != m_symbol)
    {
        return;
    }

    BracketOverlayState nextState;
    nextState.strategyID = QStringLiteral("GUI-MANUAL-ARM");
    nextState.symbol = normalizedSymbol;
    if (m_activeBracketOverlay.has_value() && m_activeBracketOverlay->isManualArmedPreview &&
        m_activeBracketOverlay->symbol == normalizedSymbol)
    {
        nextState.armTimestamp = m_activeBracketOverlay->armTimestamp;
    }
    else
    {
        nextState.armTimestamp = armTimestamp.isValid() ? armTimestamp : MainApp::getCurrentAppTime();
    }
    nextState.side = side;
    nextState.stopPrice = qMax(0.01, stopPrice);
    nextState.takePrice = qMax(0.01, takePrice);
    nextState.referenceEntryPrice = qMax(0.0, referenceEntryPrice);
    nextState.previewQuantity = qMax(0, previewQuantity);
    nextState.triggered = false;
    nextState.triggerReason.clear();
    nextState.isManualArmedPreview = true;

    m_activeBracketOverlay = nextState;
    updateBracketOverlayVisuals();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void StockPriceChart::clearManualArmedBracketOverlay(const QString& symbol)
{
    if (!m_activeBracketOverlay.has_value() || !m_activeBracketOverlay->isManualArmedPreview)
    {
        return;
    }

    if (!symbol.trimmed().isEmpty() && m_activeBracketOverlay->symbol != symbol.trimmed().toUpper())
    {
        return;
    }

    m_activeBracketOverlay.reset();
    resetBracketWheelMode(QStringLiteral("manual-overlay-cleared"));
    clearBracketOverlayVisuals();
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}
