#ifndef STOCKPRICECHART_H
#define STOCKPRICECHART_H

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QDateTime>

QT_USE_NAMESPACE

class StockPriceChart : public QWidget {
    Q_OBJECT
public:
    explicit StockPriceChart(QWidget* parent = nullptr);
    ~StockPriceChart() override;

    void setSymbol(const QString& symbol);

public slots:
    void addPrice(double price, const QDateTime& timestamp);

private:
    QString symbol;
    QChart* chart;
    QLineSeries* series;
    QChartView* chartView;
    QDateTimeAxis* axisX;
    QValueAxis* axisY;
};

#endif // STOCKPRICECHART_H
