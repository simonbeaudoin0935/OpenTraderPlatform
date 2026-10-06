#include "StrategyQuickView.h"

#include "MainAlgo.h"
#include "StrategyManager.h"
#include "StrategyLoadDialog.h"
#include "Position.h"
#include "Core/MainApp.h"
#include "Misc/Logging/Logging.h"

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
#include <QPointer>
#include <QShortcut>
#include "CONSTANTS.h"
#include <algorithm>
#include <utility>

// Column indices
static constexpr int COL_NAME = 0;
static constexpr int COL_QTY = 1;
static constexpr int COL_PRICE = 2;
static constexpr int COL_PNL = 3;  // Unrealized P&L
static constexpr int COL_RPNL = 4; // Realized P&L
static constexpr int TREE_INDENTATION = 8;
static constexpr int AVG_PRICE_COLUMN_WIDTH = 54;
static constexpr int PNL_COLUMN_WIDTH = 65;
static constexpr int RPNL_COLUMN_WIDTH = 65;

namespace
{
    [[nodiscard]] QString formatPnlValue(const double p_value)
    {
        return QString::number(p_value, 'f', 2);
    }

    [[nodiscard]] QColor strategyStateColor(const StrategyManager::StrategyExecutionState p_state)
    {
        switch (p_state)
        {
        case StrategyManager::StrategyExecutionState::Running:
            return QColor("#00C800");
        case StrategyManager::StrategyExecutionState::Primed:
        case StrategyManager::StrategyExecutionState::Paused:
            return QColor("#E5C100");
        case StrategyManager::StrategyExecutionState::Loaded:
        case StrategyManager::StrategyExecutionState::Stopped:
        default:
            return QColor("#888888");
        }
    }
} // namespace

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
    if (m_mainAlgo && m_displayMode == DisplayMode::Strategies)
        m_positionTimer->start(2000);
}

void StrategyQuickView::setReviewModeEnabled(const bool p_enabled)
{
    m_displayMode = p_enabled ? DisplayMode::ReviewSymbols : DisplayMode::Strategies;
    m_tree->clear();
    m_strategyItems.clear();
    m_strategyNames.clear();
    m_symbolItems.clear();
    m_strategyStates.clear();
    m_strategyStateRefreshTokens.clear();

    if (m_titleLabel)
    {
        m_titleLabel->setText(p_enabled ? "Review Symbols" : "Strategies");
    }
    if (m_loadButton)
    {
        m_loadButton->setVisible(!p_enabled);
        m_loadButton->setEnabled(!p_enabled);
    }

    if (p_enabled)
    {
        m_positionTimer->stop();
    }
    else if (m_mainAlgo)
    {
        m_positionTimer->start(2000);
    }
}

void StrategyQuickView::setReviewSymbols(const QStringList& p_symbols)
{
    if (m_displayMode != DisplayMode::ReviewSymbols)
    {
        return;
    }

    m_tree->clear();
    auto* root = new QTreeWidgetItem(m_tree);
    root->setText(COL_NAME, "Loaded Review Session");
    root->setFirstColumnSpanned(true);
    root->setExpanded(true);

    QFont font = root->font(COL_NAME);
    font.setBold(true);
    root->setFont(COL_NAME, font);

    QStringList symbols = p_symbols;
    symbols.removeDuplicates();
    std::sort(symbols.begin(), symbols.end());

    updateSymbolChildren(root, symbols);
}

