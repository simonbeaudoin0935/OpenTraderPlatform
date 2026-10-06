#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "OpenTraderPlatform/StrategyProtocol/GeneratedProtocol.h"
#include "OpenTraderPlatform/StrategySDK/RuntimeEnvironment.h"
#include "OpenTraderPlatform/StrategySDK/StrategyDescription.h"
#include "OpenTraderPlatform/StrategySDK/UnixSocketConnection.h"

namespace OpenTraderPlatform::StrategySDK
{
    namespace Protocol = opentraderplatform::strategy::v1;
    class ExternalStrategyRuntime;

    enum class BracketSide
    {
        Long,
        Short
    };

    enum class BracketExecutionPolicy
    {
        Auto,
        VirtualOnly,
        NativeOnly
    };

    struct ClaimSymbolsResult
    {
        std::vector<std::string> grantedSymbols;
        std::vector<std::string> rejectedSymbols;
    };

    struct HistoricalBarsResult
    {
        std::vector<Protocol::Bar> bars;
        std::optional<std::string> errorCode;
        std::optional<std::string> errorMessage;

        [[nodiscard]] bool hasError() const
        {
            return errorCode.has_value();
        }
    };

    struct ClosePositionItemResult
    {
        std::string positionId;
        std::string accountId;
        std::string symbol;
        std::string longShort;
        std::uint32_t quantity = 0;
        Protocol::OrderSide side = Protocol::ORDER_SIDE_UNSPECIFIED;
        Protocol::OrderType type = Protocol::ORDER_TYPE_UNSPECIFIED;
        std::optional<double> limitPrice;
        bool submitted = false;
        bool placementSucceeded = false;
        std::vector<std::string> orderIds;
        std::vector<std::string> brokerMessages;
        std::vector<std::string> brokerErrors;
        std::optional<std::string> errorCode;
        std::optional<std::string> errorMessage;
    };

    struct ClosePositionsResult
    {
        std::string requestId;
        std::string accountId;
        std::vector<std::string> requestedSymbols;
        std::string session;
        bool usesAggressiveLimitOrders = false;
        bool forcedDayPlus = false;
        double aggressivityOffsetCents = 0.0;
        std::uint32_t matchedPositionCount = 0;
        std::uint32_t submittedOrderCount = 0;
        std::vector<ClosePositionItemResult> items;
        std::optional<std::string> errorCode;
        std::optional<std::string> errorMessage;

        [[nodiscard]] bool hasError() const
        {
            return errorCode.has_value();
        }
    };

    class ExternalStrategyHandler
    {
      public:
        virtual ~ExternalStrategyHandler() = default;

        virtual void bindRuntime(ExternalStrategyRuntime* p_runtime)
        {
            (void)p_runtime;
        }

        virtual void onStart(const Protocol::StrategyConfiguration& p_configuration) = 0;
        virtual void onPause(std::string_view p_reason)
        {
            (void)p_reason;
        }
        virtual void onResume(std::string_view p_reason)
        {
            (void)p_reason;
        }
        virtual void onStop(std::string_view p_reason)
        {
            (void)p_reason;
        }
        virtual void onShutdown(std::string_view p_reason)
        {
            onStop(p_reason);
        }
        virtual void onHeartbeat(const Protocol::Heartbeat& p_heartbeat)
        {
            (void)p_heartbeat;
        }

        virtual void onBar(const Protocol::Bar& p_bar)
        {
            (void)p_bar;
        }
        virtual void onLevel2Snapshot(const Protocol::Level2Snapshot& p_level2)
        {
            (void)p_level2;
        }
        virtual void onTrade(const Protocol::Trade& p_trade)
        {
            (void)p_trade;
        }
        virtual void onOrderUpdate(const Protocol::OrderUpdate& p_order)
        {
            (void)p_order;
        }
        virtual void onPositionUpdate(const Protocol::PositionUpdate& p_position)
        {
            (void)p_position;
        }
        virtual void onBalanceUpdate(const Protocol::BalanceUpdate& p_balance)
        {
            (void)p_balance;
        }
        virtual void onHostError(const Protocol::ErrorMessage& p_error)
        {
            (void)p_error;
        }
        virtual void onOrderConfirmationUpdate(const Protocol::OrderConfirmationUpdate& p_update)
        {
            (void)p_update;
        }
        virtual void onClaimedSymbolsUpdate(const Protocol::ClaimedSymbolsUpdate& p_update)
        {
            (void)p_update;
        }
    };

