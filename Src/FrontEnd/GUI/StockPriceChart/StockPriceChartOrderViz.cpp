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
    if (timestampToIndex.isEmpty())
    {
        return 0.0;
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
        // Assume 1-minute bars: 60000ms per index
        return lastIndex + (msDiff / 60000.0);
    }

    if (it == timestampToIndex.begin())
    {
        // Timestamp is before all bars
        int firstIndex = it.value();
        QDateTime firstBarTime = it.key();
        qint64 msDiff = timestamp.msecsTo(firstBarTime);
        return firstIndex - (msDiff / 60000.0);
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
        registerTooltip(textLabel, [textLabel]() { return textLabel->position->pixelPosition(); }, tooltipText);
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
        registerTooltip(marker->markerItem, [triangle]() { return triangle->tip->pixelPosition(); }, tooltip);
        return;
    }

    auto* label = static_cast<QCPItemText*>(marker->markerItem);
    registerTooltip(marker->markerItem, [label]() { return label->position->pixelPosition(); }, tooltip);
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
            createPositionLine(fillCoords.x(), fillCoords.y(), currentIndex, currentPrice, profitable);
    }
    else
    {
        m_currentOpenPosition->dynamicLine->setPen(QPen(color, ORDER_VIZ_LINE_WIDTH - 1, Qt::DotLine));
        m_currentOpenPosition->dynamicLine->start->setCoords(fillCoords);
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
    const QPointF exitCoords = getOrderMarkerCoords(lastExit);

    // Create label
    posViz->plLabel = new QCPItemText(m_customPlot);
    posViz->plLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    posViz->plLabel->position->setType(QCPItemPosition::ptPlotCoords);
    posViz->plLabel->position->setAxes(m_customPlot->xAxis, m_customPlot->axisRect()->axis(QCPAxis::atRight));
    posViz->plLabel->position->setCoords(exitCoords);
    posViz->plLabel->setFont(QFont(font().family(), ORDER_VIZ_PL_FONT_SIZE));

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
    OBJ_ASSUME_GT(price, 0.01);

    // Defer if chart has no bars yet — loadHistoricalOrders() will pick this up once bars arrive
    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    if (barCount == 0)
    {
        return;
    }

    double index = getExactIndexForTimestamp(order.getOpenedDateTime());
    bool isBuy = order.getTradeAction().toUpper().contains("BUY");
    // entry: BUY (long entry) or SELLSHORT (short entry); exit: SELL (close long) or BUYTOCOVER (close short)
    const QString action = order.getTradeAction().toUpper();
    bool isEntry = (action == "BUY" || action == "SELLSHORT" || action.contains("OPEN"));

    index = clampIndexToValidRange(index, barCount);

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
        marker->timestamp = order.getClosedDateTime();

        const int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
        if (barCount > 0)
        {
            const double fillIndex =
                clampIndexToValidRange(getExactIndexForTimestamp(order.getClosedDateTime()), barCount);
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
        OBJ_ASSUME_GT(price, 0.01);

        // Defer if chart has no bars yet — loadHistoricalOrders() will pick this up once bars arrive
        int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
        if (barCount == 0)
        {
            return;
        }

        double index = getExactIndexForTimestamp(order.getClosedDateTime());
        bool isBuy = order.getTradeAction().toUpper().contains("BUY");
        const QString action = order.getTradeAction().toUpper();
        bool isEntry = (action == "BUY" || action == "SELLSHORT" || action.contains("OPEN"));

        index = clampIndexToValidRange(index, barCount);

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
        Q_UNREACHABLE();
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
    double index = getExactIndexForTimestamp(entry.timestamp);
    int barCount = m_candlesticks ? m_candlesticks->data()->size() : 0;
    if (barCount == 0)
    {
        return nullptr;
    }
    index = clampIndexToValidRange(index, barCount);

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
    lm->timestamp = entry.timestamp;
    lm->message = entry.message;
    lm->strategyID = entry.strategyID;
    lm->markerItem = dot;
    m_logMarkers.append(lm);

    // Tooltip text
    const QString tip = QString("● Strategy Log  [%1]\n\"%2\"\nStrategy: %3")
                            .arg(entry.timestamp.toString("HH:mm:ss"), entry.message, entry.strategyID);
    registerTooltip(dot, [dot]() { return dot->dotPixelPosition(); }, tip);

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
