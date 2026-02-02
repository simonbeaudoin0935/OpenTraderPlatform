#pragma once

#include <QWidget>
#include <QVBoxLayout>

class StrategyTile;
class StrategyDetailsPanel;
class StrategyManager;

/**
 * @brief StrategyTileWithPanel - Composite widget combining tile and embedded panel
 *
 * Layout:
 * - Top: StrategyTile (header with name, symbols, status)
 * - Bottom: StrategyDetailsPanel (fixed height, scrollable internally)
 *
 * Each tile has its own panel instance, no shared state.
 * Fixed width (400px) for consistent horizontal scrolling.
 */
class StrategyTileWithPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyTileWithPanel(const QString& strategyID,
                                   const QString& name,
                                   const QVector<QString>& symbols,
                                   bool isRunning,
                                   StrategyManager* p_strategyManager,
                                   QWidget* parent = nullptr);
    ~StrategyTileWithPanel() override = default;

    // Set selection state (visual feedback with border)
    void setSelected(bool selected);

    // Update tile status
    void setStatus(bool isRunning, const QString& errorMessage);

    // Refresh display
    void refreshDisplay();

    // Get the strategy ID
    [[nodiscard]] QString getStrategyID() const;

  signals:
    void tileClicked(const QString& strategyID);

  protected:
    void mousePressEvent(QMouseEvent* event) override;

  private:
    QString m_strategyID;
    bool m_isSelected;

    // UI components (using raw pointers - Qt parent/child management)
    QVBoxLayout* m_mainLayout;
    StrategyTile* m_tile;
    StrategyDetailsPanel* m_panel;
};
