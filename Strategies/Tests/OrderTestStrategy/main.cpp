#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <QDateTime>
#include <QTime>
#include <QTimeZone>

#include "OpenTraderPlatform/StrategySDK/ConfigurationHelpers.h"
#include "OpenTraderPlatform/StrategySDK/ExternalStrategyRuntime.h"
#include "OpenTraderPlatform/StrategySDK/StrategyDescription.h"
#include "OpenTraderPlatform/StrategySDK/StrategyProcessMain.h"

namespace
{
    using namespace std::chrono_literals;
    namespace Protocol = opentraderplatform::strategy::v1;

    constexpr std::string_view kStrategyName = "OrderTestStrategyProcess";
    constexpr std::string_view kStrategyVersion = "1.3.0";
    constexpr double kMinimumEquityPrice = 0.01;

    [[nodiscard]] const QTimeZone& getNewYorkTimeZone()
    {
        static const QTimeZone timezone("America/New_York");
        return timezone;
    }

    enum class StrategyTradingSession
    {
        Regular,
        Extended,
        Closed,
    };

    [[nodiscard]] StrategyTradingSession tradingSessionForUnixNanos(const std::int64_t p_unixNanos)
    {
        const QDateTime time = QDateTime::fromMSecsSinceEpoch(p_unixNanos / 1000000LL, getNewYorkTimeZone());
        if (!time.isValid())
        {
            return StrategyTradingSession::Closed;
        }

        const int dayOfWeek = time.date().dayOfWeek();
        if (dayOfWeek == Qt::Saturday || dayOfWeek == Qt::Sunday)
        {
            return StrategyTradingSession::Closed;
        }

        const QTime current = time.time();
        if (current >= QTime(9, 30) && current < QTime(16, 0))
        {
            return StrategyTradingSession::Regular;
        }
        if (current >= QTime(4, 0) && current < QTime(19, 0))
        {
            return StrategyTradingSession::Extended;
        }
        return StrategyTradingSession::Closed;
    }

    [[nodiscard]] OpenTraderPlatform::StrategySDK::StrategyDescription describeStrategy()
    {
        return {
            .name = std::string(kStrategyName),
            .version = std::string(kStrategyVersion),
            .parameterSchema =
                {
                    OpenTraderPlatform::StrategySDK::stringField("symbol",
                                                       "Symbol",
                                                       "SPY",
                                                       "Ticker symbol used for the Strategy View exercise"),
                    OpenTraderPlatform::StrategySDK::stringField("accountID",
                                                       "Account ID",
                                                       "SIM123456",
                                                       "Broker account used for order placement"),
                    OpenTraderPlatform::StrategySDK::intField("quantity",
                                                    "Quantity",
                                                    1,
                                                    "Share quantity for each buy and sell cycle"),
                    OpenTraderPlatform::StrategySDK::intField("cycleCount",
                                                    "Cycle Count",
                                                    3,
                                                    "How many buy/hold/sell/flat cycles to run"),
                    OpenTraderPlatform::StrategySDK::intField(
                        "holdOpenSeconds",
                        "Hold Open Seconds",
                        8,
                        "How long to keep each position open so Strategy View can show live U/P&L"),
                    OpenTraderPlatform::StrategySDK::intField(
                        "holdFlatSeconds",
                        "Hold Flat Seconds",
                        5,
                        "How long to wait flat between cycles so Strategy View can show the realized total"),
                    OpenTraderPlatform::StrategySDK::intField(
                        "startupDelaySeconds",
                        "Startup Delay Seconds",
                        3,
                        "How long to wait before the first trade so replay/live quotes have time to populate."),
                    OpenTraderPlatform::StrategySDK::doubleField(
                        "aggressiveLimitOffsetCents",
                        "Aggressive Limit Offset (cents)",
                        5.0,
                        "Outside regular hours, use inside bid/ask when available, otherwise latest trade plus or minus this offset."),
                    OpenTraderPlatform::StrategySDK::intField("timeoutSecs",
                                                    "Timeout (seconds)",
                                                    60,
                                                    "Fails the test if the strategy does not complete in time"),
                },
        };
    }

