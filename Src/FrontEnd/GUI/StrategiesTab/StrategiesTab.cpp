#include "StrategiesTab.h"
#include "StrategyGridWidget.h"
#include "StrategyDetailsPanel.h"
#include "StrategyLoadDialog.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QDebug>

#include "StrategyManager.h"
#include "MainAlgo.h"
#include "Assume.h"

StrategiesTab::StrategiesTab(MainAlgo* p_mainAlgo, QWidget* parent)
    : QWidget(parent), m_mainAlgo(p_mainAlgo), m_strategyManager(nullptr), m_selectedStrategyID("")
{
    ASSUME_TRUE(p_mainAlgo);

    // Get StrategyManager from MainAlgo
    m_strategyManager = m_mainAlgo->getStrategyManager();
    ASSUME_TRUE(m_strategyManager);

    // Initialize UI components
    m_strategyGrid = std::make_unique<StrategyGridWidget>(m_strategyManager);
    m_detailsPanel = std::make_unique<StrategyDetailsPanel>(m_strategyManager);

    setupUI();
    connectSignals();
    updateStrategyGrid();
}

void StrategiesTab::setupUI()
{
    m_mainWidget = new QWidget();
    m_mainLayout = new QHBoxLayout(m_mainWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // Left panel: strategy grid with load button
    m_leftPanel = new QWidget();
    m_leftLayout = new QVBoxLayout(m_leftPanel);

    // Load Strategy button at top
    m_loadStrategyButton = new QPushButton("Load Strategy");
    m_loadStrategyButton->setMaximumWidth(150);
    m_leftLayout->addWidget(m_loadStrategyButton);

    // Scroll area for strategy grid
    m_gridScrollArea = new QScrollArea();
    m_gridScrollArea->setWidgetResizable(true);
    m_gridScrollArea->setWidget(m_strategyGrid.get());
    m_leftLayout->addWidget(m_gridScrollArea);

    // Add left panel to main layout (takes ~70% of width)
    m_mainLayout->addWidget(m_leftPanel, 7);

    // Right panel: details (takes ~30% of width)
    m_mainLayout->addWidget(m_detailsPanel.get(), 3);

    // Set the main widget as this tab's content
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_mainWidget);
}

void StrategiesTab::connectSignals()
{
    // Connect to StrategyManager signals
    auto loadedConn = connect(m_strategyManager,
                              &StrategyManager::strategyLoaded,
                              this,
                              &StrategiesTab::onStrategyLoaded,
                              Qt::QueuedConnection);
    OBJ_ASSUME_TRUE(loadedConn);

    auto unloadedConn = connect(m_strategyManager,
                                &StrategyManager::strategyUnloaded,
                                this,
                                &StrategiesTab::onStrategyUnloaded,
                                Qt::QueuedConnection);
    OBJ_ASSUME_TRUE(unloadedConn);

    auto statusConn = connect(m_strategyManager,
                              &StrategyManager::strategyStatusChanged,
                              this,
                              &StrategiesTab::onStrategyStatusChanged,
                              Qt::QueuedConnection);
    OBJ_ASSUME_TRUE(statusConn);

    // Connect load button
    auto buttonConn = connect(m_loadStrategyButton,
                              &QPushButton::clicked,
                              this,
                              &StrategiesTab::onLoadStrategyClicked,
                              Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(buttonConn);

    // Connect grid tile selection
    auto tileConn = connect(m_strategyGrid.get(),
                            &StrategyGridWidget::strategyTileClicked,
                            this,
                            &StrategiesTab::onStrategyTileClicked,
                            Qt::UniqueConnection);
    OBJ_ASSUME_TRUE(tileConn);
}

void StrategiesTab::updateStrategyGrid()
{
    // Clear and rebuild grid with active strategies
    m_strategyGrid->clearTiles();

    QVector<QString> activeStrategies = m_strategyManager->getActiveStrategies();
    for (const auto& strategyID: activeStrategies)
    {
        StrategyConfig config = m_strategyManager->getStrategyConfig(strategyID);
        bool isRunning = m_strategyManager->isStrategyRunning(strategyID);
        m_strategyGrid->addStrategyTile(strategyID, config.name, config.symbols, isRunning);
    }
}

void StrategiesTab::onStrategyLoaded(const QString& strategyID, const QString& /* name */)
{
    // A new strategy was loaded - update grid
    updateStrategyGrid();

    // Auto-select the newly loaded strategy
    m_selectedStrategyID = strategyID;
    m_strategyGrid->setSelectedTile(strategyID);
    m_detailsPanel->setStrategy(strategyID);
}

void StrategiesTab::onStrategyUnloaded(const QString& strategyID)
{
    // A strategy was unloaded - update grid
    updateStrategyGrid();

    // If the unloaded strategy was selected, deselect
    if (m_selectedStrategyID == strategyID)
    {
        m_selectedStrategyID = "";
        m_detailsPanel->clearStrategy();
    }
}

void StrategiesTab::onStrategyStatusChanged(const QString& strategyID, bool isRunning, const QString& errorMessage)
{
    // Strategy status changed - update its tile
    m_strategyGrid->updateStrategyTileStatus(strategyID, isRunning, errorMessage);

    // If it's the selected strategy, update details panel
    if (m_selectedStrategyID == strategyID)
    {
        m_detailsPanel->setStrategy(strategyID);
    }
}

void StrategiesTab::onLoadStrategyClicked()
{
    // Show load strategy dialog
    auto dialog = std::make_unique<StrategyLoadDialog>(this);
    qDebug() << "[StrategiesTab] Load Strategy dialog opened";

    if (dialog->exec() == QDialog::Accepted)
    {
        qDebug() << "[StrategiesTab] Dialog accepted";
        auto config = dialog->getSelectedConfig();
        if (config.has_value())
        {
            qDebug() << "[StrategiesTab] Config loaded:" << config.value().name;
            qDebug() << "[StrategiesTab] Plugin path:" << config.value().soPath;

            // Load the strategy using StrategyManager
            auto result = m_strategyManager->loadStrategy(config.value());
            if (result.has_value())
            {
                // Success - the strategyLoaded signal will update UI
                qDebug() << "[StrategiesTab] Strategy loaded successfully, ID:" << result.value();
            }
            else
            {
                // Show error message
                qWarning() << "[StrategiesTab] Failed to load strategy:" << result.error();
            }
        }
        else
        {
            qWarning() << "[StrategiesTab] Dialog accepted but no config selected";
        }
    }
    else
    {
        qDebug() << "[StrategiesTab] Dialog cancelled";
    }
}

void StrategiesTab::onStrategyTileClicked(const QString& strategyID)
{
    m_selectedStrategyID = strategyID;
    m_strategyGrid->setSelectedTile(strategyID);
    m_detailsPanel->setStrategy(strategyID);
}
