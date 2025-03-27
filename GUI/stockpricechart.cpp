#include "stockpricechart.h"
#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>

StockPriceChart::StockPriceChart(QWidget* parent)
    : QWidget(parent) {
    series = new QLineSeries();


    chart = new QChart();
    chart->addSeries(series);

    setSymbol("");

    axisX = new QDateTimeAxis();
    axisX->setTickCount(10);
    axisX->setFormat("hh:mm:ss");
    axisX->setTitleText("Time");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    axisY = new QValueAxis();
    axisY->setLabelFormat("%.2f");
    axisY->setTitleText("Price");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chartView = new QChartView(chart, this);
    chartView->setRenderHint(QPainter::Antialiasing);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(chartView);
    setLayout(layout);
}

StockPriceChart::~StockPriceChart() {
    // No need to delete chart, series, etc.—handled by Qt parent hierarchy
}

void StockPriceChart::setSymbol(const QString& symbol){
    this->symbol = symbol;
    series->setName(symbol);

    chart->setTitle("Stock Price: " + symbol);

}

void StockPriceChart::addPrice(double price, const QDateTime& timestamp) {
    series->append(timestamp.toMSecsSinceEpoch(), price);

    // Auto-adjust Y-axis range
    qreal minY = axisY->min();
    qreal maxY = axisY->max();
    if (price < minY || series->count() == 1) axisY->setMin(price - 1.0);
    if (price > maxY) axisY->setMax(price + 1.0);

    // Limit X-axis to last 60 points (e.g., 2 minutes at 2s intervals)
    if (series->count() > 60) {
        series->remove(0);
    }
    axisX->setRange(QDateTime::fromMSecsSinceEpoch(series->at(0).x()),
                    QDateTime::fromMSecsSinceEpoch(series->at(series->count() - 1).x()));
}