    [[nodiscard]] std::string orderStatusToString(const Protocol::OrderStatus p_status)
    {
        switch (p_status)
        {
        case Protocol::ORDER_STATUS_PENDING:
            return "PENDING";
        case Protocol::ORDER_STATUS_ACKNOWLEDGED:
            return "ACKNOWLEDGED";
        case Protocol::ORDER_STATUS_OPEN:
            return "OPEN";
        case Protocol::ORDER_STATUS_FILLED:
            return "FILLED";
        case Protocol::ORDER_STATUS_CANCELLED:
            return "CANCELLED";
        case Protocol::ORDER_STATUS_REJECTED:
            return "REJECTED";
        case Protocol::ORDER_STATUS_UNSPECIFIED:
        default:
            return "UNSPECIFIED";
        }
    }

    class OrderTestStrategyProcess final : public OpenTraderPlatform::StrategySDK::ExternalStrategyHandler
    {
      public:
        void bindRuntime(OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* const p_runtime) override
        {
            m_runtime = p_runtime;
        }

        void onStart(const Protocol::StrategyConfiguration& p_configuration) override
        {
            m_symbol = OpenTraderPlatform::StrategySDK::stringFieldOr(p_configuration, "symbol", "SPY");
            m_accountId = OpenTraderPlatform::StrategySDK::stringFieldOr(p_configuration, "accountID", "SIM123456");
            m_quantity = static_cast<std::uint32_t>(OpenTraderPlatform::StrategySDK::intFieldOr(p_configuration, "quantity", 1));
            m_cycleTarget = static_cast<int>(OpenTraderPlatform::StrategySDK::intFieldOr(p_configuration, "cycleCount", 3));
            m_holdOpenSeconds =
                static_cast<int>(OpenTraderPlatform::StrategySDK::intFieldOr(p_configuration, "holdOpenSeconds", 8));
            m_holdFlatSeconds =
                static_cast<int>(OpenTraderPlatform::StrategySDK::intFieldOr(p_configuration, "holdFlatSeconds", 5));
            m_startupDelaySeconds =
                static_cast<int>(OpenTraderPlatform::StrategySDK::intFieldOr(p_configuration, "startupDelaySeconds", 3));
            m_aggressiveLimitOffsetCents =
                std::max(0.0, OpenTraderPlatform::StrategySDK::doubleFieldOr(p_configuration, "aggressiveLimitOffsetCents", 5.0));
            const int timeoutSecs =
                static_cast<int>(OpenTraderPlatform::StrategySDK::intFieldOr(p_configuration, "timeoutSecs", 60));

            if (m_quantity == 0)
            {
                fail("[Init] FAIL - quantity must be greater than zero");
                return;
            }

            if (m_cycleTarget <= 0)
            {
                fail("[Init] FAIL - cycleCount must be greater than zero");
                return;
            }

            if (m_holdOpenSeconds < 0 || m_holdFlatSeconds < 0 || m_startupDelaySeconds < 0 || timeoutSecs <= 0)
            {
                fail("[Init] FAIL - timer values must be non-negative and timeout must be positive");
                return;
            }

            sendLog("=== OrderTestStrategyProcess starting ===");
            sendLog("  account         : " + m_accountId);
            sendLog("  symbol          : " + m_symbol);
            sendLog("  quantity        : " + std::to_string(m_quantity));
            sendLog("  cycleCount      : " + std::to_string(m_cycleTarget));
            sendLog("  holdOpenSeconds : " + std::to_string(m_holdOpenSeconds));
            sendLog("  holdFlatSeconds : " + std::to_string(m_holdFlatSeconds));
            sendLog("  startupDelaySec : " + std::to_string(m_startupDelaySeconds));
            sendLog("  limitOffsetCts  : " + std::to_string(m_aggressiveLimitOffsetCents));
            sendLog("  timeout         : " + std::to_string(timeoutSecs) + "s");
            sendLog("[Init] balance=$" + std::to_string(m_runtime->cashBalance()) +
                    " openOrders=" + std::to_string(m_runtime->orders().size()) +
                    " positions=" + std::to_string(m_runtime->positions().size()));

            const auto claimResult = m_runtime->claimSymbols({m_symbol});
            if (claimResult.grantedSymbols.empty())
            {
                fail("[Init] FAIL - symbol claim denied");
                return;
            }

            m_timeoutTimerId =
                m_runtime->startTimer(std::chrono::seconds(timeoutSecs),
                                      [this]()
                                      {
                                          if (m_state == State::Complete)
                                          {
                                              return;
                                          }

                                          fail("[Timeout] Test timed out in state " + stateToString(m_state));
                                      });

            startNextCycle();
        }

