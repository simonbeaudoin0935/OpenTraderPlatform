#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QMap>
#include <QStringList>
#include <QTimer>
#include <functional>

#include "StrategyManager.h"

class MainAlgo;
class QLabel;
class QPushButton;

/// @brief Compact live view of active strategies and their claimed symbols.
/// Displays a collapsible tree: strategy nodes (root) → symbol nodes (children).
///
/// Header row contains a "Load ⊕" button for loading new strategies.
/// Symbol rows show live position columns: Qty | Avg Price | P&L.
/// Clicking a symbol node emits symbolSelected(symbol) → GUIFrontend::displayStock().
///
/// Right-clicking a strategy row opens a context menu: Start / Stop / Edit / Unload / Display Logs.
/// Right-clicking a symbol row passes up to the parent strategy context menu.
///
/// Updated via StrategyManager signals: strategyLoaded, strategyUnloaded,
/// strategyStatusChanged, symbolsClaimed.
class StrategyQuickView : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyQuickView(QWidget* parent = nullptr);
    ~StrategyQuickView() override = default;

    /// @brief Provide access to MainAlgo for position data and context-menu actions.
    void setMainAlgo(MainAlgo* p_mainAlgo);
    void setReviewModeEnabled(bool p_enabled);
    void setReviewSymbols(const QStringList& p_symbols);

  signals:
    /// @brief Emitted when user clicks a symbol node in the tree.
    /// Thread context: Emitted from Main/GUI thread
    /// @param symbol The symbol that was selected (e.g. "NVDA")
    void symbolSelected(const QString& symbol);

    /// @brief Emitted when user triggers "Display Logs" from the context menu.
    /// Thread context: Emitted from Main/GUI thread
    /// @param strategyID  Strategy instance ID
    /// @param strategyName Display name of the strategy
    void displayLogsRequested(const QString& strategyID, const QString& strategyName);

  public slots:
    /// @brief Add a strategy row to the tree.
    void onStrategyLoaded(const QString& strategyID, const QString& name);

    /// @brief Clear all displayed strategy rows and position summary values.
    void clearStrategies();

    /// @brief Remove a strategy and its symbol children from the tree.
    void onStrategyUnloaded(const QString& strategyID);

    /// @brief Update the status indicator dot for a strategy and surface runtime failures.
    void onStrategyStatusChanged(const QString& strategyID,
                                 StrategyManager::StrategyExecutionState state,
                                 const QString& errorMessage);

    /// @brief Update the symbol children for a strategy after claim is granted.
    void onSymbolsClaimed(const QString& strategyID, const QStringList& claimedSymbols);

  private slots:
    void onLoadButtonClicked();
    void onRefreshPositions();
    void onContextMenuRequested(const QPoint& pos);
    void onDeleteSymbolShortcut();

  private:
    enum class DisplayMode
    {
        Strategies,
        ReviewSymbols
    };

    QTreeWidget* m_tree;
    MainAlgo* m_mainAlgo{nullptr};
    QTimer* m_positionTimer;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_loadButton = nullptr;
    DisplayMode m_displayMode = DisplayMode::Strategies;

    /// strategyID → top-level QTreeWidgetItem (strategy node)
    QMap<QString, QTreeWidgetItem*> m_strategyItems;
    /// strategyID → strategy display name (for context menus)
    QMap<QString, QString> m_strategyNames;
    /// strategyID → (symbol → child item)
    QMap<QString, QMap<QString, QTreeWidgetItem*>> m_symbolItems;
    /// strategyID → latest execution state received from StrategyManager signals
    QMap<QString, StrategyManager::StrategyExecutionState> m_strategyStates;
    /// strategyID → most recent async refresh token for stale-response rejection
    QMap<QString, quint64> m_strategyStateRefreshTokens;

    void setupUI();
    void setupStyles();
    void requestStrategySymbolStates(const QString& p_strategyID);
    void applyStrategySymbolStates(const QString& p_strategyID, const QVector<StrategySymbolViewState>& p_states);
    void dispatchMainAlgoStringCommand(const QString& p_dialogTitle, std::function<QString(MainAlgo*)> p_command);
    void requestSymbolUnclaim(const QString& p_strategyID,
                              const QString& p_strategyName,
                              const QString& p_symbol,
                              bool p_blockForSession);

    /// @brief Create or update symbol child items for a strategy node.
    void updateSymbolChildren(QTreeWidgetItem* strategyItem, const QStringList& symbols);

    /// @brief Find the strategy ID that owns the given tree item (strategy or child).
    QString strategyIDForItem(QTreeWidgetItem* item) const;
};
