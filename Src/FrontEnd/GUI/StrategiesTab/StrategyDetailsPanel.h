#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QString>

class StrategyManager;

/**
 * @brief StrategyDetailsPanel - Docked right panel showing strategy details
 *
 * Displays when a strategy tile is selected:
 * - Full config (name, symbols, position size, risk limits, custom params)
 * - Recent orders/trades (last 20) - TODO: implement
 * - PnL chart - TODO: implement
 * - Open positions breakdown - TODO: implement
 * - Error messages if any
 *
 * Placeholder implementation for Phase 4.1.
 * Full implementation in Phase 4.4.
 */
class StrategyDetailsPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyDetailsPanel(StrategyManager* p_strategyManager, QWidget* parent = nullptr);
    ~StrategyDetailsPanel() override = default;

    // Set which strategy to display details for
    void setStrategy(const QString& strategyID);

    // Clear displayed strategy
    void clearStrategy();

  private:
    void setupUI();
    void updateDisplay();

    StrategyManager* m_strategyManager;
    QString m_currentStrategyID;

    // UI components
    QLabel* m_titleLabel;
    QLabel* m_configLabel;
    QLabel* m_statusLabel;
    QPushButton* m_stopButton;
    QPushButton* m_viewLogsButton;
    QWidget* m_emptyStateWidget;
};
