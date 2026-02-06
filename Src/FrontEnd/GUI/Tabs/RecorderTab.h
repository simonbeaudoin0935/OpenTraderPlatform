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

// Forward declarations
class LiveStreamDB;

class RecorderTab : public QWidget
{
    Q_OBJECT

  public:
    explicit RecorderTab(QWidget* p_parent = nullptr);
    ~RecorderTab() override;

  private slots:
    void onStartRecording();
    void onStopRecording();
    void refreshRecorderStats();
    void onTradeStationAuthStateChanged(bool p_isAuthenticated, QString p_reason);
    void onBrowseButtonClicked();
    void onCsvFilePathChanged(const QString& p_text);

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
    QLabel* m_recordingSizeLabel;
    QTimer* m_refreshTimer;

    // Recorder state
    bool m_isRecording;
    bool m_isAuthenticated;
    QDateTime m_startTime;
    std::unique_ptr<LiveStreamDB> m_liveBarsDB;
    std::unique_ptr<LiveStreamDB> m_liveMarketDepthQuoteDB;
    QStringList m_stockTickers;
    QString m_stockCsvFilePath;
};
