#include "StrategyGridWidget.h"
#include "StrategyTile.h"
#include "StrategyManager.h"

StrategyGridWidget::StrategyGridWidget(StrategyManager* p_strategyManager, QWidget* parent)
    : QWidget(parent), m_selectedStrategyID(""), m_strategyManager(p_strategyManager)
{
    setupUI();
}

void StrategyGridWidget::setupUI()
{
    m_gridLayout = new QGridLayout(this);
    m_gridLayout->setSpacing(10);
    m_gridLayout->setContentsMargins(10, 10, 10, 10);
    m_gridLayout->setColumnStretch(0, 1);
    m_gridLayout->setColumnStretch(1, 1);
    m_gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding), 100, 0, 1, 2);
}

void StrategyGridWidget::addStrategyTile(const QString& strategyID,
                                         const QString& name,
                                         const QVector<QString>& symbols,
                                         bool isRunning)
{
    auto tile = new StrategyTile(strategyID, name, symbols, isRunning, m_strategyManager);

    [[maybe_unused]] auto tileConn =
        connect(tile, &StrategyTile::tileClicked, this, [this](const QString& id) { emit strategyTileClicked(id); });

    m_tiles[strategyID] = tile;

    // Add to grid (2 columns, wrapping layout)
    int tileCount = m_tiles.size() - 1; // Current count before this addition
    int row = tileCount / 2;
    int col = tileCount % 2;
    m_gridLayout->addWidget(tile, row, col);

    if (m_selectedStrategyID == strategyID)
    {
        tile->setSelected(true);
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
        delete m_tiles[strategyID];
        m_tiles.remove(strategyID);

        if (m_selectedStrategyID == strategyID)
        {
            m_selectedStrategyID = "";
        }

        // Rebuild grid to reflow remaining tiles
        reflowTiles();
    }
}

void StrategyGridWidget::reflowTiles()
{
    // Clear all items from layout (except spacer at row 100)
    while (m_gridLayout->count() > 0)
    {
        auto item = m_gridLayout->itemAt(0);
        if (item->widget())
        {
            m_gridLayout->removeWidget(item->widget());
        }
        else
        {
            m_gridLayout->removeItem(item);
        }
    }

    // Re-add spacer
    m_gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding), 100, 0, 1, 2);

    // Re-add all tiles in order
    int tileIndex = 0;
    for (const auto& tile: m_tiles)
    {
        int row = tileIndex / 2;
        int col = tileIndex % 2;
        m_gridLayout->addWidget(tile, row, col);
        tileIndex++;
    }
}

void StrategyGridWidget::setSelectedTile(const QString& strategyID)
{
    if (m_selectedStrategyID != strategyID)
    {
        // Deselect old
        if (m_tiles.contains(m_selectedStrategyID))
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
