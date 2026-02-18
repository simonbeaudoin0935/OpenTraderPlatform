#pragma once

#include <QWidget>
#include <QStandardItemModel>

#include "MarketDepthQuote.h"
#include "Quote.h"

class QLabel;
class MarketDepthTableView;

class MarketDepthTable : public QWidget
{
    Q_OBJECT
  public:
    /// Display mode for market depth data
    enum class DisplayMode
    {
        Level2, ///< Full order book depth (10 levels)
        Level1, ///< Best bid/ask only from Quote stream
        NoData  ///< No data available
    };

    explicit MarketDepthTable(QWidget* parent = nullptr);
    ~MarketDepthTable();

    /// Update market depth display with Level 2 data (full book)
    /// @param bids Vector of bid levels (price, size, MPID)
    /// @param asks Vector of ask levels (price, size, MPID)
    void updateData(const QVector<MarketDepthLevel>& bids, const QVector<MarketDepthLevel>& asks);

    /// Update market depth display with Level 1 data (best bid/ask only)
    /// @param quote Quote containing best bid/ask
    void updateLevel1Data(const Quote& quote);

    /// Update Depth-Weighted Price indicators
    /// @param bidDWP Bid side depth-weighted price
    /// @param askDWP Ask side depth-weighted price
    void updateDWP(double bidDWP, double askDWP);

    /// Get the current display mode
    [[nodiscard]] DisplayMode getDisplayMode() const
    {
        return m_displayMode;
    }

    /// Clear all data and show NoData state
    void clearData();

  private:
    void setupUI();
    void setupStyles();
    void setMarketDepthItem(QStandardItem* item, const MarketDepthLevel& level, const QString& field, int rowIndex);
    void updateDataSourceIndicator();

    MarketDepthTableView* tableView;
    QStandardItemModel* model;
    QLabel* bidLabel;
    QLabel* askLabel;
    QLabel* spreadLabel;
    QLabel* dwpLabel;
    QLabel* bidDWPLabel;
    QLabel* askDWPLabel;
    QLabel* m_dataSourceLabel = nullptr; ///< Shows L2/L1/-- indicator

    DisplayMode m_displayMode = DisplayMode::NoData;
};
