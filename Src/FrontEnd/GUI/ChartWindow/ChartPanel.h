#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPointer>
#include <QShortcut>
#include <QLoggingCategory>

#include "Misc/TimeFrame.h"

// Forward declarations
class StockPriceChart;
class SymbolContext;
class MainAlgo;
class Order;
class Position;
struct StrategyLogEntry;

Q_DECLARE_LOGGING_CATEGORY(ChartPanelLog)

/**
 * @brief A single chart panel: symbol input + StockPriceChart + SymbolContext binding.
 *
 * ChartPanel is the atomic unit of chart display. One or more ChartPanels
 * live inside a ChartWindow's QSplitter.
 *
 * Thread context: All methods called on Main/GUI thread.
 */
class ChartPanel : public QWidget
{
    Q_OBJECT
  public:
    explicit ChartPanel(MainAlgo* p_mainAlgo, int p_panelId, QWidget* parent = nullptr);
    ~ChartPanel() override;

    void setSymbol(const QString& p_symbol);
    [[nodiscard]] QString getSymbol() const
    {
        return m_symbol;
    }

    [[nodiscard]] int panelId() const
    {
        return m_panelId;
    }

    void setTimeFrame(TimeFrame p_tf);
    [[nodiscard]] TimeFrame currentTimeFrame() const
    {
        return m_currentTimeFrame;
    }

    /// @brief Called at 30Hz by GUIFrontend to pull latest data from DisplaySnapshot.
    void refreshFromSnapshot();

    [[nodiscard]] StockPriceChart* chart() const
    {
        return m_chart;
    }

    void onOrderReceived(const Order& p_order);
    void onPositionReceived(const Position& p_position);
    void onPositionClosed(const Position& p_position);
    void onStrategyLogReceived(const StrategyLogEntry& p_entry);

    void enterReplayMode(const QString& p_replaySymbol);
    void exitReplayMode();

  signals:
    /**
     * @brief Emitted when this panel's displayed symbol changes.
     * Thread context: Emitted from Main/GUI thread.
     */
    void symbolChanged(const QString& p_symbol);

  private slots:
    void onSymbolInputReturnPressed();
    void onTimeFrameChanged(TimeFrame p_tf);

  private:
    void setupUi();
    void setupShortcuts();
    void releaseCurrentSymbolContext();
    void requestMissingBarsFromCache(const QDateTime& p_from, const QDateTime& p_to);

    MainAlgo* m_mainAlgo;
    int m_panelId;
    QString m_symbol;
    QString m_preReplaySymbol;
    TimeFrame m_currentTimeFrame = TimeFrame::ONE_MINUTE;
    QPointer<SymbolContext> m_symbolContext;

    // UI elements
    StockPriceChart* m_chart = nullptr;
    QLineEdit* m_symbolInput = nullptr;
    QLabel* m_symbolLabel = nullptr;

    // Shortcuts (scoped to this panel's parent window via context)
    QShortcut* m_focusShortcut = nullptr;
    QShortcut* m_tf10sShortcut = nullptr;
    QShortcut* m_tf1mShortcut = nullptr;
    QShortcut* m_tf5mShortcut = nullptr;
    QShortcut* m_tf15mShortcut = nullptr;
    QShortcut* m_tf30mShortcut = nullptr;
    QShortcut* m_tf1hShortcut = nullptr;
    QShortcut* m_tf4hShortcut = nullptr;
    QShortcut* m_tf1dShortcut = nullptr;
    QShortcut* m_tf1wShortcut = nullptr;
    QShortcut* m_tf1MShortcut = nullptr;
};
