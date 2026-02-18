#pragma once

#include <QWidget>
#include <QStandardItemModel>

#include "MarketDepthQuote.h"

class QLabel;
class MarketDepthTableView;

class MarketDepthTable : public QWidget
{
    Q_OBJECT
  public:
    explicit MarketDepthTable(QWidget* parent = nullptr);
    ~MarketDepthTable();

    /// Update market depth display with new bid/ask levels
    /// @param bids Vector of bid levels (price, size, MPID)
    /// @param asks Vector of ask levels (price, size, MPID)
    void updateData(const QVector<MarketDepthLevel>& bids, const QVector<MarketDepthLevel>& asks);

    /// Update Depth-Weighted Price indicators
    /// @param bidDWP Bid side depth-weighted price
    /// @param askDWP Ask side depth-weighted price
    void updateDWP(double bidDWP, double askDWP);

  private:
    void setupUI();
    void setupStyles();
    void setMarketDepthItem(QStandardItem* item, const MarketDepthLevel& level, const QString& field, int rowIndex);

    MarketDepthTableView* tableView;
    QStandardItemModel* model;
    QLabel* bidLabel;
    QLabel* askLabel;
    QLabel* spreadLabel;
    QLabel* dwpLabel;
    QLabel* bidDWPLabel;
    QLabel* askDWPLabel;
};
