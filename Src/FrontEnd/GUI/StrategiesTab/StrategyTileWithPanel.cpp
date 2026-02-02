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
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    // Add tile (top)
    m_mainLayout->addWidget(m_tile);

    // Add panel (bottom) with fixed height
    m_panel->setFixedHeight(300);
    m_mainLayout->addWidget(m_panel);

    // Set the strategy in the panel
    m_panel->setStrategy(strategyID);

    // Connect tile click signal to our signal
    connect(m_tile, &StrategyTile::tileClicked, this, &StrategyTileWithPanel::tileClicked, Qt::QueuedConnection);
}

void StrategyTileWithPanel::setSelected(bool selected)
{
    m_isSelected = selected;

    // Visual feedback: subtle border on selection
    if (selected)
    {
        setStyleSheet("QWidget { border: 2px solid #0078d4; border-radius: 4px; }");
    }
    else
    {
        setStyleSheet("QWidget { border: 1px solid #cccccc; border-radius: 4px; }");
    }

    m_tile->setSelected(selected);
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
