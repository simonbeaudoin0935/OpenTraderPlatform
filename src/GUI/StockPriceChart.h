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
#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QTimeZone>

#include "Bar.h"

QT_USE_NAMESPACE

class QGraphicsRectItem;

class StockPriceChart : public QWidget {
    Q_OBJECT

public:
    explicit StockPriceChart(QWidget* parent = nullptr);
    ~StockPriceChart() override;

    void setSymbol(const QString& symbol);

public slots:
    void addBar(const Bar& bar);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    static const int MAX_BARS = 1000;

    void updateChart();
    void handleClosedBar(const Bar& bar);
    void handleOpenBar(const Bar& bar);
    void updateLastPriceLine(double price, bool isUpTick);
    void updatePriceLabelPosition();
    bool isAfterMarketHours(const QDateTime& localTime);
    void updateAfterHoursBackground();
    void maintainBarLimit();
    void handleVerticalPanning(QWheelEvent* event);
    void handleHorizontalPanning(QWheelEvent* event);
    void handleHorizontalZoom(QWheelEvent* event, qreal zoomFactor);
    void handleVerticalZoom(QWheelEvent* event, qreal zoomFactor);
    void handleBothAxesZoom(QWheelEvent* event, qreal zoomFactor);
    void updateLastPriceLineIfNeeded();
    void handlePanning(QMouseEvent* mouseEvent);

    QString symbol;
    QChart* chart;
    QLineSeries* lastPriceLine;
    QCandlestickSeries* candlestickSeries;
    QChartView* chartView;
    QDateTimeAxis* axisX;
    QValueAxis* axisY;
    QGraphicsTextItem* priceLabel;
    QList<QGraphicsRectItem*> afterHoursRects;  // List of rectangles for after-hours sessions
    QList<QGraphicsRectItem*> preMarketRects;   // List of rectangles for pre-market sessions
    QList<QGraphicsRectItem*> closedMarketRects; // List of rectangles for closed market periods

    // Track the current open bar
    Bar currentOpenBar;
    bool hasOpenBar = false;
    double lastPrice = 0.0;

    // Store completed bars
    QVector<Bar> completedBars;

    // Mouse tracking for panning
    bool isPanning = false;
    QPoint lastMousePos;

    // Helper method to create a background rectangle
    QGraphicsRectItem* createBackgroundRect(const QColor& color, int zValue);
    // Helper method to clear all background rectangles
    void clearBackgroundRects();
};