void StrategyQuickView::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header row: "Strategies" label + "Load ⊕" button
    auto* header = new QWidget(this);
    header->setStyleSheet(QStringLiteral("background-color: %1; border-bottom: 1px solid %2;")
                              .arg(QString::fromLatin1(GUIThemeConstants::SIDEBAR_BACKGROUND))
                              .arg(QString::fromLatin1(GUIThemeConstants::BORDER)));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 3, 4, 3);
    headerLayout->setSpacing(4);

    m_titleLabel = new QLabel("Strategies", header);
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; font-weight: bold;")
                                    .arg(QString::fromLatin1(GUIThemeConstants::TEXT_MUTED)));
    headerLayout->addWidget(m_titleLabel, 1);

    m_loadButton = new QPushButton("⊕ Load", header);
    m_loadButton->setFixedHeight(20);
    m_loadButton->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; color: #ffffff; border: 1px solid %2;"
                       " font-size: 10px; padding: 0 6px; border-radius: 2px; }"
                       "QPushButton:hover { background-color: %3; }")
            .arg(QString::fromLatin1(GUIThemeConstants::ACCENT))
            .arg(QString::fromLatin1(GUIThemeConstants::ACCENT))
            .arg(QString::fromLatin1(GUIThemeConstants::SELECTION_BACKGROUND)));
    m_loadButton->setToolTip("Load a strategy executable");
    headerLayout->addWidget(m_loadButton);
    connect(m_loadButton, &QPushButton::clicked, this, &StrategyQuickView::onLoadButtonClicked);

    layout->addWidget(header);

    // Tree widget
    layout->addWidget(m_tree, 1);

    m_tree->setColumnCount(5);
    m_tree->setHeaderLabels({"Strategy / Symbol", "Qty", "Avg Price", "U/P&L", "R/P&L"});
    m_tree->setRootIsDecorated(true);
    m_tree->setExpandsOnDoubleClick(true);
    m_tree->setIndentation(TREE_INDENTATION);
    m_tree->setAnimated(false);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Column widths — keep numeric columns compact so Strategy / Symbol gets the spare width.
    m_tree->header()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(COL_QTY, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_PRICE, QHeaderView::Fixed);
    m_tree->header()->setSectionResizeMode(COL_PNL, QHeaderView::Fixed);
    m_tree->header()->setSectionResizeMode(COL_RPNL, QHeaderView::Fixed);
    m_tree->setColumnWidth(COL_PRICE, AVG_PRICE_COLUMN_WIDTH);
    m_tree->setColumnWidth(COL_PNL, PNL_COLUMN_WIDTH);
    m_tree->setColumnWidth(COL_RPNL, RPNL_COLUMN_WIDTH);

    // Symbol click → display stock
    connect(
        m_tree,
        &QTreeWidget::itemClicked,
        this,
        [this](QTreeWidgetItem* item, int /*column*/)
        {
            // Only emit for symbol children (depth 1), not strategy roots (depth 0)
            if (item && item->parent())
            {
                logInputEvent(u"StrategyQuickView", u"select-symbol", {inputDetail(u"symbol", item->text(COL_NAME))});
                emit symbolSelected(item->text(COL_NAME));
            }
        });

    // Right-click context menu
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &StrategyQuickView::onContextMenuRequested);

    auto* deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), m_tree);
    connect(deleteShortcut, &QShortcut::activated, this, &StrategyQuickView::onDeleteSymbolShortcut);
}

