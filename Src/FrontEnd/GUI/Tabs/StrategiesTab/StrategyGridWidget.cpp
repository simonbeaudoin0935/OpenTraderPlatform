#include "StrategyGridWidget.h"
#include "StrategyCard.h"
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

    // Add stretchable space at the end to push cards to the left
    m_mainLayout->addStretch();
}

void StrategyGridWidget::addStrategyCard(const QString& strategyID,
                                         const QString& name,
                                         const QVector<QString>& symbols,
                                         bool isRunning)
{
    auto card = new StrategyCard(strategyID, name, symbols, isRunning, m_strategyManager);

    // Connect card click signal
    [[maybe_unused]] auto cardConn =
        connect(card, &StrategyCard::cardClicked, this, [this](const QString& id) { emit strategyCardClicked(id); });

    m_cards[strategyID] = card;

    // Insert before the stretch item
    m_mainLayout->insertWidget(m_mainLayout->count() - 1, card);

    // If this is the first card, select it
    if (m_selectedStrategyID.isEmpty())
    {
        setSelectedCard(strategyID);
    }
}

void StrategyGridWidget::updateStrategyCardStatus(const QString& strategyID,
                                                  bool isRunning,
                                                  const QString& errorMessage)
{
    if (m_cards.contains(strategyID))
    {
        m_cards[strategyID]->setStatus(isRunning, errorMessage);
    }
}

void StrategyGridWidget::removeStrategyCard(const QString& strategyID)
{
    if (m_cards.contains(strategyID))
    {
        auto card = m_cards[strategyID];
        m_mainLayout->removeWidget(card);
        delete card;
        m_cards.remove(strategyID);

        if (m_selectedStrategyID == strategyID)
        {
            m_selectedStrategyID = "";
            // Select first remaining card if any
            if (!m_cards.isEmpty())
            {
                setSelectedCard(m_cards.first()->getStrategyID());
            }
        }
    }
}

void StrategyGridWidget::setSelectedCard(const QString& strategyID)
{
    // Deselect previous
    if (!m_selectedStrategyID.isEmpty() && m_cards.contains(m_selectedStrategyID))
    {
        // Could add visual feedback here if desired
    }

    // Select new
    m_selectedStrategyID = strategyID;
    if (m_cards.contains(strategyID))
    {
        // Could add visual feedback here if desired
    }
}

void StrategyGridWidget::clearCards()
{
    for (auto card: m_cards)
    {
        delete card;
    }
    m_cards.clear();
    m_selectedStrategyID = "";
}
