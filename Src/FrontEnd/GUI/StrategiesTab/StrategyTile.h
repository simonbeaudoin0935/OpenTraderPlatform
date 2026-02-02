#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <memory>

class StrategyManager;

/**
 * @brief StrategyTile - Individual strategy card in the grid
 *
 * Displays:
 * - Strategy name
 * - Symbols being monitored
 * - Status (running/error) with color coding
 * - PnL (shows 0 for now - TODO: implement later)
 * - Open positions count
 * - Thread ID and CPU/memory usage (real-time from /proc)
 * - Last update timestamp (auto-updates)
 *
 * Clickable to select and show details in side panel.
 */
class StrategyTile : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyTile(const QString& strategyID,
                          const QString& name,
                          const QVector<QString>& symbols,
                          bool isRunning,
                          StrategyManager* p_strategyManager,
                          QWidget* parent = nullptr);
    ~StrategyTile() override = default;

    // Set selection state (visual feedback)
    void setSelected(bool selected);

    // Update status
    void setStatus(bool isRunning, const QString& errorMessage);

    // Refresh display (called by timer or externally)
    void refreshDisplay();

  signals:
    void tileClicked(const QString& strategyID);

  protected:
    void mousePressEvent(QMouseEvent* event) override;

  private slots:
    void onRefreshTimer();

  private:
    void setupUI();
    void updateStatusDisplay();
    void updateThreadAndMemoryInfo();

    QString m_strategyID;
    QString m_name;
    QVector<QString> m_symbols;
    bool m_isRunning;
    QString m_errorMessage;
    bool m_isSelected;
    StrategyManager* m_strategyManager;
    std::unique_ptr<QTimer> m_refreshTimer;

    // UI components
    QFrame* m_contentFrame;
    QLabel* m_titleLabel;
    QLabel* m_symbolsLabel;
    QLabel* m_statusLabel;
    QLabel* m_pnlLabel;
    QLabel* m_positionsLabel;
    QLabel* m_threadInfoLabel;
    QLabel* m_timestampLabel;
};
