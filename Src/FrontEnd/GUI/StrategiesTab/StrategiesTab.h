#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QMap>
#include <memory>

class StrategyManager;
class MainAlgo;
class StrategyGridWidget;
class StrategyDetailsPanel;
class StrategyLoadDialog;

/**
 * @brief StrategiesTab - Main UI container for strategy management
 *
 * Layout:
 * - Left: Strategy grid showing active strategies as tiles
 * - Right: Docked details panel showing selected strategy info
 *
 * Responsibilities:
 * - Create and manage strategy grid and details panel
 * - Connect to StrategyManager signals for real-time updates
 * - Handle user interactions (load, unload, view details)
 * - Maintain selected strategy state
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
    QWidget* m_mainWidget;
    QHBoxLayout* m_mainLayout;

    // Left side: grid of strategy tiles
    QWidget* m_leftPanel;
    QVBoxLayout* m_leftLayout;
    QPushButton* m_loadStrategyButton;
    QScrollArea* m_gridScrollArea;
    std::unique_ptr<StrategyGridWidget> m_strategyGrid;

    // Right side: details panel
    std::unique_ptr<StrategyDetailsPanel> m_detailsPanel;
};