    class ExternalStrategyRuntime
    {
      public:
        using TimerId = std::uint64_t;

        explicit ExternalStrategyRuntime(const StrategyDescription& p_description, ExternalStrategyHandler& p_handler);

        [[nodiscard]] int run();
        /// Fail execution from a runtime callback; reports the reason and exits without a crash.
        void fail(const std::string& p_reason);
        [[nodiscard]] bool hasFailed() const
        {
            return m_failureReason.has_value();
        }
        void close();

        [[nodiscard]] bool isConnected() const
        {
            return m_connection.isOpen();
        }

        [[nodiscard]] const RuntimeEnvironment* environment() const
        {
            return m_environment ? &m_environment.value() : nullptr;
        }

        [[nodiscard]] const Protocol::StrategyConfiguration* configuration() const
        {
            return m_configuration ? &m_configuration.value() : nullptr;
        }

        [[nodiscard]] const StrategyDescription& description() const
        {
            return m_description;
        }

        [[nodiscard]] double cashBalance() const
        {
            return m_cashBalance;
        }

        [[nodiscard]] const std::unordered_map<std::string, Protocol::OrderUpdate>& orders() const
        {
            return m_orders;
        }

        [[nodiscard]] const std::unordered_map<std::string, Protocol::PositionUpdate>& positions() const
        {
            return m_positions;
        }

        [[nodiscard]] bool log(const std::string& p_message, Protocol::LogLevel p_level = Protocol::LOG_LEVEL_INFO);

        [[nodiscard]] ClaimSymbolsResult claimSymbols(const std::vector<std::string>& p_symbols);
        [[nodiscard]] std::optional<std::int64_t> requestCurrentTimeUnixNanos();
        [[nodiscard]] bool logToChart(std::string_view p_symbol, std::string_view p_message);
        [[nodiscard]] bool requestChartDisplaySwitch(std::string_view p_symbol, std::string_view p_reason = {});
        [[nodiscard]] bool setChartStatus(std::string_view p_symbol, std::string_view p_message);
        [[nodiscard]] bool clearChartStatus(std::string_view p_symbol);
        [[nodiscard]] std::string
        upsertManagedBracket(std::string_view p_symbol,
                             std::string_view p_accountId,
                             BracketSide p_side,
                             double p_stopPrice,
                             double p_takePrice,
                             BracketExecutionPolicy p_executionPolicy = BracketExecutionPolicy::Auto,
                             std::string_view p_requestId = {});
        [[nodiscard]] std::string upsertManagedBracket(std::string_view p_symbol,
                                                       std::string_view p_accountId,
                                                       BracketSide p_side,
                                                       double p_stopPrice,
                                                       double p_takePrice,
                                                       BracketExecutionPolicy p_executionPolicy,
                                                       std::optional<double> p_referenceEntryPrice,
                                                       std::string_view p_requestId = {});
        [[nodiscard]] std::string cancelManagedBracket(std::string_view p_symbol,
                                                       std::string_view p_accountId,
                                                       std::string_view p_requestId = {});

        [[nodiscard]] HistoricalBarsResult requestHistoricalBars(std::string_view p_symbol,
                                                                 std::int64_t p_sessionDayUnixNanos,
                                                                 std::int64_t p_firstBarUnixNanos,
                                                                 std::int64_t p_lastBarUnixNanos,
                                                                 std::string_view p_timeframe);

        [[nodiscard]] std::string placeOrder(std::string_view p_symbol,
                                             std::string_view p_accountId,
                                             Protocol::OrderSide p_side,
                                             Protocol::OrderType p_type,
                                             std::uint32_t p_quantity,
                                             std::optional<double> p_limitPrice = std::nullopt,
                                             std::optional<double> p_stopPrice = std::nullopt,
                                             Protocol::OrderDuration p_duration = Protocol::ORDER_DURATION_UNSPECIFIED,
                                             std::string_view p_requestId = {});

