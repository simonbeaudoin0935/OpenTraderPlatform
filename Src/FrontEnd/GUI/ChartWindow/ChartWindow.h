#pragma once

#include <QWidget>
#include <QVector>
#include <QShortcut>
#include <QLoggingCategory>

#include "Misc/TimeFrame.h"

// Forward declarations
class ChartPanel;
class MainAlgo;
class QSplitter;
class Order;
class Position;
struct StrategyLogEntry;
struct StrategyStatusEntry;
struct StrategyBracketOverlayEntry;

Q_DECLARE_LOGGING_CATEGORY(ChartWindowLog)

/**
 * @brief A top-level window containing one or more ChartPanels in a QSplitter.
 *
 * ChartWindow is a container for ChartPanels. Each Ctrl+T press either creates
 * a new ChartWindow (if none exist) or adds a new panel to the existing window.
 * The QSplitter orientation is auto-detected from the screen aspect ratio:
 *   - Landscape → Qt::Horizontal (side-by-side)
 *   - Portrait  → Qt::Vertical   (stacked)
 *
 * Lifecycle:
 *  - Created by WindowManager::createChartWindow()
 *  - Panels added via addChartPanel()
 *  - Closed via Ctrl+W or window close button
 *  - On close, all panels release their SymbolContext refs
 *
 * Thread context: All methods called on Main/GUI thread.
 */
class ChartWindow : public QWidget
{
    Q_OBJECT
  public:
    explicit ChartWindow(MainAlgo* p_mainAlgo, int p_windowId, QWidget* parent = nullptr);
    ~ChartWindow() override;

    /// @brief Add a new chart panel displaying the given symbol.
    /// @return Pointer to the newly created panel.
    ChartPanel* addChartPanel(const QString& p_symbol = QString());

    /// @brief Get the window's unique ID (for persistence).
    [[nodiscard]] int windowId() const
    {
        return m_windowId;
    }

    /// @brief Get all panels in this window.
    [[nodiscard]] const QVector<ChartPanel*>& panels() const
    {
        return m_panels;
    }

    /// @brief Number of panels in this window.
    [[nodiscard]] int panelCount() const
    {
        return m_panels.size();
    }

    /// @brief Refresh all panels from their respective snapshots (called at 30Hz).
    void refreshAllPanels();

    /// @brief Forward order events to all panels (each filters by symbol internally).
    void onOrderReceived(const Order& p_order);

    /// @brief Forward position events to all panels.
    void onPositionReceived(const Position& p_position);
    void onPositionClosed(const Position& p_position);

    /// @brief Forward strategy log events to all panels.
    void onStrategyLogReceived(const StrategyLogEntry& p_entry);
    void onStrategyStatusReceived(const StrategyStatusEntry& p_entry);
    void onStrategyBracketOverlayReceived(const StrategyBracketOverlayEntry& p_entry);

    /// @brief Save all panel symbols and switch to replay symbol.
    void enterReplayMode(const QString& p_replaySymbol);

    /// @brief Restore pre-replay symbols for all panels.
    void exitReplayMode();

    /// @brief Update the QSplitter orientation based on current screen geometry.
    void updateSplitOrientation();

    /// @brief Update the window title to reflect panel symbols.
    void updateWindowTitle();

  signals:
    /**
     * @brief Emitted when the user closes this window (Ctrl+W or close button).
     * Thread context: Emitted from Main/GUI thread.
     */
    void windowClosed(ChartWindow* window);

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    void setupUi();
    void setupShortcuts();

    MainAlgo* m_mainAlgo;
    int m_windowId;
    int m_nextPanelId = 0;

    // UI elements
    QSplitter* m_splitter = nullptr;
    QVector<ChartPanel*> m_panels;

    // Shortcuts
    QShortcut* m_closeShortcut = nullptr;
};
