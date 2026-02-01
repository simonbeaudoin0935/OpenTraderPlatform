#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QString>
#include <QTimer>
#include <QTextEdit>
#include <QComboBox>
#include <memory>

class StrategyManager;

/**
 * @brief StrategyDetailsPanel - Docked right panel showing strategy details and logs
 *
 * Displays when a strategy tile is selected:
 * - Full config (name, symbols, position size, risk limits, custom params)
 * - Recent orders/trades (last 5 shown)
 * - Current balance and PnL
 * - Open positions breakdown
 * - CPU/Memory usage (updated periodically)
 * - Strategy logs with filtering by level
 * - Error messages if any
 *
 * Real-time updates via StrategyManager signals.
 */
class StrategyDetailsPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyDetailsPanel(StrategyManager* p_strategyManager, QWidget* parent = nullptr);
    ~StrategyDetailsPanel() override;

    // Set which strategy to display details for
    void setStrategy(const QString& strategyID);

    // Clear displayed strategy
    void clearStrategy();

  private slots:
    void onStrategyBalanceUpdated(const QString& strategyID, double newBalance);
    void onRefreshStatsTimer();
    void onLogsLevelFilterChanged(int index);
    void onExportLogs();
    void onClearLogs();
    void onStopStrategy();
    void onStartStrategy();

  private:
    void setupUI();
    void updateDisplay();
    void updateOrdersList();
    void updatePositionsList();
    void updateStats();
    void updateLogs();
    QString levelToString(QtMsgType level) const;
    QString levelToColor(QtMsgType level) const;

    StrategyManager* m_strategyManager;
    QString m_currentStrategyID;
    double m_currentBalance = 0.0;
    int m_selectedLogLevel = -1; // Filter for logs (-1 means "All")

    // UI components
    QLabel* m_titleLabel;
    QLabel* m_statusLabel;
    QLabel* m_balanceLabel;
    QLabel* m_statsLabel;
    QTextEdit* m_ordersDisplay;
    QTextEdit* m_positionsDisplay;
    QComboBox* m_logsLevelFilter;
    QTextEdit* m_logsDisplay;
    QLabel* m_logsStatsLabel;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_viewLogsButton;
    QPushButton* m_exportLogsButton;
    QPushButton* m_clearLogsButton;
    QWidget* m_emptyStateWidget;

    // Timer for periodic stats refresh (CPU/Memory)
    std::unique_ptr<QTimer> m_statsRefreshTimer;
};
