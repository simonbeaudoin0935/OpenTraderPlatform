#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QMap>
#include <QStringList>

/// @brief Compact live view of active strategies and their claimed symbols.
/// Displays a collapsible tree: strategy nodes (root) → symbol nodes (children).
/// Clicking a symbol node emits symbolSelected(symbol) → GUIFrontend::displayStock().
/// Updated via StrategyManager signals: strategyLoaded, strategyUnloaded,
/// strategyStatusChanged, symbolsClaimed.
class StrategyQuickView : public QWidget
{
    Q_OBJECT

  public:
    explicit StrategyQuickView(QWidget* parent = nullptr);
    ~StrategyQuickView() override = default;

  signals:
    /// @brief Emitted when user clicks a symbol node in the tree.
    /// Thread context: Emitted from Main/GUI thread
    /// @param symbol The symbol that was selected (e.g. "NVDA")
    void symbolSelected(const QString& symbol);

  public slots:
    /// @brief Add a strategy row to the tree.
    void onStrategyLoaded(const QString& strategyID, const QString& name);

    /// @brief Remove a strategy and its symbol children from the tree.
    void onStrategyUnloaded(const QString& strategyID);

    /// @brief Update the status indicator dot for a strategy.
    /// @param isRunning true = green dot (running), false = grey dot (stopped/error)
    void onStrategyStatusChanged(const QString& strategyID, bool isRunning, const QString& errorMessage);

    /// @brief Update the symbol children for a strategy after claim is granted.
    void onSymbolsClaimed(const QString& strategyID, const QStringList& claimedSymbols);

  private:
    QTreeWidget* m_tree;

    /// strategyID → top-level QTreeWidgetItem (strategy node)
    QMap<QString, QTreeWidgetItem*> m_strategyItems;

    void setupUI();
    void setupStyles();

    /// @brief Create or update symbol child items for a strategy node.
    void updateSymbolChildren(QTreeWidgetItem* strategyItem, const QStringList& symbols);
};