        [[nodiscard]] std::string
        placeOrderWithUserConfirmation(std::string_view p_symbol,
                                       std::string_view p_accountId,
                                       Protocol::OrderSide p_side,
                                       Protocol::OrderType p_type,
                                       std::uint32_t p_quantity,
                                       std::optional<double> p_limitPrice = std::nullopt,
                                       std::optional<double> p_stopPrice = std::nullopt,
                                       Protocol::OrderDuration p_duration = Protocol::ORDER_DURATION_UNSPECIFIED,
                                       std::string_view p_promptText = {},
                                       std::string_view p_requestId = {});

        /// Place an order requiring manual confirmation, with final execution
        /// price/type resolved by the platform at accept time.
        [[nodiscard]] std::string placeOrderWithUserConfirmationMarketable(
            std::string_view p_symbol,
            std::string_view p_accountId,
            Protocol::OrderSide p_side,
            Protocol::OrderType p_type,
            std::uint32_t p_quantity,
            std::optional<double> p_limitPrice = std::nullopt,
            std::optional<double> p_stopPrice = std::nullopt,
            Protocol::OrderDuration p_duration = Protocol::ORDER_DURATION_UNSPECIFIED,
            std::string_view p_promptText = {},
            std::string_view p_requestId = {});

        [[nodiscard]] std::string cancelOrder(std::string_view p_orderId, std::string_view p_requestId = {});

        [[nodiscard]] ClosePositionsResult closePositions(std::string_view p_accountId,
                                                          const std::vector<std::string>& p_symbols = {},
                                                          std::string_view p_requestId = {});

        [[nodiscard]] TimerId
        startTimer(std::chrono::milliseconds p_delay, std::function<void()> p_callback, bool p_repeat = false);
        [[nodiscard]] bool cancelTimer(TimerId p_timerId);

      private:
        enum class DispatchResult
        {
            Continue,
            StopRequested,
            ShutdownRequested,
            Failure,
        };

        struct TimerEntry
        {
            TimerId id = 0;
            std::chrono::steady_clock::time_point nextFireAt;
            std::chrono::milliseconds interval{0};
            bool repeat = false;
            std::function<void()> callback;
        };

        [[nodiscard]] bool connectAndHandshake();
        [[nodiscard]] bool sendHandshakeHello();
        [[nodiscard]] bool readEnvelope(Protocol::HostToStrategyEnvelope* p_envelope);
        [[nodiscard]] bool sendEnvelope(Protocol::StrategyToHostEnvelope p_envelope);
        [[nodiscard]] bool sendHeartbeatReply(const Protocol::Heartbeat& p_heartbeat);
        [[nodiscard]] bool sendStrategyReady();
        [[nodiscard]] bool pumpUntilResponse(std::string_view p_correlationId,
                                             Protocol::HostToStrategyEnvelope::PayloadCase p_payloadCase,
                                             Protocol::HostToStrategyEnvelope* p_responseEnvelope);
        [[nodiscard]] DispatchResult dispatchEnvelope(const Protocol::HostToStrategyEnvelope& p_envelope);
        [[nodiscard]] DispatchResult dispatchEnvelopeImpl(const Protocol::HostToStrategyEnvelope& p_envelope);
        void executeDueTimers();
        [[nodiscard]] int nextTimerTimeoutMs() const;
        [[nodiscard]] std::string nextCorrelationId(std::string_view p_prefix);

        StrategyDescription m_description;
        ExternalStrategyHandler& m_handler;
        std::optional<RuntimeEnvironment> m_environment;
        UnixSocketConnection m_connection;
        std::uint64_t m_outboundSequence = 1;
        TimerId m_nextTimerId = 1;
        std::optional<Protocol::StrategyConfiguration> m_configuration;
        std::optional<std::string> m_failureReason;
        std::unordered_map<std::string, Protocol::OrderUpdate> m_orders;
        std::unordered_map<std::string, Protocol::PositionUpdate> m_positions;
        std::vector<TimerEntry> m_timers;
        double m_cashBalance = 0.0;
        std::mutex m_writeMutex;
        std::thread::id m_ownerThreadId;
    };
} // namespace OpenTraderPlatform::StrategySDK
