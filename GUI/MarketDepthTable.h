#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "../Clients/TSClient/MarketData/StreamMarketDepthQuote/MarketDepthQuote.h"

class QLabel;
class MarketDepthTableView;

class MarketDepthTable : public QWidget {
    Q_OBJECT
public:
    explicit MarketDepthTable(QWidget* parent = nullptr);
    ~MarketDepthTable();

    // Update the table with new market depth data
    void updateData(const QVector<MarketDepthLevel>& bids, const QVector<MarketDepthLevel>& asks, double bidAskImbalance);

private:
    void setupUI();
    void setupStyles();
    void setMarketDepthItem(QStandardItem* item, const MarketDepthLevel& level, const QString& field, int rowIndex);

    MarketDepthTableView* tableView;
    QStandardItemModel* model;
    QLabel* bidLabel;
    QLabel* askLabel;
    QLabel* spreadLabel;
}; 