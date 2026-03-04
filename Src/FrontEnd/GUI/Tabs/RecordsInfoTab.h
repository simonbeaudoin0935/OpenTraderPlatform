#pragma once

#include <QDate>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

/**
 * @class RecordsInfoTab
 * @brief Tab for exploring and downloading recorded Databento market data (.dbn.zst files)
 *
 * Provides:
 * - A download section to fetch replay data for a list of symbols on a given date
 * - A three-column browser for existing recorded data (days, symbols, file details)
 *
 * Data is stored in:
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

    /**
     * @brief Parse a CSV file to extract ticker symbols from the first column.
     * Skips the header row. Handles quoted fields like "AARD".
     * @param p_filePath Path to the CSV file
     * @return List of ticker symbols (uppercased, trimmed)
     */
    [[nodiscard]] static QStringList parseSymbolCsv(const QString& p_filePath);

  private slots:
    void onRefreshClicked();
    void onDaySelected();
    void onBrowseCsvClicked();
    void onDownloadClicked();
    void
    onDownloadFinished(const QString& p_symbol, const QDate& p_date, bool p_success, const QString& p_errorMessage);
    void onDaysTableContextMenu(const QPoint& p_pos);

  private:
    void setupUI();
    void scanRecordedDays();
    void loadSymbolsForDay(const QDate& p_date);
    void clearSymbolsList();
    void dispatchDownloads();
    void finishDownload();
    void updateDaysTableRow(const QDate& p_date);
    void saveCsvPath();
    void saveManualSymbols();

    [[nodiscard]] QString formatFileSize(qint64 p_bytes) const;
    [[nodiscard]] QStringList parseManualSymbols() const;
    [[nodiscard]] QStringList buildDownloadQueue(const QDate& p_date, QStringList* p_outSkipped = nullptr) const;

    // UI - Download section
    QDateEdit* m_dateEdit;
    QLineEdit* m_csvPathEdit;
    QPushButton* m_browseCsvButton;
    QLineEdit* m_manualSymbolsEdit;
    QPushButton* m_downloadButton;
    QProgressBar* m_downloadProgressBar;
    QLabel* m_downloadStatusLabel;
    QTextEdit* m_skipLogEdit;

    // UI - Left (Days)
    QTableWidget* m_daysTable;
    QPushButton* m_refreshButton;

    // UI - Middle (Symbols)
    QTableWidget* m_symbolsTable;

    static constexpr int MAX_CONCURRENT_DOWNLOADS = 5;

    // Download state
    QDate m_downloadDate;
    QStringList m_downloadQueue;
    int m_nextDownloadIndex = 0;
    int m_completedCount = 0;
    int m_downloadSuccessCount = 0;
    int m_downloadFailCount = 0;
    QSet<QString> m_inFlightSymbols;

    // Browser state
    QDate m_selectedDate;
};