        void onStop(std::string_view p_reason) override
        {
            stopAllTimers();
            sendLog("=== OrderTestStrategyProcess stopped ===");
            sendLog("[Stop] " + std::string(p_reason));
        }

        void onShutdown(std::string_view p_reason) override
        {
            stopAllTimers();
            sendLog("=== OrderTestStrategyProcess stopped ===");
            sendLog("[Shutdown] " + std::string(p_reason));
        }

        void onOrderUpdate(const Protocol::OrderUpdate& p_order) override
        {
            if (p_order.symbol() != m_symbol)
            {
                return;
            }

            sendLog("[Order] cycle=" + std::to_string(m_currentCycle) + " id=" + p_order.order_id() +
                    " status=" + orderStatusToString(p_order.status()) + " qty=" + std::to_string(p_order.quantity()));

            switch (m_state)
            {
            case State::WaitStartupDelay:
                break;
            case State::WaitEntryFill:
                handleEntryOrderUpdate(p_order);
                break;
            case State::WaitExitFill:
                handleExitOrderUpdate(p_order);
                break;
            case State::Init:
            case State::WaitEntryIntent:
            case State::HoldOpenPosition:
            case State::WaitExitIntent:
            case State::HoldFlat:
            case State::Complete:
                break;
            }
        }

        void onPositionUpdate(const Protocol::PositionUpdate& p_position) override
        {
            if (p_position.symbol() != m_symbol)
            {
                return;
            }

            sendLog("[Position] cycle=" + std::to_string(m_currentCycle) + " symbol=" + p_position.symbol() + " qty=" +
                    std::to_string(p_position.quantity()) + " avgPx=" + std::to_string(p_position.average_price()) +
                    " uPnL=" + std::to_string(p_position.unrealized_pnl()) +
                    " realizedSum=" + std::to_string(m_cumulativeRealizedPnL));

            if (p_position.quantity() != 0)
            {
                ++m_openPositionUpdateCount;
            }
            else
            {
                ++m_flatPositionUpdateCount;
            }
        }

        void onLevel2Snapshot(const Protocol::Level2Snapshot& p_level2) override
        {
            if (p_level2.symbol() != m_symbol)
            {
                return;
            }

            m_lastObservedUnixNanos = p_level2.snapshot_unix_nanos();
            m_bestBidPrice = p_level2.bids_size() > 0 ? p_level2.bids(0).price() : 0.0;
            m_bestAskPrice = p_level2.asks_size() > 0 ? p_level2.asks(0).price() : 0.0;

            if (m_state == State::WaitEntryIntent)
            {
                placeEntryOrder();
            }
            else if (m_state == State::WaitExitIntent)
            {
                placeExitOrder();
            }
        }

        void onTrade(const Protocol::Trade& p_trade) override
        {
            if (p_trade.symbol() != m_symbol)
            {
                return;
            }

            m_lastObservedUnixNanos = p_trade.trade_unix_nanos();
            m_lastTradePrice = p_trade.price();

            if (m_state == State::WaitEntryIntent)
            {
                placeEntryOrder();
            }
            else if (m_state == State::WaitExitIntent)
            {
                placeExitOrder();
            }
        }

        void onBalanceUpdate(const Protocol::BalanceUpdate& p_balance) override
        {
            sendLog("[Balance] $" + std::to_string(p_balance.cash()), Protocol::LOG_LEVEL_DEBUG);
        }

