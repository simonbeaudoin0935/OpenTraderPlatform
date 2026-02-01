#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QString>
#include <QTimer>
#include <QTextEdit>
#include <memory>

class StrategyManager;

/**
 * @brief StrategyDetailsPanel - Docked right panel showing strategy details
 *
 * Displays when a strategy tile is selected:
 * - Full config (name, symbols, position size, risk limits, custom params)
 * - Recent orders/trades (last 20)
 * - Current balance and PnL
 * - Open positions breakdown
 * - CPU/Memory usage (updated periodically)
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

  private:
    void setupUI();
    void updateDisplay();
    void updateOrdersList();
    void updatePositionsList();
    void updateStats();

    StrategyManager* m_strategyManager;
    QString m_currentStrategyID;
    double m_currentBalance = 0.0;

    // UI components
    QLabel* m_titleLabel;
    QLabel* m_statusLabel;
    QLabel* m_balanceLabel;
    QLabel* m_statsLabel;
    QTextEdit* m_ordersDisplay;
    QTextEdit* m_positionsDisplay;
    QPushButton* m_stopButton;
    QPushButton* m_viewLogsButton;
    QWidget* m_emptyStateWidget;

    // Timer for periodic stats refresh (CPU/Memory)
    std::unique_ptr<QTimer> m_statsRefreshTimer;
};
