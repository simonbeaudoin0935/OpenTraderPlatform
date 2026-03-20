#include "StrategyQuickView.h"

#include "MainAlgo.h"
#include "StrategyManager.h"
#include "StrategyLoadDialog.h"
#include "Position.h"
#include "Core/MainApp.h"
#include "Clients/DBClient/DBClient.h"

#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QMenu>
#include <QAction>
#include <QMessageBox>

// Column indices
static constexpr int COL_NAME = 0;
static constexpr int COL_QTY = 1;
static constexpr int COL_PRICE = 2;
static constexpr int COL_PNL = 3;  // Unrealized P&L
static constexpr int COL_RPNL = 4; // Realized P&L

StrategyQuickView::StrategyQuickView(QWidget* parent)
    : QWidget(parent), m_tree(new QTreeWidget(this)), m_positionTimer(new QTimer(this))
{
    setupUI();
    setupStyles();
    connect(m_positionTimer, &QTimer::timeout, this, &StrategyQuickView::onRefreshPositions);
}

void StrategyQuickView::setMainAlgo(MainAlgo* p_mainAlgo)
{
    m_mainAlgo = p_mainAlgo;
    if (m_mainAlgo)
        m_positionTimer->start(2000);
}

void StrategyQuickView::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header row: "Strategies" label + "Load ⊕" button
    auto* header = new QWidget(this);
    header->setStyleSheet("background-color: #2D2D2D; border-bottom: 1px solid #3D3D3D;");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 3, 4, 3);
    headerLayout->setSpacing(4);

    auto* titleLabel = new QLabel("Strategies", header);
    titleLabel->setStyleSheet("color: #AAAAAA; font-size: 11px; font-weight: bold;");
    headerLayout->addWidget(titleLabel, 1);

    auto* loadButton = new QPushButton("⊕ Load", header);
    loadButton->setFixedHeight(20);
    loadButton->setStyleSheet("QPushButton { background-color: #3A5A3A; color: #88DD88; border: 1px solid #4A7A4A;"
                              "              font-size: 10px; padding: 0 6px; border-radius: 2px; }"
                              "QPushButton:hover { background-color: #4A7A4A; }");
    loadButton->setToolTip("Load a strategy plugin");
    headerLayout->addWidget(loadButton);
    connect(loadButton, &QPushButton::clicked, this, &StrategyQuickView::onLoadButtonClicked);

    layout->addWidget(header);

    // Tree widget
    layout->addWidget(m_tree, 1);

    m_tree->setColumnCount(5);
    m_tree->setHeaderLabels({"Strategy / Symbol", "Qty", "Avg Price", "U/P&L", "R/P&L"});
    m_tree->setRootIsDecorated(true);
    m_tree->setExpandsOnDoubleClick(true);
    m_tree->setIndentation(14);
    m_tree->setAnimated(false);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Column widths — COL_NAME is elastic; P&L columns are pinned to equal fixed widths
    m_tree->header()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(COL_QTY, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_PRICE, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_PNL, QHeaderView::Fixed);
    m_tree->header()->setSectionResizeMode(COL_RPNL, QHeaderView::Fixed);
    m_tree->setColumnWidth(COL_PNL, 65);
    m_tree->setColumnWidth(COL_RPNL, 65);

    // Symbol click → display stock
    connect(m_tree,
            &QTreeWidget::itemClicked,
            this,
            [this](QTreeWidgetItem* item, int /*column*/)
            {
                // Only emit for symbol children (depth 1), not strategy roots (depth 0)
                if (item && item->parent())
                {
                    emit symbolSelected(item->text(COL_NAME));
                }
            });

    // Right-click context menu
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &StrategyQuickView::onContextMenuRequested);
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
    item->setText(COL_NAME, QString("⬤ %1").arg(name));
    item->setForeground(COL_NAME, QColor("#888888")); // grey dot = not yet running
    item->setExpanded(true);

    QFont font = item->font(COL_NAME);
    font.setBold(true);
    item->setFont(COL_NAME, font);

    m_strategyItems.insert(strategyID, item);
    m_strategyNames.insert(strategyID, name);
}

void StrategyQuickView::onStrategyUnloaded(const QString& strategyID)
{
    auto it = m_strategyItems.find(strategyID);
    if (it == m_strategyItems.end())
        return;

    delete it.value();
    m_strategyItems.erase(it);
    m_strategyNames.remove(strategyID);
}

void StrategyQuickView::onStrategyStatusChanged(const QString& strategyID,
                                                bool isRunning,
                                                const QString& /*errorMessage*/)
{
    auto it = m_strategyItems.find(strategyID);
    if (it == m_strategyItems.end())
        return;

    QTreeWidgetItem* item = it.value();
    item->setForeground(COL_NAME, QColor(isRunning ? "#00C800" : "#888888")); // green when running
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
        child->setText(COL_NAME, symbol);
        child->setForeground(COL_NAME, QColor("#80C8FF")); // light blue for symbols

        // Position columns start empty; filled by onRefreshPositions
        child->setText(COL_QTY, "—");
        child->setText(COL_PRICE, "—");
        child->setText(COL_PNL, "—");
        child->setText(COL_RPNL, "—");
        child->setTextAlignment(COL_QTY, Qt::AlignRight | Qt::AlignVCenter);
        child->setTextAlignment(COL_PRICE, Qt::AlignRight | Qt::AlignVCenter);
        child->setTextAlignment(COL_PNL, Qt::AlignRight | Qt::AlignVCenter);
        child->setTextAlignment(COL_RPNL, Qt::AlignRight | Qt::AlignVCenter);

        QFont font = child->font(COL_NAME);
        font.setBold(false);
        child->setFont(COL_NAME, font);
    }
}