void StrategyQuickView::setupStyles()
{
    m_tree->setStyleSheet("QTreeWidget::item {"
                          "  padding: 2px 2px;"
                          "  border: none;"
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
    if (m_displayMode == DisplayMode::ReviewSymbols)
        return;

    if (m_strategyItems.contains(strategyID))
        return;

    auto* item = new QTreeWidgetItem(m_tree);
    item->setText(COL_NAME, QString("⬤ %1").arg(name));
    item->setFirstColumnSpanned(true);
    item->setForeground(COL_NAME, QColor("#888888")); // grey dot = not yet running
    item->setExpanded(true);

    QFont font = item->font(COL_NAME);
    font.setBold(true);
    item->setFont(COL_NAME, font);

    m_strategyItems.insert(strategyID, item);
    m_strategyNames.insert(strategyID, name);
    m_symbolItems.insert(strategyID, {});
    m_strategyStates.insert(strategyID, StrategyManager::StrategyExecutionState::Loaded);
    m_strategyStateRefreshTokens.remove(strategyID);
}

void StrategyQuickView::clearStrategies()
{
    m_tree->clear();
    m_strategyItems.clear();
    m_strategyNames.clear();
    m_symbolItems.clear();
    m_strategyStates.clear();
    m_strategyStateRefreshTokens.clear();
    qDebug() << "StrategyQuickView cleared all strategies";
}

void StrategyQuickView::onStrategyUnloaded(const QString& strategyID)
{
    if (m_displayMode == DisplayMode::ReviewSymbols)
        return;

    auto it = m_strategyItems.find(strategyID);
    if (it == m_strategyItems.end())
        return;

    delete it.value();
    m_strategyItems.erase(it);
    m_strategyNames.remove(strategyID);
    m_symbolItems.remove(strategyID);
    m_strategyStates.remove(strategyID);
    m_strategyStateRefreshTokens.remove(strategyID);
}

void StrategyQuickView::onStrategyStatusChanged(const QString& strategyID,
                                                const StrategyManager::StrategyExecutionState state,
                                                const QString& errorMessage)
{
    if (m_displayMode == DisplayMode::ReviewSymbols)
        return;

    auto it = m_strategyItems.find(strategyID);
    if (it == m_strategyItems.end())
        return;

    QTreeWidgetItem* item = it.value();
    item->setForeground(COL_NAME, strategyStateColor(state));
    item->setToolTip(COL_NAME, errorMessage);
    m_strategyStates.insert(strategyID, state);

    if (state == StrategyManager::StrategyExecutionState::Stopped && !errorMessage.isEmpty())
    {
        const QString strategyName = m_strategyNames.value(strategyID, strategyID);
        QMessageBox::critical(this,
                              "Strategy Failed",
                              QString("Strategy \"%1\" failed.\n\nReason:\n%2\n\n"
                                      "Check the strategy log for more details.")
                                  .arg(strategyName, errorMessage));
    }
}

void StrategyQuickView::onSymbolsClaimed(const QString& strategyID, const QStringList& claimedSymbols)
{
    if (m_displayMode == DisplayMode::ReviewSymbols)
        return;

    auto it = m_strategyItems.find(strategyID);
    if (it == m_strategyItems.end())
        return;

    updateSymbolChildren(it.value(), claimedSymbols);
}

void StrategyQuickView::updateSymbolChildren(QTreeWidgetItem* strategyItem, const QStringList& symbols)
{
    const QString strategyID = strategyIDForItem(strategyItem);

    // Remove old symbol children
    while (strategyItem->childCount() > 0)
    {
        delete strategyItem->takeChild(0);
    }

    m_symbolItems[strategyID].clear();

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

        m_symbolItems[strategyID].insert(symbol, child);
    }
}

void StrategyQuickView::onLoadButtonClicked()
{
    if (m_displayMode == DisplayMode::ReviewSymbols)
        return;

    if (!m_mainAlgo)
        return;

    logInputEvent(u"StrategyQuickView", u"open-load-strategy-dialog");
    StrategyLoadDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        auto config = dialog.getSelectedConfig();
        if (config)
        {
            logInputEvent(u"StrategyQuickView",
                          u"load-strategy-requested",
                          {inputDetail(u"name", config->name), inputDetail(u"path", config->executablePath)});
            const StrategyConfig strategyConfig = *config;
            QPointer<StrategyQuickView> view(this);
            const bool invoked = QMetaObject::invokeMethod(
                m_mainAlgo,
                [algo = m_mainAlgo, view, strategyConfig]()
                {
                    ASSUME_DIFF(algo, nullptr);
                    if (view.isNull())
                    {
                        return;
                    }

                    const std::expected<QString, QString> result = algo->loadStrategy(strategyConfig);
                    const QString error = result.has_value() ? QString() : result.error();
                    QMetaObject::invokeMethod(
                        view.data(),
                        [view, error]()
                        {
                            if (view.isNull() || error.isEmpty())
                            {
                                return;
                            }
                            QMessageBox::warning(view, "Load Strategy", error);
                        },
                        Qt::QueuedConnection);
                },
                Qt::QueuedConnection);
            ASSUME_TRUE(invoked);
        }
    }
}

