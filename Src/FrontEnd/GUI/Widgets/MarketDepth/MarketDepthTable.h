#pragma once

#include <QWidget>
#include <QStandardItemModel>

#include "Level2.h"

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
    void updateData(const std::array<Level2Row, 10>& bids, const std::array<Level2Row, 10>& asks);

    /// Update market depth display with Level 1 data (best bid/ask only)
    /// @param level1 Level1 BBO data
    void updateLevel1Data(const Level1& level1);

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

    /// Pre-set the display mode indicator without adding any data rows.
    /// Called when entering replay mode to show what data will be available
    /// before any replay data has been emitted.
    /// @param p_hasLevel2 True if Level 2 depth data exists in the replay DB for this symbol
    /// @param p_hasLevel1 True if Level 1 quote data exists in the replay DB for this symbol
    void setExpectedDataMode(bool p_hasLevel2, bool p_hasLevel1);

  private:
    void setupUI();
    void setupStyles();
    void setMarketDepthItem(QStandardItem* item, const Level2Row& level, const QString& field, int rowIndex);
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
