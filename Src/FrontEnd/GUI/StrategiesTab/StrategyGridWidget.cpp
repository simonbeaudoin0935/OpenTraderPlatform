#include "StrategyGridWidget.h"
#include "StrategyTileWithPanel.h"
#include "StrategyManager.h"

#include <QHBoxLayout>
#include <QSpacerItem>

StrategyGridWidget::StrategyGridWidget(StrategyManager* p_strategyManager, QWidget* parent)
    : QWidget(parent), m_selectedStrategyID(""), m_strategyManager(p_strategyManager)
{
    setupUI();
}

void StrategyGridWidget::setupUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);

    // Add stretchable space at the end to push tiles to the left
    m_mainLayout->addStretch();
}

void StrategyGridWidget::addStrategyTile(const QString& strategyID,
                                         const QString& name,
                                         const QVector<QString>& symbols,
                                         bool isRunning)
{
    auto tileWithPanel = new StrategyTileWithPanel(strategyID, name, symbols, isRunning, m_strategyManager);

    // Connect tile click signal
    [[maybe_unused]] auto tileConn = connect(tileWithPanel,
                                             &StrategyTileWithPanel::tileClicked,
                                             this,
                                             [this](const QString& id) { emit strategyTileClicked(id); });

    m_tiles[strategyID] = tileWithPanel;

    // Insert before the stretch item
    m_mainLayout->insertWidget(m_mainLayout->count() - 1, tileWithPanel);

    // If this is the first tile, select it
    if (m_selectedStrategyID.isEmpty())
    {
        setSelectedTile(strategyID);
    }
}

void StrategyGridWidget::updateStrategyTileStatus(const QString& strategyID,
                                                  bool isRunning,
                                                  const QString& errorMessage)
{
    if (m_tiles.contains(strategyID))
    {
        m_tiles[strategyID]->setStatus(isRunning, errorMessage);
    }
}

void StrategyGridWidget::removeStrategyTile(const QString& strategyID)
{
    if (m_tiles.contains(strategyID))
    {
        auto tile = m_tiles[strategyID];
        m_mainLayout->removeWidget(tile);
        delete tile;
        m_tiles.remove(strategyID);

        if (m_selectedStrategyID == strategyID)
        {
            m_selectedStrategyID = "";
            // Select first remaining tile if any
            if (!m_tiles.isEmpty())
            {
                setSelectedTile(m_tiles.first()->getStrategyID());
            }
        }
    }
}

void StrategyGridWidget::setSelectedTile(const QString& strategyID)
{
    // Deselect previous
    if (!m_selectedStrategyID.isEmpty() && m_tiles.contains(m_selectedStrategyID))
    {
        m_tiles[m_selectedStrategyID]->setSelected(false);
    }

    // Select new
    m_selectedStrategyID = strategyID;
    if (m_tiles.contains(strategyID))
    {
        m_tiles[strategyID]->setSelected(true);
    }
}

void StrategyGridWidget::clearTiles()
{
    for (auto tile: m_tiles)
    {
        delete tile;
    }
    m_tiles.clear();
    m_selectedStrategyID = "";
}
