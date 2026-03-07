#include "StrategyQuickView.h"

#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QFont>

StrategyQuickView::StrategyQuickView(QWidget* parent) : QWidget(parent), m_tree(new QTreeWidget(this))
{
    setupUI();
    setupStyles();
}

void StrategyQuickView::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tree);

    m_tree->setColumnCount(1);
    m_tree->setHeaderLabel("Strategies");
    m_tree->setRootIsDecorated(true);
    m_tree->setExpandsOnDoubleClick(true);
    m_tree->setIndentation(14);
    m_tree->setAnimated(false);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(m_tree,
            &QTreeWidget::itemClicked,
            this,
            [this](QTreeWidgetItem* item, int /*column*/)
            {
                // Only emit for symbol children (depth 1), not strategy roots (depth 0)
                if (item && item->parent())
                {
                    emit symbolSelected(item->text(0));
                }
            });
}

void StrategyQuickView::setupStyles()
{
    m_tree->setStyleSheet("QTreeWidget {"
                          "  background-color: #1C1C1C;"
                          "  color: #CCCCCC;"
                          "  border: none;"
                          "  font-size: 12px;"
                          "}"
                          "QTreeWidget::item {"
                          "  padding: 2px 4px;"
                          "  border: none;"
                          "}"
                          "QTreeWidget::item:hover {"
                          "  background-color: #2A2A2A;"
                          "}"
                          "QTreeWidget::item:selected {"
                          "  background-color: #2C539E;"
                          "  color: white;"
                          "}"
                          "QHeaderView::section {"
                          "  background-color: #2D2D2D;"
                          "  color: #AAAAAA;"
                          "  padding: 4px;"
                          "  border-bottom: 1px solid #3D3D3D;"
                          "  font-size: 11px;"
                          "  font-weight: bold;"
                          "}"
                          "QTreeWidget::branch:has-children:!has-siblings:closed,"
                          "QTreeWidget::branch:closed:has-children:has-siblings {"
                          "  border-image: none;"
                          "  image: none;"
                          "}"
                          "QTreeWidget::branch:open:has-children:!has-siblings,"
                          "QTreeWidget::branch:open:has-children:has-siblings {"
                          "  border-image: none;"
                          "  image: none;"
                          "}");
}

void StrategyQuickView::onStrategyLoaded(const QString& strategyID, const QString& name)
{
    if (m_strategyItems.contains(strategyID))
        return;

    auto* item = new QTreeWidgetItem(m_tree);
    item->setText(0, QString("⬤ %1").arg(name));
    item->setForeground(0, QColor("#888888")); // grey dot = not yet running
    item->setExpanded(true);

    QFont font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);

    m_strategyItems.insert(strategyID, item);
}

void StrategyQuickView::onStrategyUnloaded(const QString& strategyID)
{
    auto it = m_strategyItems.find(strategyID);
    if (it == m_strategyItems.end())
        return;

    delete it.value();
    m_strategyItems.erase(it);
}

void StrategyQuickView::onStrategyStatusChanged(const QString& strategyID,
                                                bool isRunning,
                                                const QString& /*errorMessage*/)
{
    auto it = m_strategyItems.find(strategyID);
    if (it == m_strategyItems.end())
        return;

    QTreeWidgetItem* item = it.value();
    item->setForeground(0, QColor(isRunning ? "#00C800" : "#888888")); // green when running
}

void StrategyQuickView::onSymbolsClaimed(const QString& strategyID, const QStringList& claimedSymbols)
{
    auto it = m_strategyItems.find(strategyID);
    if (it == m_strategyItems.end())
        return;

    updateSymbolChildren(it.value(), claimedSymbols);
}

void StrategyQuickView::updateSymbolChildren(QTreeWidgetItem* strategyItem, const QStringList& symbols)
{
    // Remove old symbol children
    while (strategyItem->childCount() > 0)
    {
        delete strategyItem->takeChild(0);
    }

    for (const QString& symbol: symbols)
    {
        auto* child = new QTreeWidgetItem(strategyItem);
        child->setText(0, symbol);
        child->setForeground(0, QColor("#80C8FF")); // light blue for symbols

        QFont font = child->font(0);
        font.setBold(false);
        child->setFont(0, font);
    }
}