        void onHostError(const Protocol::ErrorMessage& p_error) override
        {
            fail("[Error] " + p_error.code() + ": " + p_error.message());
        }

      private:
        enum class State
        {
            Init,
            WaitStartupDelay,
            WaitEntryIntent,
            WaitEntryFill,
            HoldOpenPosition,
            WaitExitIntent,
            WaitExitFill,
            HoldFlat,
            Complete,
        };

        [[nodiscard]] static std::string stateToString(const State p_state)
        {
            switch (p_state)
            {
            case State::Init:
                return "Init";
            case State::WaitStartupDelay:
                return "WaitStartupDelay";
            case State::WaitEntryIntent:
                return "WaitEntryIntent";
            case State::WaitEntryFill:
                return "WaitEntryFill";
            case State::HoldOpenPosition:
                return "HoldOpenPosition";
            case State::WaitExitIntent:
                return "WaitExitIntent";
            case State::WaitExitFill:
                return "WaitExitFill";
            case State::HoldFlat:
                return "HoldFlat";
            case State::Complete:
                return "Complete";
            }

            return "Unknown";
        }

        struct OrderIntentDetails
        {
            Protocol::OrderType type = Protocol::ORDER_TYPE_UNSPECIFIED;
            Protocol::OrderDuration duration = Protocol::ORDER_DURATION_UNSPECIFIED;
            std::optional<double> limitPrice;
        };

        [[nodiscard]] std::optional<std::int64_t> currentTimeUnixNanos() const
        {
            if (m_runtime == nullptr)
            {
                return std::nullopt;
            }

            if (const auto requested = m_runtime->requestCurrentTimeUnixNanos(); requested.has_value())
            {
                return requested;
            }

            if (m_lastObservedUnixNanos > 0)
            {
                return m_lastObservedUnixNanos;
            }

            return std::nullopt;
        }

        [[nodiscard]] double aggressiveLimitOffsetDollars() const
        {
            return std::max(0.0, m_aggressiveLimitOffsetCents) / 100.0;
        }

        [[nodiscard]] std::optional<double> aggressiveLimitPrice(const Protocol::OrderSide p_side,
                                                                 const double p_fallbackPrice = 0.0) const
        {
            const double marketReference = p_side == Protocol::ORDER_SIDE_BUY ? m_bestAskPrice : m_bestBidPrice;
            const double tradeReference = m_lastTradePrice > 0.0 ? m_lastTradePrice : p_fallbackPrice;
            const double referencePrice = marketReference > 0.0 ? marketReference : tradeReference;
            if (referencePrice <= 0.0)
            {
                return std::nullopt;
            }

            const double offset = aggressiveLimitOffsetDollars();
            if (p_side == Protocol::ORDER_SIDE_BUY)
            {
                return std::max(kMinimumEquityPrice, referencePrice + offset);
            }

            return std::max(kMinimumEquityPrice, referencePrice - offset);
        }

        [[nodiscard]] std::optional<OrderIntentDetails> buildOrderIntent(const Protocol::OrderSide p_side,
                                                                         const Protocol::OrderType p_regularType,
                                                                         const double p_fallbackPrice = 0.0) const
        {
            const auto nowUnixNanos = currentTimeUnixNanos();
            if (!nowUnixNanos.has_value())
            {
                return std::nullopt;
            }

            const auto session = tradingSessionForUnixNanos(*nowUnixNanos);
            if (session == StrategyTradingSession::Closed)
            {
                return std::nullopt;
            }

            OrderIntentDetails intent;
            if (session == StrategyTradingSession::Regular)
            {
                intent.type = p_regularType;
                intent.duration = Protocol::ORDER_DURATION_DAY;
                return intent;
            }

            const auto limitPrice = aggressiveLimitPrice(p_side, p_fallbackPrice);
            if (!limitPrice.has_value())
            {
                return std::nullopt;
            }

            intent.type = Protocol::ORDER_TYPE_LIMIT;
            intent.duration = Protocol::ORDER_DURATION_DAY_PLUS;
            intent.limitPrice = *limitPrice;
            return intent;
        }

