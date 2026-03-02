#pragma once

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/Models/Trade.h"

/**
 * @class TimeAndSalesWidget
 * @brief Displays a scrolling tape of real-time trade prints (Time & Sales).
 *
 * Shows each individual trade with timestamp, price, size, and color-coded
 * direction (green = buy aggressor hitting ask, red = sell aggressor hitting bid).
 *
 * The widget maintains a fixed-size ring of the most recent trades. Newest
 * trades appear at the top and the table auto-scrolls to keep them visible.
 *
 * Thread context: All public methods must be called from the GUI thread.
 */
class TimeAndSalesWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit TimeAndSalesWidget(QWidget* p_parent = nullptr);

    /// Add a new trade print to the tape
    void onNewTrade(const QString& p_symbol, const Trade& p_trade);

    /// Clear all trades (e.g., on symbol change)
    void clearData();

    /// Set the maximum number of rows to display
    void setMaxRows(int p_maxRows);

  private:
    void setupUI();
    void setupStyles();

    QLabel* m_headerLabel;
    QTableWidget* m_table;

    int m_maxRows = 200;

    // Column indices
    static constexpr int COL_TIME = 0;
    static constexpr int COL_PRICE = 1;
    static constexpr int COL_SIZE = 2;
    static constexpr int NUM_COLS = 3;
};
