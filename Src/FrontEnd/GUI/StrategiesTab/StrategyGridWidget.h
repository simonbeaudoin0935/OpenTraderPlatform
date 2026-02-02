#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QMap>

class StrategyTileWithPanel;
class StrategyManager;

/**
 * @brief StrategyGridWidget - Horizontal container for strategy tiles with embedded panels
 *
 * Displays all active strategies as composite tiles (tile + panel below) in horizontal layout.
 * Each tile shows: name, symbols, status, PnL, positions, thread ID, CPU/memory, timestamp.
 * Each tile has its own embedded details panel below it.
 *
 * Layout: Horizontal scrolling, fixed-width tiles (400px).
 *
 * Signals:
 * - strategyTileClicked(strategyID) - emitted when a tile is clicked
 */
class StrategyGridWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyGridWidget(StrategyManager* p_strategyManager, QWidget* parent = nullptr);
    ~StrategyGridWidget() override = default;

    // Add a strategy tile with panel
    void
    addStrategyTile(const QString& strategyID, const QString& name, const QVector<QString>& symbols, bool isRunning);

    // Update status of a specific tile
    void updateStrategyTileStatus(const QString& strategyID, bool isRunning, const QString& errorMessage);

    // Remove a specific tile
    void removeStrategyTile(const QString& strategyID);

    // Set which tile is selected (visual highlighting)
    void setSelectedTile(const QString& strategyID);

    // Clear all tiles
    void clearTiles();

  signals:
    void strategyTileClicked(const QString& strategyID);

  private:
    void setupUI();

    QHBoxLayout* m_mainLayout;
    QMap<QString, StrategyTileWithPanel*> m_tiles; // strategyID -> tile+panel widget
    QString m_selectedStrategyID;
    StrategyManager* m_strategyManager;
};
