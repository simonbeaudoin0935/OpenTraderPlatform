#include "StrategiesTab.h"
#include "StrategyGridWidget.h"
#include "StrategyLoadDialog.h"

#include <QLabel>
#include <QVBoxLayout>
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

    setupUI();
    connectSignals();
    updateStrategyGrid();
}

void StrategiesTab::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // Load Strategy button at top
    m_loadStrategyButton = new QPushButton("Load Strategy");
    m_loadStrategyButton->setMaximumWidth(150);
    m_mainLayout->addWidget(m_loadStrategyButton);

    // Horizontal scroll area for strategy tiles with embedded panels
    m_gridScrollArea = new QScrollArea();
    m_gridScrollArea->setWidgetResizable(true);
    m_gridScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_gridScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_gridScrollArea->setWidget(m_strategyGrid.get());
    m_mainLayout->addWidget(m_gridScrollArea);
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

    // Connect to user actions
    auto loadConn = connect(m_loadStrategyButton, &QPushButton::clicked, this, &StrategiesTab::onLoadStrategyClicked);
    OBJ_ASSUME_TRUE(loadConn);

    // Connect to grid card selection
    auto gridConn = connect(m_strategyGrid.get(),
                            &StrategyGridWidget::strategyCardClicked,
                            this,
                            &StrategiesTab::onStrategyTileClicked,
                            Qt::QueuedConnection);
    OBJ_ASSUME_TRUE(gridConn);
}

void StrategiesTab::updateStrategyGrid()
{
    // Get all active strategies from StrategyManager and add them to grid
    auto strategyIDs = m_strategyManager->getActiveStrategies();
    for (const auto& strategyID: strategyIDs)
    {
        auto config = m_strategyManager->getStrategyConfig(strategyID);
        bool isRunning = m_strategyManager->isStrategyRunning(strategyID);
        m_strategyGrid->addStrategyCard(strategyID, config.name, config.symbols, isRunning);
    }
}

void StrategiesTab::onStrategyLoaded(const QString& strategyID, const QString& name)
{
    qDebug() << "[StrategiesTab] Strategy loaded:" << strategyID << name;

    // Get strategy config from StrategyManager
    auto config = m_strategyManager->getStrategyConfig(strategyID);
    bool isRunning = m_strategyManager->isStrategyRunning(strategyID);
    m_strategyGrid->addStrategyCard(strategyID, name, config.symbols, isRunning);
}

void StrategiesTab::onStrategyUnloaded(const QString& strategyID)
{
    qDebug() << "[StrategiesTab] Strategy unloaded:" << strategyID;
    m_strategyGrid->removeStrategyCard(strategyID);
}

void StrategiesTab::onStrategyStatusChanged(const QString& strategyID, bool isRunning, const QString& errorMessage)
{
    m_strategyGrid->updateStrategyCardStatus(strategyID, isRunning, errorMessage);
}

void StrategiesTab::onLoadStrategyClicked()
{
    auto dialog = std::make_unique<StrategyLoadDialog>(this);

    if (dialog->exec() == QDialog::Accepted)
    {
        auto config = dialog->getSelectedConfig();
        if (config)
        {
            auto result = m_strategyManager->loadStrategy(config.value());
            if (!result)
            {
                qWarning() << "[StrategiesTab] Failed to load strategy:" << result.error();
            }
        }
    }
}

void StrategiesTab::onStrategyTileClicked(const QString& strategyID)
{
    m_selectedStrategyID = strategyID;
    m_strategyGrid->setSelectedCard(strategyID);
}
