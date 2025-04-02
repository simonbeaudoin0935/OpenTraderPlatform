#include "StockPriceChart.h"
#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QVBoxLayout>
#include <QGraphicsTextItem>
#include <QFont>

StockPriceChart::StockPriceChart(QWidget* parent)
    : QWidget(parent) {
    lineSeries = new QLineSeries();
    lastPriceLine = new QLineSeries();
    lastPriceLine->setPen(QPen(Qt::green, 1, Qt::DashLine)); // Start with green, dashed line
    
    candlestickSeries = new QCandlestickSeries();
    candlestickSeries->setIncreasingColor(QColor(Qt::green));
    candlestickSeries->setDecreasingColor(QColor(Qt::red));
    candlestickSeries->setBodyWidth(0.9); // Make bars wider (90% of available space)

    chart = new QChart();
    chart->addSeries(lineSeries);
    chart->addSeries(candlestickSeries);
    chart->addSeries(lastPriceLine);

    // Add margins to ensure price label is visible
    chart->setMargins(QMargins(5, 5, 50, 5));  // Left, Top, Right, Bottom
    
    chartView = new QChartView(chart, this);
    chartView->setRenderHint(QPainter::Antialiasing);

    // Create price label as a child of the chart view's scene
    priceLabel = chartView->scene()->addText("");
    priceLabel->setDefaultTextColor(Qt::white);
    QFont font = priceLabel->font();
    font.setBold(true);
    priceLabel->setFont(font);
    
    setSymbol("");

    axisX = new QDateTimeAxis();
    // We want labels only at 5-minute marks
    axisX->setFormat("hh:mm"); // Show only hours and minutes
    axisX->setTitleText("Time");
    axisX->setGridLineVisible(true);
    axisX->setMinorGridLineVisible(false);
    axisX->setLabelsAngle(-45); // Angle the time labels for better readability

    // Calculate number of ticks for 30-minute view
    // We want a tick every 5 minutes, so for 30 minutes we need 7 ticks (0,5,10,15,20,25,30)
    axisX->setTickCount(7);

    chart->addAxis(axisX, Qt::AlignBottom);
    lineSeries->attachAxis(axisX);
    candlestickSeries->attachAxis(axisX);
    lastPriceLine->attachAxis(axisX);

    axisY = new QValueAxis();
    axisY->setLabelFormat("%.2f");
    axisY->setTitleText("Price");
    chart->addAxis(axisY, Qt::AlignLeft);
    lineSeries->attachAxis(axisY);
    candlestickSeries->attachAxis(axisY);
    lastPriceLine->attachAxis(axisY);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);  // Remove widget margins
    layout->addWidget(chartView);
    setLayout(layout);
}

StockPriceChart::~StockPriceChart() {
    // No need to delete chart, series, etc.—handled by Qt parent hierarchy
}

void StockPriceChart::setSymbol(const QString& symbol) {
    this->symbol = symbol;
    lineSeries->setName(symbol);
    candlestickSeries->setName(symbol + " (Bars)");
    chart->setTitle("Stock Price: " + symbol);
}

void StockPriceChart::addPrice(double price, const QDateTime& timestamp) {
    lineSeries->append(timestamp.toMSecsSinceEpoch(), price);

    // Auto-adjust Y-axis range
    qreal minY = axisY->min();
    qreal maxY = axisY->max();
    if (price < minY || lineSeries->count() == 1) axisY->setMin(price - 1.0);
    if (price > maxY) axisY->setMax(price + 1.0);

    // Limit X-axis to last 60 points
    if (lineSeries->count() > 60) {
        lineSeries->remove(0);
    }
    axisX->setRange(QDateTime::fromMSecsSinceEpoch(lineSeries->at(0).x()),
                    QDateTime::fromMSecsSinceEpoch(lineSeries->at(lineSeries->count() - 1).x()));
}

void StockPriceChart::addBar(const Bar& bar) {
    if (!bar.isValid()) {
        return;
    }

    if (bar.getBarStatus() == "Closed") {
        handleClosedBar(bar);
    } else if (bar.getBarStatus() == "Open") {
        handleOpenBar(bar);
    }

    updateChart();
}

