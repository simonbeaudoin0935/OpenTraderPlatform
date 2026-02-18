#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QSplitter>
#include <QDate>
#include <QSqlDatabase>
#include <QMap>

/**
 * @class RecordsInfoTab
 * @brief Tab for exploring recorded market data
 *
 * Provides a three-column interface to browse recorded market data:
 * - Left: List of recorded days (date, bar count, file size)
 * - Middle: Stocks recorded for selected day (symbol, bar count, depth availability)
 * - Right: Detailed metrics for selected stock (bars + market depth)
 *
 * Data is loaded from:
 * - ~/.cache/L2Trader/RecordedLiveData/Bars/{YYYY-MM-DD}.db
 * - ~/.cache/L2Trader/RecordedLiveData/MarketDepthQuotes/{YYYY-MM-DD}.db
 *
 * Note: Due to TradeStation API limit (10 concurrent depth streams), not all
 * stocks in Bars database will have corresponding MarketDepthQuotes data.
 */
class RecordsInfoTab : public QWidget
{
    Q_OBJECT

  public:
    explicit RecordsInfoTab(QWidget* p_parent = nullptr);
    ~RecordsInfoTab() override = default;

    /**
     * @brief Structure to hold metrics for a stock
     */
    struct StockMetrics
    {
        QString symbol;
        qint64 barCount = 0;
        qint64 firstTimestampMs = 0;
        qint64 lastTimestampMs = 0;
        bool hasMarketDepth = false;
        qint64 depthCount = 0;
        qint64 depthFirstTimestampMs = 0;
        qint64 depthLastTimestampMs = 0;
    };

  private slots:
    /// Refresh the list of recorded days
    void onRefreshClicked();

    /// Handle day selection change
    /// Loads stocks for the selected day
    void onDaySelected();

    /// Handle stock selection change
    /// Loads detailed metrics for the selected stock
    void onStockSelected();

  private:
    void setupUI();
    void scanRecordedDays();
    void loadStocksForDay(const QDate& p_date);
    void loadStockDetails(const QDate& p_date, const QString& p_symbol);
    void updateDetailsDisplay(const StockMetrics& p_metrics);
    void clearStocksList();
    void clearDetailsDisplay();

    // Helper methods
    [[nodiscard]] QString getBarsDbPath(const QDate& p_date) const;
    [[nodiscard]] QString getMarketDepthDbPath(const QDate& p_date) const;
    [[nodiscard]] QString formatFileSize(qint64 p_bytes) const;
    [[nodiscard]] QString formatDuration(qint64 p_durationMs) const;
    [[nodiscard]] QString formatTimestamp(qint64 p_epochMs) const;

    // Database query methods
    [[nodiscard]] QStringList getStocksFromDatabase(const QString& p_dbPath);
    [[nodiscard]] bool checkStockInDatabase(const QString& p_dbPath, const QString& p_symbol);
    [[nodiscard]] StockMetrics
    queryStockMetrics(const QString& p_barsDbPath, const QString& p_depthDbPath, const QString& p_symbol);

    // UI Components - Left column (Days)
    QTableWidget* m_daysTable;
    QPushButton* m_refreshButton;

    // UI Components - Middle column (Stocks)
    QTableWidget* m_stocksTable;

    // UI Components - Right column (Details)
    QLabel* m_symbolLabel;
    QGroupBox* m_barsGroupBox;
    QLabel* m_barsCountLabel;
    QLabel* m_barsFirstTimeLabel;
    QLabel* m_barsLastTimeLabel;
    QLabel* m_barsDurationLabel;
    QGroupBox* m_depthGroupBox;
    QLabel* m_depthStatusLabel;
    QLabel* m_depthCountLabel;
    QLabel* m_depthFirstTimeLabel;
    QLabel* m_depthLastTimeLabel;
    QLabel* m_depthDurationLabel;

    // State
    QDate m_selectedDate;
    QString m_selectedSymbol;

    // Cache: Date -> (Symbol -> hasDepth)
    QMap<QDate, QMap<QString, bool>> m_depthAvailabilityCache;
};
