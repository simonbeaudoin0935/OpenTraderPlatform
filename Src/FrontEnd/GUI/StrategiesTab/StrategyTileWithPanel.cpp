#include "StrategyTileWithPanel.h"
#include "StrategyTile.h"
#include "StrategyDetailsPanel.h"
#include "StrategyManager.h"

#include <QVBoxLayout>
#include <QMouseEvent>

StrategyTileWithPanel::StrategyTileWithPanel(const QString& strategyID,
                                             const QString& name,
                                             const QVector<QString>& symbols,
                                             bool isRunning,
                                             StrategyManager* p_strategyManager,
                                             QWidget* parent)
    : QWidget(parent)
    , m_strategyID(strategyID)
    , m_isSelected(false)
    , m_tile(new StrategyTile(strategyID, name, symbols, isRunning, p_strategyManager, this))
    , m_panel(new StrategyDetailsPanel(p_strategyManager, this))
{
    // Set fixed width (400px)
    setFixedWidth(400);

    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(5);

    // Add tile (top)
    m_mainLayout->addWidget(m_tile);

    // Add panel (bottom) - maximize space for logs
    m_panel->setMinimumHeight(500);
    m_mainLayout->addWidget(m_panel, 1); // stretch to fill available space

    // Set the strategy in the panel
    m_panel->setStrategy(strategyID);

    // Connect tile click signal to our signal
    connect(m_tile, &StrategyTile::tileClicked, this, &StrategyTileWithPanel::tileClicked, Qt::QueuedConnection);
}

void StrategyTileWithPanel::setSelected(bool selected)
{
    m_isSelected = selected;
    // No visual feedback - panels always visible for context
}

void StrategyTileWithPanel::setStatus(bool isRunning, const QString& errorMessage)
{
    m_tile->setStatus(isRunning, errorMessage);
}

void StrategyTileWithPanel::refreshDisplay()
{
    m_tile->refreshDisplay();
}

QString StrategyTileWithPanel::getStrategyID() const
{
    return m_strategyID;
}

void StrategyTileWithPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        setSelected(true);
        emit tileClicked(m_strategyID);
    }
    QWidget::mousePressEvent(event);
}
