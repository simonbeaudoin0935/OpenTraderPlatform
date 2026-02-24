#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTimer>
#include <QDateTime>
#include <QLineEdit>
#include <memory>

#include "TSClient.h" // For TSClient::AuthStateReason enum

// Forward declarations
class LiveStreamDB;

class RecorderTab : public QWidget
{
    Q_OBJECT

  public:
    explicit RecorderTab(QWidget* p_parent = nullptr);
    ~RecorderTab() override;

  private slots:
    /// Start recording live market data streams
    void onStartRecording();

    /// Stop recording and close database connections
    void onStopRecording();

    /// Refresh recording statistics display
    /// Updates record counts, size, uptime
    void refreshRecorderStats();

    /// Handle TradeStation authentication state changes
    /// Enables/disables recording based on auth state
    /// @param p_isAuthenticated True if authenticated
    /// @param p_reason Reason code for state change
    /// @param p_message Human-readable message
    void onTradeStationAuthStateChanged(bool p_isAuthenticated, TSClient::AuthStateReason p_reason, QString p_message);

    /// Handle browse button click for CSV file selection
    void onBrowseButtonClicked();

    /// Handle CSV file path text changes
    /// Validates and saves the new path
    /// @param p_text New file path
    void onCsvFilePathChanged(const QString& p_text);

  signals:
    /// Emitted when total recording size changes
    /// @param totalBytes Total bytes written to databases
    void recordingSizeChanged(qint64 totalBytes);

  private:
    void setupUI();
    void updateStatsDisplay();
    QString formatFileSize(qint64 p_bytes) const;
    QString formatUptime(qint64 p_seconds) const;
    void updateStreamTable();
    void updateErrorTable();
    void saveLastCsvFilePath(const QString& p_filePath);
    void restoreLastCsvFilePath();

    // UI Components
    QTableWidget* m_streamTable;
    QTableWidget* m_errorTable;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_refreshButton;
    QPushButton* m_browseButton;
    QLineEdit* m_stockCsvFileInput;
    QLabel* m_statusLabel;
    QLabel* m_uptimeLabel;
    QLabel* m_barsRecordCountLabel;
    QLabel* m_depthRecordCountLabel;
    QLabel* m_quotesRecordCountLabel;
    QLabel* m_recordingSizeLabel;
    QTimer* m_refreshTimer;

    // Recorder state
    bool m_isRecording;
    bool m_isAuthenticated;
    QDateTime m_startTime;
    std::unique_ptr<LiveStreamDB> m_liveBarsDB;
    std::unique_ptr<LiveStreamDB> m_liveMarketDepthQuoteDB;
    std::unique_ptr<LiveStreamDB> m_liveQuotesDB;
    QStringList m_stockTickers;
    QString m_stockCsvFilePath;
};
