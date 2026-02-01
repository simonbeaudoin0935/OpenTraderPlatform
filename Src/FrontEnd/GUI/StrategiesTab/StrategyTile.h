#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

/**
 * @brief StrategyTile - Individual strategy card in the grid
 *
 * Displays:
 * - Strategy name
 * - Symbols being monitored
 * - Status (running/error) with color coding
 * - PnL (shows 0 for now - TODO: implement later)
 * - Open positions count
 * - Thread ID and CPU/memory usage
 * - Last update timestamp
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
                          QWidget* parent = nullptr);
    ~StrategyTile() override = default;

    // Set selection state (visual feedback)
    void setSelected(bool selected);

    // Update status
    void setStatus(bool isRunning, const QString& errorMessage);

  signals:
    void tileClicked(const QString& strategyID);

  protected:
    void mousePressEvent(QMouseEvent* event) override;

  private:
    void setupUI();
    void updateStatusDisplay();

    QString m_strategyID;
    QString m_name;
    QVector<QString> m_symbols;
    bool m_isRunning;
    QString m_errorMessage;
    bool m_isSelected;

    // UI components
    QLabel* m_titleLabel;
    QLabel* m_symbolsLabel;
    QLabel* m_statusLabel;
    QLabel* m_pnlLabel;
    QLabel* m_positionsLabel;
    QLabel* m_threadInfoLabel;
    QLabel* m_timestampLabel;
};
