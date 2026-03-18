#pragma once

#include <QObject>
#include <QVector>
#include <QLoggingCategory>

class ChartWindow;
class ChartPanel;
class MainAlgo;
class QScreen;

Q_DECLARE_LOGGING_CATEGORY(WindowManagerLog)

/**
 * @brief Manages the lifecycle of secondary chart windows and their panels.
 *
 * Handles creation, destruction, QScreen-based auto-placement, and
 * AppState persistence for ChartWindows and their ChartPanels.
 *
 * Ctrl+T logic:
 *  - If no secondary ChartWindow exists → create one with one panel
 *  - If a secondary ChartWindow already exists → add a new panel to it
 *
 * Persistence: state is saved to AppState.ini on every mutation (panel added,
 * symbol changed, window closed).
 *
 * Thread context: All methods must be called on the Main/GUI thread.
 */
class WindowManager : public QObject
{
    Q_OBJECT
  public:
    explicit WindowManager(MainAlgo* p_mainAlgo, QObject* parent = nullptr);
    ~WindowManager() override;

    /// @brief Handle Ctrl+T: create a new window or add a panel to the existing one.
    /// @return The ChartPanel that was just created.
    ChartPanel* openNewChart(const QString& p_symbol = QString());

    /// @brief Create a new chart window (always creates a new top-level window).
    ChartWindow* createChartWindow(const QString& p_symbol = QString());

    /// @brief Close and destroy a specific chart window.
    void closeChartWindow(ChartWindow* p_window);

    /// @brief Close all secondary chart windows (called on app exit).
    void closeAllChartWindows();

    /// @brief Get all currently open chart windows.
    [[nodiscard]] const QVector<ChartWindow*>& chartWindows() const
    {
        return m_chartWindows;
    }

    /// @brief Save all chart window states to AppState.ini.
    void saveWindowState();

    /// @brief Restore chart windows from last session's AppState.ini.
    void restoreWindowState();

    /// @brief Mark shutdown in progress — suppresses saves from window close events.
    void setShuttingDown()
    {
        m_shuttingDown = true;
    }

  private:
    /// @brief Find the best screen to place a new window on (fewest windows).
    QScreen* findBestScreenForNewWindow() const;

    /// @brief Handle a chart window signaling that it was closed.
    void onChartWindowClosed(ChartWindow* p_window);

    MainAlgo* m_mainAlgo;
    QVector<ChartWindow*> m_chartWindows;
    int m_nextWindowId = 0;
    bool m_shuttingDown = false;
};
