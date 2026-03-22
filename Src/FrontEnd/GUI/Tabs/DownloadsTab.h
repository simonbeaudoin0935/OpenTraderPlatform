#pragma once

#include "DBClient.h"

#include <QDate>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <expected>

/**
 * @class DownloadsTab
 * @brief Tab for exploring and downloading recorded Databento market data (.dbn.zst files)
 *
 * Provides:
 * - A download section to fetch replay data for a list of symbols on a given date
 * - A three-column browser for existing recorded data (days, symbols, file details)
 *
 * Data is stored in:
 * - ~/.local/share/L2Trader/ReplayData/{YYYY-MM-DD}/{symbol}_mbp10.dbn.zst
 * - ~/.local/share/L2Trader/ReplayData/{YYYY-MM-DD}/{symbol}_trades.dbn.zst
 */
class DownloadsTab : public QWidget
{
    Q_OBJECT

  public:
    explicit DownloadsTab(QWidget* p_parent = nullptr);
    ~DownloadsTab() override = default;

    /**
     * @brief Parse a CSV file to extract ticker symbols from the first column.
     * Skips the header row. Handles quoted fields like "AARD".
     * @param p_filePath Path to the CSV file
     * @return List of ticker symbols (uppercased, trimmed)
     */
    [[nodiscard]] static QStringList parseSymbolCsv(const QString& p_filePath);

    [[nodiscard]] std::expected<DBClient::ReplayDownloadBatchResult, QString>
    startExternalDownloadBatch(const QDate& p_date, const QStringList& p_symbols);

  signals:
    /**
     * @brief Emitted after the current replay-download batch finishes updating tab state
     * Thread context: Emitted from Main/GUI thread
     */
    void downloadBatchFinished();

  private slots:
    void onRefreshClicked();
    void onDaySelected();
    void onBrowseCsvClicked();
    void onBrowseReplayDirClicked();
    void onDownloadClicked();
    void
    onDownloadFinished(const QString& p_symbol, const QDate& p_date, bool p_success, const QString& p_errorMessage);
    void onDaysTableContextMenu(const QPoint& p_pos);

  private:
    void setupUI();
    void scanRecordedDays();
    void loadSymbolsForDay(const QDate& p_date);
    void clearSymbolsList();
    void beginDownloadBatch(const QDate& p_date,
                            const QStringList& p_requestedSymbols,
                            const QStringList& p_queue,
                            const QStringList& p_skippedSymbols);
    void dispatchDownloads();
    void finishDownload();
    void updateDaysTableRow(const QDate& p_date);
    void saveCsvPath();
    void saveManualSymbols();
    void saveReplayDir();

    [[nodiscard]] QString formatFileSize(qint64 p_bytes) const;
    [[nodiscard]] QStringList collectRequestedSymbols() const;
    [[nodiscard]] QStringList parseManualSymbols() const;
    [[nodiscard]] QStringList buildDownloadQueue(const QDate& p_date, QStringList* p_outSkipped = nullptr) const;
    [[nodiscard]] static QStringList normalizeSymbolList(const QStringList& p_symbols);
    void updateSkipLog(const QStringList& p_skippedSymbols);

    // UI - Download section
    QDateEdit* m_dateEdit;
    QLineEdit* m_replayDirEdit;
    QPushButton* m_browseReplayDirButton;
    QPushButton* m_resetReplayDirButton;
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

    // Download state
    QDate m_downloadDate;
    QStringList m_downloadQueue;
    int m_nextDownloadIndex = 0;
    int m_completedCount = 0;
    int m_downloadSuccessCount = 0;
    int m_downloadFailCount = 0;
    QSet<QString> m_inFlightSymbols;
    DBClient::ReplayDownloadBatchResult m_activeDownloadBatch;

    // Browser state
    QDate m_selectedDate;
};