void StockPriceChart::handleClosedBar(const Bar& bar) {
    // If we have an open bar and it's not the same timestamp as the closed bar,
    // add it to completed bars
    if (hasOpenBar) {
        QDateTime openBarTime = QDateTime::fromString(currentOpenBar.getTimeStamp(), Qt::ISODate);
        QDateTime closedBarTime = QDateTime::fromString(bar.getTimeStamp(), Qt::ISODate);
        
        if (openBarTime != closedBarTime) {
            completedBars.append(currentOpenBar);
        }
        hasOpenBar = false;
    }

    // Add the closed bar to completed bars
    completedBars.append(bar);

    // Maintain maximum number of bars
    while (completedBars.size() > MAX_BARS) {
        completedBars.removeFirst();
    }
}

void StockPriceChart::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updatePriceLabelPosition();
}

void StockPriceChart::updatePriceLabelPosition() {
    if (!lastPriceLine->points().isEmpty() && lastPriceLine->points().size() >= 2) {
        QDateTime endTime = QDateTime::fromMSecsSinceEpoch(lastPriceLine->points().last().x());
        double price = lastPriceLine->points().last().y();
        
        // Get the price point in view coordinates
        QPointF pricePoint(endTime.toMSecsSinceEpoch(), price);
        QPointF viewPoint = chartView->mapToScene(
            chartView->mapFromParent(
                chart->mapToPosition(pricePoint, lastPriceLine).toPoint()
            )
        );
        
        // Calculate position in view coordinates
        QRectF viewRect = chartView->sceneRect();
        qreal labelX = viewRect.right() - priceLabel->boundingRect().width() - 15;
        qreal labelY = viewPoint.y() - (priceLabel->boundingRect().height() / 2);
        
        // Keep label within view bounds
        labelY = qMax(labelY, viewRect.top() + 10);
        labelY = qMin(labelY, viewRect.bottom() - priceLabel->boundingRect().height() - 10);
        
        priceLabel->setPos(labelX, labelY);
    }
}

void StockPriceChart::updateLastPriceLine(double price, bool isUpTick) {
    lastPriceLine->clear();
    
    // Get the current visible range
    QDateTime startTime = axisX->min();
    QDateTime endTime = axisX->max();
    
    // Create two points for the horizontal line spanning the visible range
    lastPriceLine->append(startTime.toMSecsSinceEpoch(), price);
    lastPriceLine->append(endTime.toMSecsSinceEpoch(), price);
    
    // Update the line color based on price movement
    QColor lineColor = isUpTick ? Qt::green : Qt::red;
    lastPriceLine->setPen(QPen(lineColor, 1, Qt::DashLine));
    
    // Update price label
    priceLabel->setPlainText(QString::number(price, 'f', 2));
    priceLabel->setDefaultTextColor(lineColor);
    
    // Update the label position
    updatePriceLabelPosition();
    
    lastPrice = price;
}

void StockPriceChart::handleOpenBar(const Bar& bar) {
    QDateTime newBarTime = QDateTime::fromString(bar.getTimeStamp(), Qt::ISODate);
    
    if (!hasOpenBar) {
        // This is the first open bar for this time period
        currentOpenBar = bar;
        hasOpenBar = true;
        updateLastPriceLine(bar.getClose().toDouble(), 
                          bar.getClose().toDouble() >= lastPrice);
    } else {
        QDateTime currentBarTime = QDateTime::fromString(currentOpenBar.getTimeStamp(), Qt::ISODate);
        
        if (newBarTime != currentBarTime) {
            // This is a new bar period - move current to completed
            completedBars.append(currentOpenBar);
            currentOpenBar = bar;
        } else {
            // Update the existing bar
            currentOpenBar = bar;
        }
        
        double newPrice = bar.getClose().toDouble();
        updateLastPriceLine(newPrice, newPrice >= currentOpenBar.getOpen().toDouble());
    }
}

