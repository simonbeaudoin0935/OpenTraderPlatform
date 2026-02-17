#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <memory>

class StrategyManager;
class MainAlgo;
class StrategyGridWidget;
class StrategyLoadDialog;

/**
 * @brief StrategiesTab - Main UI container for strategy management
 *
 * Layout:
 * - Top: "Load Strategy" button
 * - Main: Horizontal scrolling area with strategy tiles + embedded panels
 *
 * Each tile is a self-contained unit with:
 * - Strategy header info (name, symbols, status)
 * - Embedded details panel below (fixed height with internal scroll)
 *
 * Responsibilities:
 * - Create and manage strategy grid with composite tiles
 * - Connect to StrategyManager signals for real-time updates
 * - Handle user interactions (load, unload)
 */
class StrategiesTab : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategiesTab(MainAlgo* p_mainAlgo, QWidget* parent = nullptr);
    ~StrategiesTab() override = default;

  private slots:
    // Strategy lifecycle signals from StrategyManager
    void onStrategyLoaded(const QString& strategyID, const QString& name);
    void onStrategyUnloaded(const QString& strategyID);
    void onStrategyStatusChanged(const QString& strategyID, bool isRunning, const QString& errorMessage);

    // User actions
    void onLoadStrategyClicked();
    void onStrategyTileClicked(const QString& strategyID);

  private:
    void setupUI();
    void connectSignals();
    void updateStrategyGrid();

    MainAlgo* m_mainAlgo;
    StrategyManager* m_strategyManager;
    QString m_selectedStrategyID; // Currently selected strategy

    // UI Components
    QVBoxLayout* m_mainLayout;
    QPushButton* m_loadStrategyButton;
    QScrollArea* m_gridScrollArea;
    std::unique_ptr<StrategyGridWidget> m_strategyGrid;
};
