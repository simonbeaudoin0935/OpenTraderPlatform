#pragma once

#include <QDate>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

/**
 * @class RecordsInfoTab
 * @brief Tab for exploring recorded Databento market data (.dbn.zst files)
 *
 * Provides a three-column interface to browse recorded market data:
 * - Left: List of recorded days (date, file count, total size)
 * - Middle: Symbols recorded for selected day (with Mbp10/Trades availability)
 * - Right: File details for selected symbol (sizes, paths)
 *
 * Data is loaded from:
 * - ~/.cache/L2Trader/ReplayData/{YYYY-MM-DD}/{symbol}_mbp10.dbn.zst
 * - ~/.cache/L2Trader/ReplayData/{YYYY-MM-DD}/{symbol}_trades.dbn.zst
 */
class RecordsInfoTab : public QWidget
{
    Q_OBJECT

  public:
    explicit RecordsInfoTab(QWidget* p_parent = nullptr);
    ~RecordsInfoTab() override = default;

    struct SymbolFiles
    {
        QString symbol;
        bool hasMbp10 = false;
        bool hasTrades = false;
        qint64 mbp10Size = 0;
        qint64 tradesSize = 0;
    };

  private slots:
    void onRefreshClicked();
    void onDaySelected();
    void onStockSelected();

  private:
    void setupUI();
    void scanRecordedDays();
    void loadSymbolsForDay(const QDate& p_date);
    void loadSymbolDetails(const QDate& p_date, const QString& p_symbol);
    void clearSymbolsList();
    void clearDetailsDisplay();

    [[nodiscard]] QString formatFileSize(qint64 p_bytes) const;

    // UI - Left (Days)
    QTableWidget* m_daysTable;
    QPushButton* m_refreshButton;

    // UI - Middle (Symbols)
    QTableWidget* m_symbolsTable;

    // UI - Right (Details)
    QLabel* m_symbolLabel;
    QGroupBox* m_mbp10GroupBox;
    QLabel* m_mbp10StatusLabel;
    QLabel* m_mbp10SizeLabel;
    QLabel* m_mbp10PathLabel;
    QGroupBox* m_tradesGroupBox;
    QLabel* m_tradesStatusLabel;
    QLabel* m_tradesSizeLabel;
    QLabel* m_tradesPathLabel;

    // State
    QDate m_selectedDate;
    QString m_selectedSymbol;
};