        [[nodiscard]] static std::string describeOrderIntent(const OrderIntentDetails& p_intent)
        {
            std::string description = p_intent.type == Protocol::ORDER_TYPE_MARKET ? "market" : "limit";
            if (p_intent.limitPrice.has_value())
            {
                description += " @$" + std::to_string(*p_intent.limitPrice);
            }

            if (p_intent.duration == Protocol::ORDER_DURATION_DAY_PLUS)
            {
                description += " DayPlus";
            }
            else if (p_intent.duration == Protocol::ORDER_DURATION_DAY)
            {
                description += " Day";
            }

            return description;
        }

        [[nodiscard]] static std::string sessionToString(const StrategyTradingSession p_session)
        {
            switch (p_session)
            {
            case StrategyTradingSession::Regular:
                return "regular";
            case StrategyTradingSession::Extended:
                return "extended";
            case StrategyTradingSession::Closed:
                return "closed";
            }

            return "unknown";
        }

        [[nodiscard]] std::string describeIntentWaitReason(const Protocol::OrderSide p_side,
                                                           const double p_fallbackPrice = 0.0) const
        {
            const auto nowUnixNanos = currentTimeUnixNanos();
            if (!nowUnixNanos.has_value())
            {
                return "host current time is not available yet";
            }

            const auto session = tradingSessionForUnixNanos(*nowUnixNanos);
            if (session == StrategyTradingSession::Closed)
            {
                return "session is currently closed";
            }

            if (session == StrategyTradingSession::Extended)
            {
                const auto limitPrice = aggressiveLimitPrice(p_side, p_fallbackPrice);
                if (!limitPrice.has_value())
                {
                    return "extended-hours reference price is not available yet (bid=$" +
                           std::to_string(m_bestBidPrice) + " ask=$" + std::to_string(m_bestAskPrice) + " last=$" +
                           std::to_string(m_lastTradePrice) + ")";
                }
            }

            return "session=" + sessionToString(session) + " is not ready yet";
        }

        void ensureIntentRetryTimer()
        {
            if (m_runtime == nullptr || m_intentRetryTimerId != 0)
            {
                return;
            }

            m_intentRetryTimerId = m_runtime->startTimer(
                std::chrono::seconds(1),
                [this]()
                {
                    switch (m_state)
                    {
                    case State::WaitEntryIntent:
                        placeEntryOrder();
                        break;
                    case State::WaitExitIntent:
                        placeExitOrder();
                        break;
                    default:
                        stopTimer(m_intentRetryTimerId);
                        break;
                    }
                },
                true);
        }

        void waitForIntent(const State p_waitState,
                           const Protocol::OrderSide p_side,
                           const std::string& p_stepLabel,
                           const double p_fallbackPrice = 0.0)
        {
            const bool firstWait = (m_state != p_waitState);
            m_state = p_waitState;
            ensureIntentRetryTimer();
            if (!firstWait)
            {
                return;
            }

            sendLog("[Cycle " + std::to_string(m_currentCycle) + " " + p_stepLabel + "] Waiting to build a valid " +
                        (p_side == Protocol::ORDER_SIDE_BUY ? "entry" : "exit") +
                        " intent: " + describeIntentWaitReason(p_side, p_fallbackPrice),
                    Protocol::LOG_LEVEL_WARNING);
        }

        void startNextCycle()
        {
            if (m_completedCycles >= m_cycleTarget)
            {
                complete();
                return;
            }

            m_currentCycle = m_completedCycles + 1;
            m_entryOrderId.clear();
            m_exitOrderId.clear();
            m_entryFillPrice = 0.0;
            m_exitFillPrice = 0.0;

            sendLog("");
            sendLog("[Cycle " + std::to_string(m_currentCycle) + "/" + std::to_string(m_cycleTarget) +
                    "] Starting buy/hold/sell sequence");
            sendLog(
                "[Cycle " + std::to_string(m_currentCycle) +
                "] Strategy View expectation: U/P&L should move while open; R/P&L should reflect cumulative realized P&L across completed cycles");

            if (!m_startupDelayConsumed && m_startupDelaySeconds > 0)
            {
                m_startupDelayConsumed = true;
                m_state = State::WaitStartupDelay;
                sendLog("[Cycle " + std::to_string(m_currentCycle) + " Step 0] Warming up for " +
                        std::to_string(m_startupDelaySeconds) + "s before the first entry so market data can populate");
                m_startupDelayTimerId = m_runtime->startTimer(std::chrono::seconds(m_startupDelaySeconds),
                                                              [this]()
                                                              {
                                                                  stopTimer(m_startupDelayTimerId);
                                                                  if (m_state == State::WaitStartupDelay)
                                                                  {
                                                                      placeEntryOrder();
                                                                  }
                                                              });
                return;
            }

            placeEntryOrder();
        }