void StrategyQuickView::onLoadButtonClicked()
{
    if (!m_mainAlgo)
        return;

    StrategyLoadDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        auto config = dialog.getSelectedConfig();
        if (config)
        {
            // In replay mode, reject strategies that reference symbols without recorded data
            // for the current replay date — they would silently receive no events otherwise.
            if (MainApp::isInReplayMode())
            {
                const QDate replayDate = DBClient::getReplayDate();
                QStringList missing;
                for (const QString& sym: config->symbols)
                {
                    if (!DBClient::hasReplayData(replayDate, sym))
                        missing << sym;
                }
                if (!missing.isEmpty())
                {
                    QMessageBox::warning(this,
                                         "No Replay Data for Strategy",
                                         QString("The following symbol(s) have no recorded replay data for %1:\n\n"
                                                 "  %2\n\n"
                                                 "Loading this strategy would result in no events being received.\n"
                                                 "Select a different replay date or download the data first.")
                                             .arg(replayDate.toString("yyyy-MM-dd"), missing.join(", ")));
                    return;
                }
            }

            auto result = m_mainAlgo->loadStrategy(*config);
            if (!result)
                QMessageBox::warning(this, "Load Strategy", result.error());
        }
    }
}

void StrategyQuickView::onRefreshPositions()
{
    if (!m_mainAlgo)
        return;

    for (auto it = m_strategyItems.constBegin(); it != m_strategyItems.constEnd(); ++it)
    {
        const QString& strategyID = it.key();
        QTreeWidgetItem* stratItem = it.value();

        QVector<Position> positions = m_mainAlgo->getStrategyOpenPositions(strategyID);

        // Build a symbol → Position map for fast lookup
        QMap<QString, const Position*> posMap;
        for (const Position& pos: positions)
            posMap.insert(pos.getSymbol(), &pos);

        for (int i = 0; i < stratItem->childCount(); ++i)
        {
            QTreeWidgetItem* child = stratItem->child(i);
            const QString symbol = child->text(COL_NAME);
            auto posIt = posMap.find(symbol);

            if (posIt != posMap.end())
            {
                const Position* pos = posIt.value();
                child->setText(COL_QTY, pos->getQuantity());
                child->setText(COL_PRICE, pos->getAveragePrice());

                QString pnl = pos->getUnrealizedProfitLoss();
                child->setText(COL_PNL, pnl);
                bool upnlPositive = !pnl.startsWith('-') && pnl != "—";
                child->setForeground(COL_PNL, QColor(upnlPositive ? "#00C800" : "#FF4444"));

                QString rpnl = pos->getTodaysProfitLoss();
                child->setText(COL_RPNL, rpnl);
                bool rpnlPositive = !rpnl.startsWith('-') && rpnl != "—";
                child->setForeground(COL_RPNL, QColor(rpnlPositive ? "#00C800" : "#FF4444"));
            }
            else
            {
                child->setText(COL_QTY, "—");
                child->setText(COL_PRICE, "—");
                child->setText(COL_PNL, "—");
                child->setForeground(COL_PNL, QColor("#888888"));
                child->setText(COL_RPNL, "—");
                child->setForeground(COL_RPNL, QColor("#888888"));
            }
        }
    }
}

void StrategyQuickView::onContextMenuRequested(const QPoint& pos)
{
    if (!m_mainAlgo)
        return;

    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item)
        return;

    // Resolve strategy ID for both root and child rows
    QString strategyID = strategyIDForItem(item);
    if (strategyID.isEmpty())
        return;

    const bool isRunning = m_mainAlgo->isStrategyRunning(strategyID);
    QString strategyName = m_strategyNames.value(strategyID, strategyID);

    QMenu menu(this);

    QAction* startAction = menu.addAction("▶  Start");
    startAction->setEnabled(!isRunning);

    QAction* stopAction = menu.addAction("■  Stop / Unload");
    stopAction->setEnabled(isRunning);

    menu.addSeparator();
    QAction* logsAction = menu.addAction("📋  Display Logs");

    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));

    if (chosen == startAction)
    {
        QString error = m_mainAlgo->startStrategy(strategyID);
        if (!error.isEmpty())
            QMessageBox::warning(this, "Start Strategy", error);
    }
    else if (chosen == stopAction)
    {
        QString error = m_mainAlgo->unloadStrategy(strategyID);
        if (!error.isEmpty())
            QMessageBox::warning(this, "Stop Strategy", error);
    }
    else if (chosen == logsAction)
    {
        emit displayLogsRequested(strategyID, strategyName);
    }
}

QString StrategyQuickView::strategyIDForItem(QTreeWidgetItem* item) const
{
    if (!item)
        return {};

    // If it's a root item, find its strategy ID directly
    if (!item->parent())
    {
        for (auto it = m_strategyItems.constBegin(); it != m_strategyItems.constEnd(); ++it)
        {
            if (it.value() == item)
                return it.key();
        }
        return {};
    }

    // If it's a symbol child, delegate to parent
    return strategyIDForItem(item->parent());
}
