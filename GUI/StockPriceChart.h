#pragma once

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QDateTime>
#include <QGraphicsTextItem>
#include "Clients/TSClient/MarketData/StreamBars/Bar.h"

QT_USE_NAMESPACE

class QGraphicsRectItem;

class StockPriceChart : public QWidget {
    Q_OBJECT

public:
    explicit StockPriceChart(QWidget* parent = nullptr);
    ~StockPriceChart() override;

    void setSymbol(const QString& symbol);

public slots:
    void addPrice(double price, const QDateTime& timestamp);
    void addBar(const Bar& bar);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void updateChart();
    void handleClosedBar(const Bar& bar);
    void handleOpenBar(const Bar& bar);
    void updateLastPriceLine(double price, bool isUpTick);
    void updatePriceLabelPosition();
    bool isAfterMarketHours(const QDateTime& localTime);
    void updateAfterHoursBackground();

    QString symbol;
    QChart* chart;
    QLineSeries* lineSeries;
    QLineSeries* lastPriceLine;
    QCandlestickSeries* candlestickSeries;
    QChartView* chartView;
    QDateTimeAxis* axisX;
    QValueAxis* axisY;
    QGraphicsTextItem* priceLabel;
    QGraphicsRectItem* afterHoursRect;

    // Track the current open bar
    Bar currentOpenBar;
    bool hasOpenBar = false;
    double lastPrice = 0.0;

    // Store completed bars
    QList<Bar> completedBars;
    static const int MAX_BARS = 100; // Maximum number of bars to display

    // Mouse tracking for panning
    bool isPanning = false;
    QPoint lastMousePos;
};