        void placeEntryOrder()
        {
            const auto intent = buildOrderIntent(Protocol::ORDER_SIDE_BUY, Protocol::ORDER_TYPE_MARKET);
            if (!intent.has_value())
            {
                waitForIntent(State::WaitEntryIntent, Protocol::ORDER_SIDE_BUY, "Step 1");
                return;
            }

            stopTimer(m_intentRetryTimerId);
            sendLog("[Cycle " + std::to_string(m_currentCycle) + " Step 1] Placing " + describeOrderIntent(*intent) +
                    " BUY " + std::to_string(m_quantity) + " " + m_symbol + " ...");
            m_state = State::WaitEntryFill;
            const std::string requestId = m_runtime->placeOrder(m_symbol,
                                                                m_accountId,
                                                                Protocol::ORDER_SIDE_BUY,
                                                                intent->type,
                                                                m_quantity,
                                                                intent->limitPrice,
                                                                std::nullopt,
                                                                intent->duration);
            if (requestId.empty())
            {
                fail("[Cycle " + std::to_string(m_currentCycle) + " Step 1] FAIL - unable to send entry order intent");
            }
        }

        void scheduleExit()
        {
            stopTimer(m_holdOpenTimerId);
            sendLog("[Cycle " + std::to_string(m_currentCycle) + " Step 2] Holding open position for " +
                    std::to_string(m_holdOpenSeconds) + "s");
            m_state = State::HoldOpenPosition;
            m_holdOpenTimerId = m_runtime->startTimer(std::chrono::seconds(m_holdOpenSeconds),
                                                      [this]()
                                                      {
                                                          if (m_state != State::HoldOpenPosition)
                                                          {
                                                              return;
                                                          }

                                                          placeExitOrder();
                                                      });
        }

        void placeExitOrder()
        {
            const auto intent =
                buildOrderIntent(Protocol::ORDER_SIDE_SELL, Protocol::ORDER_TYPE_MARKET, m_entryFillPrice);
            if (!intent.has_value())
            {
                waitForIntent(State::WaitExitIntent, Protocol::ORDER_SIDE_SELL, "Step 3", m_entryFillPrice);
                return;
            }

            stopTimer(m_intentRetryTimerId);
            sendLog("[Cycle " + std::to_string(m_currentCycle) + " Step 3] Placing " + describeOrderIntent(*intent) +
                    " SELL " + std::to_string(m_quantity) + " " + m_symbol + " ...");
            m_state = State::WaitExitFill;
            const std::string requestId = m_runtime->placeOrder(m_symbol,
                                                                m_accountId,
                                                                Protocol::ORDER_SIDE_SELL,
                                                                intent->type,
                                                                m_quantity,
                                                                intent->limitPrice,
                                                                std::nullopt,
                                                                intent->duration);
            if (requestId.empty())
            {
                fail("[Cycle " + std::to_string(m_currentCycle) + " Step 3] FAIL - unable to send exit order intent");
            }
        }

