#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QMap>

class StrategyTile;
class StrategyManager;

/**
 * @brief StrategyGridWidget - Grid of strategy tiles
 *
 * Displays all active strategies as tiles/cards in a grid layout.
 * Each tile shows: name, symbols, status, PnL, positions, thread ID, CPU/memory, timestamp.
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

    // Add a strategy tile
    void
    addStrategyTile(const QString& strategyID, const QString& name, const QVector<QString>& symbols, bool isRunning);

    // Update status of a specific tile
    void updateStrategyTileStatus(const QString& strategyID, bool isRunning, const QString& errorMessage);

    // Set which tile is selected
    void setSelectedTile(const QString& strategyID);

    // Clear all tiles
    void clearTiles();

  signals:
    void strategyTileClicked(const QString& strategyID);

  private:
    void setupUI();

    QGridLayout* m_gridLayout;
    QMap<QString, StrategyTile*> m_tiles; // strategyID -> tile widget
    QString m_selectedStrategyID;
    StrategyManager* m_strategyManager;
};
