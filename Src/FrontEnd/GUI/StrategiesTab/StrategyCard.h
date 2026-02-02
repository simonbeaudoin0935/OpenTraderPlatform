#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>
#include <memory>

class StrategyManager;

/**
 * @brief StrategyCard - Unified widget combining strategy tile and details panel
 *
 * Single bordered frame containing:
 * - Header: Title, Symbols, Status, Positions, Thread info
 * - Separator line
 * - Content: Orders, Positions, Logs
 *
 * Fixed-width (400px) for horizontal scrolling layout.
 */
class StrategyCard : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyCard(const QString& strategyID,
                          const QString& name,
                          const QVector<QString>& symbols,
                          bool isRunning,
                          StrategyManager* p_strategyManager,
                          QWidget* parent = nullptr);
    ~StrategyCard() override = default;

    // Update status
    void setStatus(bool isRunning, const QString& errorMessage);

    // Refresh display
    void refreshDisplay();

    // Get the strategy ID
    [[nodiscard]] QString getStrategyID() const;

  signals:
    void cardClicked(const QString& strategyID);

  protected:
    void mousePressEvent(QMouseEvent* event) override;

  private slots:
    void onRefreshTimer();
    void onLogsLevelFilterChanged(int index);
    void onStartClicked();
    void onStopClicked();
    void onLogsScrolled();

  private:
    void setupUI();
    void setupHeader();
    void setupContent();
    void updateStatus();
    void updateThreadInfo();
    void updateLogs();
    QString levelToString(QtMsgType level) const;
    QString levelToColor(QtMsgType level) const;

    QString m_strategyID;
    QString m_name;
    QVector<QString> m_symbols;
    bool m_isRunning;
    QString m_errorMessage;
    StrategyManager* m_strategyManager;
    std::unique_ptr<QTimer> m_refreshTimer;

    // UI components - Header
    QLabel* m_titleLabel;
    QLabel* m_symbolsLabel;
    QLabel* m_statusLabel;
    QLabel* m_positionsLabel;
    QLabel* m_threadInfoLabel;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;

    // UI components - Content
    QTextEdit* m_ordersDisplay;
    QTextEdit* m_positionsDisplay;
    QComboBox* m_logsLevelFilter;
    QTextEdit* m_logsDisplay;
    QLabel* m_logsStatsLabel;

    int m_selectedLogLevel = -1;  // -1 means "All"
    bool m_logsAutoScroll = true; // Track if logs should auto-scroll
};