        void handleEntryOrderUpdate(const Protocol::OrderUpdate& p_order)
        {
            if (m_entryOrderId.empty())
            {
                m_entryOrderId = p_order.order_id();
                ++m_entryOrderPlacedCount;
                sendLog("[Cycle " + std::to_string(m_currentCycle) +
                        " Step 1] Entry order acknowledged - OrderID=" + m_entryOrderId);
            }
            else if (p_order.order_id() != m_entryOrderId)
            {
                return;
            }

            if (p_order.status() == Protocol::ORDER_STATUS_FILLED)
            {
                ++m_entryOrderFilledCount;
                if (p_order.has_average_fill_price())
                {
                    m_entryFillPrice = p_order.average_fill_price();
                }

                sendLog("[Cycle " + std::to_string(m_currentCycle) + " Step 1] PASS - entry filled @ $" +
                        std::to_string(m_entryFillPrice));
                scheduleExit();
            }
            else if (p_order.status() == Protocol::ORDER_STATUS_REJECTED)
            {
                fail("[Cycle " + std::to_string(m_currentCycle) + " Step 1] FAIL - entry order rejected");
            }
        }

        void handleExitOrderUpdate(const Protocol::OrderUpdate& p_order)
        {
            if (m_exitOrderId.empty())
            {
                m_exitOrderId = p_order.order_id();
                ++m_exitOrderPlacedCount;
                sendLog("[Cycle " + std::to_string(m_currentCycle) +
                        " Step 3] Exit order acknowledged - OrderID=" + m_exitOrderId);
            }
            else if (p_order.order_id() != m_exitOrderId)
            {
                return;
            }

            if (p_order.status() == Protocol::ORDER_STATUS_FILLED)
            {
                ++m_exitOrderFilledCount;
                if (p_order.has_average_fill_price())
                {
                    m_exitFillPrice = p_order.average_fill_price();
                }

                const double cycleRealizedPnL = (m_exitFillPrice - m_entryFillPrice) * static_cast<double>(m_quantity);
                m_cumulativeRealizedPnL += cycleRealizedPnL;
                ++m_completedCycles;

                sendLog("[Cycle " + std::to_string(m_currentCycle) + " Step 3] PASS - exit filled @ $" +
                        std::to_string(m_exitFillPrice) + " cycleRealizedPnL=" + std::to_string(cycleRealizedPnL) +
                        " cumulativeRealizedPnL=" + std::to_string(m_cumulativeRealizedPnL));

                scheduleNextCycleOrComplete();
            }
            else if (p_order.status() == Protocol::ORDER_STATUS_REJECTED)
            {
                fail("[Cycle " + std::to_string(m_currentCycle) + " Step 3] FAIL - exit order rejected");
            }
        }

        void scheduleNextCycleOrComplete()
        {
            stopTimer(m_holdFlatTimerId);
            m_state = State::HoldFlat;

            if (m_holdFlatSeconds == 0)
            {
                startNextCycle();
                return;
            }

            const bool finalCycle = (m_completedCycles >= m_cycleTarget);
            sendLog("[Cycle " + std::to_string(m_currentCycle) + " Step 4] Waiting flat for " +
                    std::to_string(m_holdFlatSeconds) + "s" +
                    (finalCycle ? " before stopping" : " before the next buy"));
            m_holdFlatTimerId = m_runtime->startTimer(std::chrono::seconds(m_holdFlatSeconds),
                                                      [this]()
                                                      {
                                                          if (m_state != State::HoldFlat)
                                                          {
                                                              return;
                                                          }

                                                          startNextCycle();
                                                      });
        }

        void complete()
        {
            stopAllTimers();
            m_state = State::Complete;
            printSummary();
        }

        void fail(const std::string& p_message)
        {
            sendLog(p_message, Protocol::LOG_LEVEL_ERROR);
            stopAllTimers();
            m_state = State::Complete;
            printSummary();
        }