void StrategyQuickView::onRefreshPositions()
{
    if (m_displayMode == DisplayMode::ReviewSymbols)
        return;

    if (!m_mainAlgo)
        return;

    for (auto it = m_strategyItems.constBegin(); it != m_strategyItems.constEnd(); ++it)
    {
        requestStrategySymbolStates(it.key());
    }
}

void StrategyQuickView::requestStrategySymbolStates(const QString& p_strategyID)
{
    if (!m_mainAlgo)
    {
        return;
    }
    if (!m_strategyItems.contains(p_strategyID))
    {
        return;
    }

    const quint64 requestToken = m_strategyStateRefreshTokens.value(p_strategyID, 0) + 1;
    m_strategyStateRefreshTokens.insert(p_strategyID, requestToken);

    QPointer<StrategyQuickView> view(this);
    const bool invoked = QMetaObject::invokeMethod(
        m_mainAlgo,
        [algo = m_mainAlgo, view, p_strategyID, requestToken]()
        {
            ASSUME_DIFF(algo, nullptr);
            if (view.isNull())
            {
                return;
            }

            const QVector<StrategySymbolViewState> states = algo->getStrategySymbolViewStates(p_strategyID);
            QMetaObject::invokeMethod(
                view.data(),
                [view, p_strategyID, requestToken, states]()
                {
                    if (view.isNull())
                    {
                        return;
                    }

                    if (view->m_strategyStateRefreshTokens.value(p_strategyID, 0) != requestToken)
                    {
                        return;
                    }

                    view->applyStrategySymbolStates(p_strategyID, states);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);
}

void StrategyQuickView::applyStrategySymbolStates(const QString& p_strategyID,
                                                  const QVector<StrategySymbolViewState>& p_states)
{
    if (!m_strategyItems.contains(p_strategyID))
    {
        return;
    }
    const auto symbolItemsIt = m_symbolItems.constFind(p_strategyID);
    if (symbolItemsIt == m_symbolItems.constEnd())
    {
        return;
    }

    const QMap<QString, QTreeWidgetItem*>& symbolItems = symbolItemsIt.value();

    QMap<QString, StrategySymbolViewState> stateMap;
    for (const StrategySymbolViewState& state: p_states)
    {
        stateMap.insert(state.symbol, state);
    }

    for (auto childIt = symbolItems.constBegin(); childIt != symbolItems.constEnd(); ++childIt)
    {
        QTreeWidgetItem* child = childIt.value();
        const QString& symbol = childIt.key();
        auto stateIt = stateMap.find(symbol);

        if (stateIt == stateMap.end())
        {
            child->setText(COL_QTY, "—");
            child->setText(COL_PRICE, "—");
            child->setText(COL_PNL, "—");
            child->setForeground(COL_PNL, QColor("#888888"));
            child->setText(COL_RPNL, "—");
            child->setForeground(COL_RPNL, QColor("#888888"));
            continue;
        }

        const StrategySymbolViewState& state = stateIt.value();
        if (!state.hasTradeHistory)
        {
            child->setText(COL_QTY, "—");
            child->setText(COL_PRICE, "—");
            child->setText(COL_PNL, "—");
            child->setForeground(COL_PNL, QColor("#888888"));
            child->setText(COL_RPNL, "—");
            child->setForeground(COL_RPNL, QColor("#888888"));
            continue;
        }

        if (state.openQuantity != 0)
        {
            child->setText(COL_QTY, QString::number(state.openQuantity));
            child->setText(COL_PRICE, QString::number(state.averagePrice, 'f', 2));
            const QString upnl = formatPnlValue(state.unrealizedPnl);
            child->setText(COL_PNL, upnl);
            child->setForeground(COL_PNL, QColor(state.unrealizedPnl >= 0.0 ? "#00C800" : "#FF4444"));
        }
        else
        {
            child->setText(COL_QTY, "0");
            child->setText(COL_PRICE, "—");
            child->setText(COL_PNL, "0.00");
            child->setForeground(COL_PNL, QColor("#888888"));
        }

        const QString rpnl = formatPnlValue(state.realizedPnl);
        child->setText(COL_RPNL, rpnl);
        child->setForeground(COL_RPNL,
                             QColor(state.realizedPnl > 0.0   ? "#00C800"
                                    : state.realizedPnl < 0.0 ? "#FF4444"
                                                              : "#888888"));
    }
}

void StrategyQuickView::dispatchMainAlgoStringCommand(const QString& p_dialogTitle,
                                                      std::function<QString(MainAlgo*)> p_command)
{
    if (!m_mainAlgo)
    {
        return;
    }

    QPointer<StrategyQuickView> view(this);
    const bool invoked = QMetaObject::invokeMethod(
        m_mainAlgo,
        [algo = m_mainAlgo, view, p_dialogTitle, p_command = std::move(p_command)]() mutable
        {
            ASSUME_DIFF(algo, nullptr);
            if (view.isNull())
            {
                return;
            }

            const QString error = p_command(algo);
            QMetaObject::invokeMethod(
                view.data(),
                [view, p_dialogTitle, error]()
                {
                    if (view.isNull() || error.isEmpty())
                    {
                        return;
                    }

                    QMessageBox::warning(view, p_dialogTitle, error);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);
}

void StrategyQuickView::requestSymbolUnclaim(const QString& p_strategyID,
                                             const QString& p_strategyName,
                                             const QString& p_symbol,
                                             const bool p_blockForSession)
{
    if (!m_mainAlgo)
    {
        return;
    }

    const QString normalizedSymbol = p_symbol.trimmed().toUpper();
    if (normalizedSymbol.isEmpty())
    {
        return;
    }

    logInputEvent(u"StrategyQuickView",
                  p_blockForSession ? u"unclaim-symbol-and-block" : u"unclaim-symbol",
                  {inputDetail(u"strategyId", p_strategyID),
                   inputDetail(u"name", p_strategyName),
                   inputDetail(u"symbol", normalizedSymbol)});

    dispatchMainAlgoStringCommand(
        "Unclaim Symbol",
        [p_strategyID, normalizedSymbol, p_blockForSession](MainAlgo* p_mainAlgo)
        {
            return p_blockForSession ? p_mainAlgo->unclaimAndBlockStrategySymbol(p_strategyID, normalizedSymbol)
                                     : p_mainAlgo->unclaimStrategySymbol(p_strategyID, normalizedSymbol);
        });
}

void StrategyQuickView::onDeleteSymbolShortcut()
{
    if (m_displayMode == DisplayMode::ReviewSymbols)
        return;

    if (!m_mainAlgo)
        return;

    QTreeWidgetItem* item = m_tree->currentItem();
    if (item == nullptr || item->parent() == nullptr)
    {
        return;
    }

    const QString strategyID = strategyIDForItem(item);
    if (strategyID.isEmpty())
    {
        return;
    }

    const QString strategyName = m_strategyNames.value(strategyID, strategyID);
    const QString symbol = item->text(COL_NAME).trimmed().toUpper();
    if (symbol.isEmpty())
    {
        return;
    }

    logInputEvent(
        u"StrategyQuickView",
        u"delete-unclaim-symbol-shortcut",
        {inputDetail(u"strategyId", strategyID), inputDetail(u"name", strategyName), inputDetail(u"symbol", symbol)});
    requestSymbolUnclaim(strategyID, strategyName, symbol, false);
}

void StrategyQuickView::onContextMenuRequested(const QPoint& pos)
{
    if (m_displayMode == DisplayMode::ReviewSymbols)
        return;

    if (!m_mainAlgo)
        return;

    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item)
        return;

    // Resolve strategy ID for both root and child rows
    QString strategyID = strategyIDForItem(item);
    if (strategyID.isEmpty())
        return;

    const StrategyManager::StrategyExecutionState state =
        m_strategyStates.value(strategyID, StrategyManager::StrategyExecutionState::Stopped);
    QString strategyName = m_strategyNames.value(strategyID, strategyID);

    if (item->parent())
    {
        const QString symbol = item->text(COL_NAME).trimmed().toUpper();
        if (symbol.isEmpty())
        {
            return;
        }

        QMenu symbolMenu(this);
        QAction* unclaimAction = symbolMenu.addAction("🗑  Unclaim Now");
        QAction* unclaimAndBlockAction = symbolMenu.addAction("⛔  Unclaim & Block (Session)");
        QAction* chosenSymbolAction = symbolMenu.exec(m_tree->viewport()->mapToGlobal(pos));

        if (chosenSymbolAction == unclaimAction)
        {
            requestSymbolUnclaim(strategyID, strategyName, symbol, false);
        }
        else if (chosenSymbolAction == unclaimAndBlockAction)
        {
            requestSymbolUnclaim(strategyID, strategyName, symbol, true);
        }
        return;
    }

    QMenu menu(this);

    QAction* startAction = menu.addAction("▶  Start");
    startAction->setEnabled(state == StrategyManager::StrategyExecutionState::Loaded ||
                            state == StrategyManager::StrategyExecutionState::Stopped);

    QAction* stopAction = menu.addAction("■  Stop");
    stopAction->setEnabled(state == StrategyManager::StrategyExecutionState::Primed ||
                           state == StrategyManager::StrategyExecutionState::Running ||
                           state == StrategyManager::StrategyExecutionState::Paused);

    QAction* editAction = menu.addAction("✎  Edit Parameters");
    editAction->setEnabled(state == StrategyManager::StrategyExecutionState::Loaded ||
                           state == StrategyManager::StrategyExecutionState::Stopped);

    QAction* unloadAction = menu.addAction("⏏  Unload");
    unloadAction->setEnabled(true);

    menu.addSeparator();
    QAction* logsAction = menu.addAction("📋  Display Logs");

    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));

    if (chosen == startAction)
    {
        logInputEvent(u"StrategyQuickView",
                      u"start-strategy",
                      {inputDetail(u"strategyId", strategyID), inputDetail(u"name", strategyName)});
        dispatchMainAlgoStringCommand("Start Strategy",
                                      [strategyID](MainAlgo* algo) { return algo->startStrategy(strategyID); });
    }
    else if (chosen == stopAction)
    {
        QPointer<StrategyQuickView> view(this);
        const bool invoked = QMetaObject::invokeMethod(
            m_mainAlgo,
            [algo = m_mainAlgo, view, strategyID, strategyName]()
            {
                ASSUME_DIFF(algo, nullptr);
                if (view.isNull())
                {
                    return;
                }

                const int openPositionCount = algo->getStrategyPositionCount(strategyID);
                const int pendingConfirmationCount = algo->getPendingManualOrderConfirmationsForStrategy(strategyID);
                QMetaObject::invokeMethod(
                    view.data(),
                    [view, strategyID, strategyName, openPositionCount, pendingConfirmationCount]()
                    {
                        if (view.isNull())
                        {
                            return;
                        }

                        if (openPositionCount > 0 || pendingConfirmationCount > 0)
                        {
                            QStringList warnings;
                            if (openPositionCount > 0)
                            {
                                warnings << QString("• %1 open position%2")
                                                .arg(openPositionCount)
                                                .arg(openPositionCount == 1 ? "" : "s");
                            }
                            if (pendingConfirmationCount > 0)
                            {
                                warnings << QString("• %1 pending manual confirmation request%2 (will be cancelled)")
                                                .arg(pendingConfirmationCount)
                                                .arg(pendingConfirmationCount == 1 ? "" : "s");
                            }

                            const auto response =
                                QMessageBox::warning(view,
                                                     "Stop Strategy",
                                                     QString("Strategy \"%1\" still has:\n%2\n\nStop anyway?")
                                                         .arg(strategyName, warnings.join('\n')),
                                                     QMessageBox::Yes | QMessageBox::No,
                                                     QMessageBox::No);
                            if (response != QMessageBox::Yes)
                            {
                                logInputEvent(
                                    u"StrategyQuickView",
                                    u"stop-strategy-cancelled",
                                    {inputDetail(u"strategyId", strategyID), inputDetail(u"name", strategyName)});
                                return;
                            }
                        }

                        logInputEvent(u"StrategyQuickView",
                                      u"stop-strategy",
                                      {inputDetail(u"strategyId", strategyID), inputDetail(u"name", strategyName)});
                        view->dispatchMainAlgoStringCommand("Stop Strategy",
                                                            [strategyID](MainAlgo* p_mainAlgo)
                                                            { return p_mainAlgo->stopStrategy(strategyID); });
                    },
                    Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
        ASSUME_TRUE(invoked);
    }
    else if (chosen == editAction)
    {
        QPointer<StrategyQuickView> view(this);
        const bool invoked = QMetaObject::invokeMethod(
            m_mainAlgo,
            [algo = m_mainAlgo, view, strategyID, strategyName]()
            {
                ASSUME_DIFF(algo, nullptr);
                if (view.isNull())
                {
                    return;
                }

                const StrategyConfig currentConfig = algo->getStrategyConfig(strategyID);
                QMetaObject::invokeMethod(
                    view.data(),
                    [view, strategyID, strategyName, currentConfig]()
                    {
                        if (view.isNull())
                        {
                            return;
                        }

                        if (currentConfig.executablePath.trimmed().isEmpty())
                        {
                            QMessageBox::warning(view,
                                                 "Edit Strategy Parameters",
                                                 "Unable to load current strategy configuration.");
                            return;
                        }

                        logInputEvent(u"StrategyQuickView",
                                      u"edit-strategy-parameters-opened",
                                      {inputDetail(u"strategyId", strategyID), inputDetail(u"name", strategyName)});
                        StrategyLoadDialog dialog(currentConfig, view);
                        if (dialog.exec() != QDialog::Accepted)
                        {
                            return;
                        }

                        const std::optional<StrategyConfig> updatedConfig = dialog.getSelectedConfig();
                        if (!updatedConfig.has_value())
                        {
                            QMessageBox::warning(view,
                                                 "Edit Strategy Parameters",
                                                 "No updated strategy configuration was returned.");
                            return;
                        }

                        logInputEvent(u"StrategyQuickView",
                                      u"edit-strategy-parameters-apply",
                                      {inputDetail(u"strategyId", strategyID), inputDetail(u"name", strategyName)});
                        view->dispatchMainAlgoStringCommand(
                            "Edit Strategy Parameters",
                            [strategyID, updatedConfig = updatedConfig.value()](MainAlgo* p_mainAlgo)
                            { return p_mainAlgo->updateStrategyConfig(strategyID, updatedConfig); });
                    },
                    Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
        ASSUME_TRUE(invoked);
    }
    else if (chosen == unloadAction)
    {
        logInputEvent(u"StrategyQuickView",
                      u"unload-strategy",
                      {inputDetail(u"strategyId", strategyID), inputDetail(u"name", strategyName)});
        dispatchMainAlgoStringCommand("Unload Strategy",
                                      [strategyID](MainAlgo* algo) { return algo->unloadStrategy(strategyID); });
    }
    else if (chosen == logsAction)
    {
        logInputEvent(u"StrategyQuickView",
                      u"display-strategy-logs",
                      {inputDetail(u"strategyId", strategyID), inputDetail(u"name", strategyName)});
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
