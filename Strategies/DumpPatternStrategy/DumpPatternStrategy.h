#pragma once

#include "Strategy/StrategyBase.h"
#include "Strategy/StrategyConfig.h"
#include "Strategy/StrategySDK.h"
#include <QObject>
#include <QDateTime>
#include <QString>

/**
 * @brief DumpPatternStrategy - Tests data feed integration + order cycling
 *
 * This strategy demonstrates event-driven price tracking and order management.
 * Rather than using a timer, every incoming market data event (trade print or
 * bar close) is checked against the cycle start time. When N seconds have
 * elapsed, the state machine advances.
 *
 * State machine (alternates Long/Short each round):
 *
 *   Idle           → wait N seconds → place LIMIT entry order → WaitingLimitEntry
 *   WaitingLimitEntry:
 *     fill arrives → EntryFilled, reset clock
 *     timeout      → cancel entry → Idle, flip direction
 *   EntryFilled    → wait N seconds → place MARKET close order → WaitingClose
 *   WaitingClose:
 *     fill arrives → log P&L → Idle, flip direction
 *     timeout      → warning, reset → Idle, flip direction
 *
 * Config customParams:
 *   "entryOffsetCents"     : Cents above/below last price for limit entry (default: 10)
 *   "cycleDurationSeconds" : Duration of each phase in seconds (default: 10)
 */
class DumpPatternStrategy : public QObject, public StrategyBase
{
    Q_OBJECT

  public:
    explicit DumpPatternStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk);
    ~DumpPatternStrategy() override = default;

    // --- Lifecycle ---
    void onStart(StrategySDK* p_sdk) override;
    void onStop() override;
    void onPause() override {}

    // --- Data callbacks ---
    void onBar(const Bar& p_bar) override;
    void onTrade(const Trade& p_trade) override;
    void onOrderUpdated(const Order& p_order) override;
    void onOrderFilled(const Order& p_order) override;
    void onOrderCancelled(const Order& p_order, const std::string& p_reason) override;
    void onOrderRejected(const Order& p_order, const std::string& p_reason) override;
    void onPositionUpdated(const Position& p_position) override;
    void onBalanceUpdated(double p_balance) override;
    void onError(const std::string& p_error) override;

  private:
    enum class State
    {
        Idle,              ///< Waiting for next cycle start (N seconds)
        WaitingLimitEntry, ///< Limit entry order placed, waiting for fill or timeout
        EntryFilled,       ///< Entry filled; waiting N seconds before placing close order
        WaitingClose,      ///< Close order placed, waiting for fill or timeout
    };

    /// @brief Update last price and check if the current phase has elapsed
    void updatePrice(double p_price);

    /// @brief Evaluate elapsed time and advance the state machine if needed
    void checkCycle();

    // Phase transitions
    void startEntryPhase();
    void evaluateEntryPhase();
    void startClosePhase();
    void evaluateClosePhase();
    void resetCycle();

    // Order helpers
    void placeEntryOrder();
    void placeCloseOrder();
    void cancelEntryOrder();

    StrategySDK* m_sdk = nullptr;
    StrategyConfig m_config;
    State m_state = State::Idle;
    bool m_isLong = true; ///< true = long cycle, false = short cycle

    double m_lastPrice = 0.0;
    QDateTime m_cycleStartTime;

    QString m_entryOrderID; ///< Active limit entry order track ID
    QString m_closeOrderID; ///< Active market close order track ID

    double m_entryFillPrice = 0.0; ///< Price at which entry was filled (for P&L)

    // Config params
    double m_entryOffsetDollars = 0.10; ///< Dollar offset from last price for limit entry
    int m_cycleDurationSec = 10;        ///< Seconds per phase

    // Stats
    int m_cyclesCompleted = 0;
    int m_cyclesCancelled = 0;
    double m_totalPnL = 0.0;
};