        void printSummary()
        {
            const std::size_t cycleTarget = static_cast<std::size_t>(m_cycleTarget);

            sendLog("");
            sendLog("========================================");
            sendLog("   ORDER TEST STRATEGY VIEW RESULTS");
            sendLog("========================================");
            sendLog("  Cycles requested         : " + std::to_string(m_cycleTarget));
            sendLog("  Cycles completed         : " + std::to_string(m_completedCycles));
            sendLog("  Entry orders placed      : " + std::to_string(m_entryOrderPlacedCount));
            sendLog("  Entry orders filled      : " + std::to_string(m_entryOrderFilledCount));
            sendLog("  Exit orders placed       : " + std::to_string(m_exitOrderPlacedCount));
            sendLog("  Exit orders filled       : " + std::to_string(m_exitOrderFilledCount));
            sendLog("  Open position updates    : " + std::to_string(m_openPositionUpdateCount));
            sendLog("  Flat position updates    : " + std::to_string(m_flatPositionUpdateCount));
            sendLog("  Cumulative realized PnL  : " + std::to_string(m_cumulativeRealizedPnL));
            sendLog("========================================");

            const bool allPassed = (m_completedCycles == m_cycleTarget) && (m_entryOrderFilledCount == cycleTarget) &&
                                   (m_exitOrderFilledCount == cycleTarget) && (m_openPositionUpdateCount > 0) &&
                                   m_runtime->positions().empty();
            sendLog(allPassed ? "  OVERALL: READY FOR STRATEGY VIEW TESTING"
                              : "  OVERALL: STRATEGY VIEW EXERCISE INCOMPLETE");
            sendLog("========================================");
            sendLog("");
            sendLog("[Final State] orders=" + std::to_string(m_runtime->orders().size()) +
                    " positions=" + std::to_string(m_runtime->positions().size()) + " balance=$" +
                    std::to_string(m_runtime->cashBalance()));
        }

        void stopAllTimers()
        {
            stopTimer(m_timeoutTimerId);
            stopTimer(m_startupDelayTimerId);
            stopTimer(m_intentRetryTimerId);
            stopTimer(m_holdOpenTimerId);
            stopTimer(m_holdFlatTimerId);
        }

        void stopTimer(OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime::TimerId& p_timerId)
        {
            if (m_runtime == nullptr || p_timerId == 0)
            {
                return;
            }

            (void)m_runtime->cancelTimer(p_timerId);
            p_timerId = 0;
        }

        void sendLog(const std::string& p_message, const Protocol::LogLevel p_level = Protocol::LOG_LEVEL_INFO)
        {
            if (m_runtime == nullptr || m_runtime->log(p_message, p_level))
            {
                return;
            }

            std::cerr << "[OrderTestStrategyProcess] Failed to send log: " << p_message << std::endl;
        }

        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
        State m_state = State::Init;
        std::string m_symbol = "SPY";
        std::string m_accountId = "SIM123456";
        std::uint32_t m_quantity = 1;
        int m_cycleTarget = 3;
        int m_currentCycle = 0;
        int m_completedCycles = 0;
        int m_holdOpenSeconds = 8;
        int m_holdFlatSeconds = 5;
        int m_startupDelaySeconds = 3;
        bool m_startupDelayConsumed = false;
        double m_aggressiveLimitOffsetCents = 5.0;
        double m_bestBidPrice = 0.0;
        double m_bestAskPrice = 0.0;
        double m_lastTradePrice = 0.0;
        std::int64_t m_lastObservedUnixNanos = 0;

        std::string m_entryOrderId;
        std::string m_exitOrderId;
        std::size_t m_entryOrderPlacedCount = 0;
        std::size_t m_entryOrderFilledCount = 0;
        std::size_t m_exitOrderPlacedCount = 0;
        std::size_t m_exitOrderFilledCount = 0;
        std::size_t m_openPositionUpdateCount = 0;
        std::size_t m_flatPositionUpdateCount = 0;
        double m_entryFillPrice = 0.0;
        double m_exitFillPrice = 0.0;
        double m_cumulativeRealizedPnL = 0.0;

        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime::TimerId m_timeoutTimerId = 0;
        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime::TimerId m_startupDelayTimerId = 0;
        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime::TimerId m_intentRetryTimerId = 0;
        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime::TimerId m_holdOpenTimerId = 0;
        OpenTraderPlatform::StrategySDK::ExternalStrategyRuntime::TimerId m_holdFlatTimerId = 0;
    };
} // namespace

int main(int argc, char** argv)
{
    OrderTestStrategyProcess strategy;
    return OpenTraderPlatform::StrategySDK::runStrategyProcessMain(argc, argv, describeStrategy(), strategy);
}