void StockPriceChart::updateChart() {
    // Clear existing candlesticks
    candlestickSeries->clear();

    // Add completed bars
    for (const Bar& bar : completedBars) {
        QDateTime timestamp = QDateTime::fromString(bar.getTimeStamp(), Qt::ISODate);
        auto set = new QCandlestickSet();
        set->setTimestamp(timestamp.toMSecsSinceEpoch());
        set->setOpen(bar.getOpen().toDouble());
        set->setHigh(bar.getHigh().toDouble());
        set->setLow(bar.getLow().toDouble());
        set->setClose(bar.getClose().toDouble());
        candlestickSeries->append(set);
    }

    // Add current open bar if it exists
    if (hasOpenBar) {
        QDateTime timestamp = QDateTime::fromString(currentOpenBar.getTimeStamp(), Qt::ISODate);
        auto set = new QCandlestickSet();
        set->setTimestamp(timestamp.toMSecsSinceEpoch());
        set->setOpen(currentOpenBar.getOpen().toDouble());
        set->setHigh(currentOpenBar.getHigh().toDouble());
        set->setLow(currentOpenBar.getLow().toDouble());
        set->setClose(currentOpenBar.getClose().toDouble());
        candlestickSeries->append(set);

        // Update the last price line
        double closePrice = currentOpenBar.getClose().toDouble();
        updateLastPriceLine(closePrice, closePrice >= currentOpenBar.getOpen().toDouble());
    }

    // Update time axis range
    if (candlestickSeries->count() > 0) {
        QDateTime currentBarTime;
        if (hasOpenBar) {
            currentBarTime = QDateTime::fromString(currentOpenBar.getTimeStamp(), Qt::ISODate);
        } else {
            currentBarTime = QDateTime::fromString(completedBars.last().getTimeStamp(), Qt::ISODate);
        }

        // Round current time down to the nearest 5-minute mark
        int currentMinute = currentBarTime.time().minute();
        int baseMinute = (currentMinute / 5) * 5;
        QDateTime baseTime = currentBarTime;
        baseTime.setTime(QTime(currentBarTime.time().hour(), baseMinute, 0));

        // Set start time to 30 minutes before the base time
        QDateTime startTime = baseTime.addSecs(-30 * 60);
        // Set end time to the next 5-minute mark after base time plus a small buffer
        QDateTime endTime = baseTime.addSecs(5 * 60 + 60); // Next 5-min mark + 1 min buffer

        axisX->setRange(startTime, endTime);

        // Calculate price range
        double minPrice = std::numeric_limits<double>::max();
        double maxPrice = std::numeric_limits<double>::lowest();
        double currentPrice = 0.0;

        // Find min/max prices and current price
        for (int i = 0; i < candlestickSeries->count(); ++i) {
            auto set = candlestickSeries->sets().at(i);
            minPrice = qMin(minPrice, set->low());
            maxPrice = qMax(maxPrice, set->high());
            if (i == candlestickSeries->count() - 1) {
                currentPrice = set->close();
            }
        }

        if (minPrice != std::numeric_limits<double>::max() && maxPrice != std::numeric_limits<double>::lowest()) {
            // Always ensure a minimum range relative to the current price
            double minRange = currentPrice * 0.0005; // 0.05% of current price
            if (maxPrice - minPrice < minRange) {
                maxPrice = currentPrice + (minRange / 2);
                minPrice = currentPrice - (minRange / 2);
            }

            // Add padding
            double padding = currentPrice * 0.0002; // 0.02% padding
            axisY->setRange(minPrice - padding, maxPrice + padding);
        }
    }
}

void StockPriceChart::wheelEvent(QWheelEvent* event) {
    // Only handle vertical scrolling when mouse is over the chart
    if (chartView->rect().contains(event->position().toPoint())) {
        // Get current y-axis range
        qreal currentMin = axisY->min();
        qreal currentMax = axisY->max();
        qreal range = currentMax - currentMin;
        qreal center = (currentMax + currentMin) / 2;

        // Calculate zoom factor based on scroll direction
        qreal zoomFactor = event->angleDelta().y() > 0 ? 0.9 : 1.1; // Zoom in for positive delta, out for negative

        // Calculate new range while keeping the center point
        qreal newRange = range * zoomFactor;
        qreal newMin = center - (newRange / 2);
        qreal newMax = center + (newRange / 2);

        // Update y-axis range
        axisY->setRange(newMin, newMax);

        // Update the price label position to match the new y-axis scale
        updatePriceLabelPosition();

        event->accept();
    } else {
        event->ignore();
    }
}
