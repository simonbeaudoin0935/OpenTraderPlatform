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

#include "L2Trader/StrategyProtocol/GeneratedProtocol.h"
#include "L2Trader/StrategySDK/RuntimeEnvironment.h"
#include "L2Trader/StrategySDK/UnixSocketConnection.h"

namespace L2Trader::StrategySDK
{
    namespace Protocol = l2trader::strategy::v1;

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

    class ExternalStrategyHandler
    {
      public:
        virtual ~ExternalStrategyHandler() = default;

        [[nodiscard]] virtual std::string strategyName() const = 0;
        [[nodiscard]] virtual std::string strategyVersion() const = 0;

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
    };

    class ExternalStrategyRuntime
    {
      public:
        using TimerId = std::uint64_t;

        explicit ExternalStrategyRuntime(ExternalStrategyHandler& p_handler);

        [[nodiscard]] int run();
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
                                             std::string_view p_requestId = {});

        [[nodiscard]] std::string cancelOrder(std::string_view p_orderId, std::string_view p_requestId = {});

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
        [[nodiscard]] bool pumpUntilResponse(std::string_view p_correlationId,
                                             Protocol::HostToStrategyEnvelope::PayloadCase p_payloadCase,
                                             Protocol::HostToStrategyEnvelope* p_responseEnvelope);
        [[nodiscard]] DispatchResult dispatchEnvelope(const Protocol::HostToStrategyEnvelope& p_envelope);
        void executeDueTimers();
        [[nodiscard]] int nextTimerTimeoutMs() const;
        [[nodiscard]] std::string nextCorrelationId(std::string_view p_prefix);

        ExternalStrategyHandler& m_handler;
        std::optional<RuntimeEnvironment> m_environment;
        UnixSocketConnection m_connection;
        std::uint64_t m_outboundSequence = 1;
        TimerId m_nextTimerId = 1;
        std::optional<Protocol::StrategyConfiguration> m_configuration;
        std::unordered_map<std::string, Protocol::OrderUpdate> m_orders;
        std::unordered_map<std::string, Protocol::PositionUpdate> m_positions;
        std::vector<TimerEntry> m_timers;
        double m_cashBalance = 0.0;
        std::mutex m_writeMutex;
        std::thread::id m_ownerThreadId;
    };
} // namespace L2Trader::StrategySDK
