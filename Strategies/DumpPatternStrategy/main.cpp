#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include <google/protobuf/struct.pb.h>

#include "L2Trader/StrategySDK/ExternalStrategyRuntime.h"

namespace
{
    namespace Protocol = l2trader::strategy::v1;

    constexpr std::string_view kStrategyName = "DumpPatternStrategyProcess";
    constexpr std::string_view kStrategyVersion = "1.0.0";
    constexpr std::int64_t kNanosPerSecond = 1000000000LL;

    [[nodiscard]] std::string getStringParam(const Protocol::StrategyConfiguration& p_configuration,
                                             std::string_view p_key,
                                             std::string_view p_fallback)
    {
        const auto fieldsIt = p_configuration.custom_params().fields().find(std::string(p_key));
        if (fieldsIt == p_configuration.custom_params().fields().end())
        {
            return std::string(p_fallback);
        }

        const google::protobuf::Value& value = fieldsIt->second;
        return value.kind_case() == google::protobuf::Value::kStringValue ? value.string_value()
                                                                          : std::string(p_fallback);
    }

    [[nodiscard]] double getNumberParam(const Protocol::StrategyConfiguration& p_configuration,
                                        std::string_view p_key,
                                        const double p_fallback)
    {
        const auto fieldsIt = p_configuration.custom_params().fields().find(std::string(p_key));
        if (fieldsIt == p_configuration.custom_params().fields().end())
        {
            return p_fallback;
        }

        const google::protobuf::Value& value = fieldsIt->second;
        return value.kind_case() == google::protobuf::Value::kNumberValue ? value.number_value() : p_fallback;
    }

    [[nodiscard]] int
    getIntParam(const Protocol::StrategyConfiguration& p_configuration, std::string_view p_key, const int p_fallback)
    {
        return static_cast<int>(getNumberParam(p_configuration, p_key, p_fallback));
    }

    [[nodiscard]] std::string formatPrice(const double p_value)
    {
        std::ostringstream stream;
        stream.setf(std::ios::fixed);
        stream.precision(2);
        stream << p_value;
        return stream.str();
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

    class DumpPatternStrategyProcess final : public L2Trader::StrategySDK::ExternalStrategyHandler
    {
      public:
        [[nodiscard]] std::string strategyName() const override
        {
            return std::string(kStrategyName);
        }

        [[nodiscard]] std::string strategyVersion() const override
        {
            return std::string(kStrategyVersion);
        }

        void setRuntime(L2Trader::StrategySDK::ExternalStrategyRuntime* const p_runtime)
        {
            m_runtime = p_runtime;
        }

        void onStart(const Protocol::StrategyConfiguration& p_configuration) override
        {
            m_symbol = p_configuration.symbols_size() > 0 ? p_configuration.symbols(0) : "NVDA";
            m_accountId = getStringParam(p_configuration, "accountID", "SIM123456");
            m_entryOffsetDollars = getNumberParam(p_configuration, "entryOffsetCents", 10.0) / 100.0;
            m_cycleDurationSec = std::max(1, getIntParam(p_configuration, "cycleDurationSeconds", 10));
            m_quantity = static_cast<std::uint32_t>(std::max(1, getIntParam(p_configuration, "quantity", 1)));

            sendLog("DumpPatternStrategyProcess started - offset=$" + formatPrice(m_entryOffsetDollars) +
                    ", phase=" + std::to_string(m_cycleDurationSec) + "s, qty=" + std::to_string(m_quantity));

            const auto claimResult = m_runtime->claimSymbols({m_symbol});
            if (claimResult.grantedSymbols.empty())
            {
                sendLog("ERROR: symbol " + m_symbol + " not granted", Protocol::LOG_LEVEL_ERROR);
                return;
            }

            const auto currentTime = requestCurrentTime("startup");
            if (!currentTime.has_value())
            {
                return;
            }

            m_cycleStartUnixNanos = *currentTime;
            m_state = State::Idle;
            sendLog("Subscribed to " + m_symbol + " - waiting for first price ...");
        }

        void onStop(std::string_view p_reason) override
        {
            sendLog("DumpPatternStrategyProcess stopping ...");
            if (!m_entryOrderId.empty() && m_state == State::WaitingLimitEntry)
            {
                sendLog("Cancelling open entry order " + m_entryOrderId + " on stop");
                std::ignore = m_runtime->cancelOrder(m_entryOrderId);
                m_entryOrderId.clear();
            }

            sendSummary();
            sendLog("[Stop] " + std::string(p_reason));
        }

        void onShutdown(std::string_view p_reason) override
        {
            onStop(p_reason);
        }

        void onBar(const Protocol::Bar& p_bar) override
        {
            if (m_lastPrice <= 0.0)
            {
                updatePrice(p_bar.close());
                return;
            }

            checkCycle();
        }

        void onTrade(const Protocol::Trade& p_trade) override
        {
            updatePrice(p_trade.price());
        }

        void onOrderUpdate(const Protocol::OrderUpdate& p_order) override
        {
            if (p_order.symbol() != m_symbol)
            {
                return;
            }

            sendLog("[Order] id=" + p_order.order_id() + " status=" + orderStatusToString(p_order.status()),
                    Protocol::LOG_LEVEL_DEBUG);

            if (m_state == State::WaitingLimitEntry && (m_entryOrderId.empty() || p_order.order_id() == m_entryOrderId))
            {
                handleEntryOrderUpdate(p_order);
                return;
            }

            if (m_state == State::WaitingClose && (m_closeOrderId.empty() || p_order.order_id() == m_closeOrderId))
            {
                handleCloseOrderUpdate(p_order);
            }
        }

        void onHostError(const Protocol::ErrorMessage& p_error) override
        {
            sendLog("Host error: " + p_error.code() + " - " + p_error.message(), Protocol::LOG_LEVEL_ERROR);
        }

      private:
        enum class State
        {
            Idle,
            WaitingLimitEntry,
            EntryFilled,
            WaitingClose,
        };

        void handleEntryOrderUpdate(const Protocol::OrderUpdate& p_order)
        {
            if (m_entryOrderId.empty())
            {
                m_entryOrderId = p_order.order_id();
                sendLog("Entry order placed: " + m_entryOrderId);
            }

            if (p_order.status() == Protocol::ORDER_STATUS_FILLED)
            {
                m_entryFillPrice = p_order.has_average_fill_price() ? p_order.average_fill_price() : m_lastPrice;
                sendLog("Entry order " + m_entryOrderId + " filled at " + formatPrice(m_entryFillPrice));
                m_state = State::EntryFilled;

                const auto currentTime = requestCurrentTime("entry fill");
                if (!currentTime.has_value())
                {
                    return;
                }

                m_cycleStartUnixNanos = *currentTime;
                sendLog("Waiting " + std::to_string(m_cycleDurationSec) + "s before placing close order ...");
                return;
            }

            if (p_order.status() == Protocol::ORDER_STATUS_CANCELLED)
            {
                sendLog("Entry order " + m_entryOrderId + " cancelled");
                m_cyclesCancelled++;
                resetCycle();
                return;
            }

            if (p_order.status() == Protocol::ORDER_STATUS_REJECTED)
            {
                const std::string reason = p_order.has_error() ? p_order.error().message() : std::string("unknown");
                sendLog("Entry order " + m_entryOrderId + " rejected: " + reason, Protocol::LOG_LEVEL_WARNING);
                m_cyclesCancelled++;
                resetCycle();
            }
        }

        void handleCloseOrderUpdate(const Protocol::OrderUpdate& p_order)
        {
            if (m_closeOrderId.empty())
            {
                m_closeOrderId = p_order.order_id();
                sendLog("Close order placed: " + m_closeOrderId);
            }

            if (p_order.status() == Protocol::ORDER_STATUS_FILLED)
            {
                const double closeFill = p_order.has_average_fill_price() ? p_order.average_fill_price() : m_lastPrice;
                const double pnl = m_isLong ? (closeFill - m_entryFillPrice) : (m_entryFillPrice - closeFill);
                m_totalPnL += pnl;
                m_cyclesCompleted++;

                sendLog("Close order " + m_closeOrderId + " filled at " + formatPrice(closeFill) + " | cycle P&L=$" +
                        formatPrice(pnl) + " | total P&L=$" + formatPrice(m_totalPnL));
                sendChartLog("Cycle #" + std::to_string(m_cyclesCompleted) +
                             " complete: " + std::string(pnl >= 0.0 ? "WIN" : "LOSS") + " P&L = $" + formatPrice(pnl) +
                             " (entry $" + formatPrice(m_entryFillPrice) + " -> exit $" + formatPrice(closeFill) + ")");
                resetCycle();
                return;
            }

            if (p_order.status() == Protocol::ORDER_STATUS_REJECTED)
            {
                const std::string reason = p_order.has_error() ? p_order.error().message() : std::string("unknown");
                sendLog("Close order " + m_closeOrderId + " rejected: " + reason, Protocol::LOG_LEVEL_WARNING);
                resetCycle();
            }
        }

        void updatePrice(const double p_price)
        {
            if (p_price > 0.0)
            {
                m_lastPrice = p_price;
            }

            checkCycle();
        }

        void checkCycle()
        {
            if (m_cycleStartUnixNanos == 0)
            {
                return;
            }

            const auto currentTime = requestCurrentTime("check cycle");
            if (!currentTime.has_value())
            {
                return;
            }

            const std::int64_t elapsedSec = (*currentTime - m_cycleStartUnixNanos) / kNanosPerSecond;
            if (elapsedSec < m_cycleDurationSec)
            {
                return;
            }

            switch (m_state)
            {
            case State::Idle:
                startEntryPhase();
                break;
            case State::WaitingLimitEntry:
                evaluateEntryPhase();
                break;
            case State::EntryFilled:
                startClosePhase();
                break;
            case State::WaitingClose:
                evaluateClosePhase();
                break;
            }
        }

        void startEntryPhase()
        {
            if (m_lastPrice <= 0.0)
            {
                sendLog("No price available yet - skipping cycle");
                const auto currentTime = requestCurrentTime("skip cycle");
                if (currentTime.has_value())
                {
                    m_cycleStartUnixNanos = *currentTime;
                }
                return;
            }

            sendChartLog("Cycle #" + std::to_string(nextCycleNumber()) + ": initiating " +
                         std::string(m_isLong ? "LONG" : "SHORT") + " entry @ $" + formatPrice(m_lastPrice));

            m_state = State::WaitingLimitEntry;
            if (!placeEntryOrder())
            {
                m_cyclesCancelled++;
                resetCycle();
                return;
            }

            const auto currentTime = requestCurrentTime("entry start");
            if (currentTime.has_value())
            {
                m_cycleStartUnixNanos = *currentTime;
            }
        }

        void evaluateEntryPhase()
        {
            sendLog("Entry order not filled after " + std::to_string(m_cycleDurationSec) + "s - cancelling");
            sendChartLog("Entry timed out after " + std::to_string(m_cycleDurationSec) + "s - cancelling " +
                         std::string(m_isLong ? "BUY" : "SELL SHORT") + " order");
            cancelEntryOrder();
            m_cyclesCancelled++;
            resetCycle();
        }

        void startClosePhase()
        {
            m_state = State::WaitingClose;
            const auto currentTime = requestCurrentTime("close start");
            if (!currentTime.has_value())
            {
                return;
            }

            m_cycleStartUnixNanos = *currentTime;
            sendChartLog("Closing " + std::string(m_isLong ? "LONG" : "SHORT") + " position - entry was @ $" +
                         formatPrice(m_entryFillPrice));

            if (!placeCloseOrder())
            {
                resetCycle();
            }
        }

        void evaluateClosePhase()
        {
            sendLog("Close order not confirmed after " + std::to_string(m_cycleDurationSec) +
                        "s - resetting cycle (position may remain open)",
                    Protocol::LOG_LEVEL_WARNING);
            m_closeOrderId.clear();
            resetCycle();
        }

        void resetCycle()
        {
            m_isLong = !m_isLong;
            m_entryOrderId.clear();
            m_closeOrderId.clear();
            m_entryFillPrice = 0.0;
            m_state = State::Idle;

            const auto currentTime = requestCurrentTime("reset cycle");
            m_cycleStartUnixNanos = currentTime.value_or(0);
            sendLog("Cycle reset - next direction: " + std::string(m_isLong ? "LONG" : "SHORT"));
        }

        [[nodiscard]] bool placeEntryOrder()
        {
            const double limitPrice =
                m_isLong ? (m_lastPrice + m_entryOffsetDollars) : (m_lastPrice - m_entryOffsetDollars);
            sendLog("Placing " + std::string(m_isLong ? "BUY" : "SELL SHORT") + " LIMIT order for " + m_symbol +
                    " @ $" + formatPrice(limitPrice) + " (last=$" + formatPrice(m_lastPrice) + ")");

            const std::string requestId =
                m_runtime->placeOrder(m_symbol,
                                      m_accountId,
                                      m_isLong ? Protocol::ORDER_SIDE_BUY : Protocol::ORDER_SIDE_SELL_SHORT,
                                      Protocol::ORDER_TYPE_LIMIT,
                                      m_quantity,
                                      limitPrice);
            if (requestId.empty())
            {
                sendLog("Failed to send entry order intent", Protocol::LOG_LEVEL_ERROR);
                return false;
            }

            return true;
        }

        [[nodiscard]] bool placeCloseOrder()
        {
            sendLog("Placing " + std::string(m_isLong ? "SELL" : "BUY TO COVER") + " MARKET order for " + m_symbol +
                    " (close position)");

            const std::string requestId =
                m_runtime->placeOrder(m_symbol,
                                      m_accountId,
                                      m_isLong ? Protocol::ORDER_SIDE_SELL : Protocol::ORDER_SIDE_BUY_TO_COVER,
                                      Protocol::ORDER_TYPE_MARKET,
                                      m_quantity);
            if (requestId.empty())
            {
                sendLog("Failed to send close order intent", Protocol::LOG_LEVEL_ERROR);
                return false;
            }

            return true;
        }

        void cancelEntryOrder()
        {
            if (m_entryOrderId.empty())
            {
                sendLog("Entry order id unavailable - cannot send cancel", Protocol::LOG_LEVEL_WARNING);
                return;
            }

            sendLog("Cancelling entry order " + m_entryOrderId);
            if (m_runtime->cancelOrder(m_entryOrderId).empty())
            {
                sendLog("Failed to send cancel intent for " + m_entryOrderId, Protocol::LOG_LEVEL_WARNING);
            }
        }

        [[nodiscard]] std::optional<std::int64_t> requestCurrentTime(std::string_view p_context)
        {
            const auto currentTime = m_runtime->requestCurrentTimeUnixNanos();
            if (!currentTime.has_value())
            {
                sendLog("Failed to request host current time during " + std::string(p_context),
                        Protocol::LOG_LEVEL_ERROR);
            }
            return currentTime;
        }

        void sendChartLog(const std::string& p_message)
        {
            if (!m_runtime->logToChart(m_symbol, p_message))
            {
                sendLog("Failed to send chart log: " + p_message, Protocol::LOG_LEVEL_WARNING);
            }
        }

        void sendLog(const std::string& p_message, const Protocol::LogLevel p_level = Protocol::LOG_LEVEL_INFO) const
        {
            if (m_runtime != nullptr && m_runtime->log(p_message, p_level))
            {
                return;
            }

            std::cerr << "[" << kStrategyName << "] Failed to send log: " << p_message << std::endl;
        }

        void sendSummary() const
        {
            sendLog("Summary: cycles completed=" + std::to_string(m_cyclesCompleted) + ", cycles cancelled=" +
                    std::to_string(m_cyclesCancelled) + ", total P&L=$" + formatPrice(m_totalPnL));
        }

        [[nodiscard]] int nextCycleNumber() const
        {
            return m_cyclesCompleted + m_cyclesCancelled + 1;
        }

        L2Trader::StrategySDK::ExternalStrategyRuntime* m_runtime = nullptr;
        State m_state = State::Idle;
        bool m_isLong = true;
        std::string m_symbol = "NVDA";
        std::string m_accountId = "SIM123456";
        double m_entryOffsetDollars = 0.10;
        int m_cycleDurationSec = 10;
        std::uint32_t m_quantity = 1;
        double m_lastPrice = 0.0;
        std::int64_t m_cycleStartUnixNanos = 0;
        std::string m_entryOrderId;
        std::string m_closeOrderId;
        double m_entryFillPrice = 0.0;
        int m_cyclesCompleted = 0;
        int m_cyclesCancelled = 0;
        double m_totalPnL = 0.0;
    };
} // namespace

int main()
{
    DumpPatternStrategyProcess handler;
    L2Trader::StrategySDK::ExternalStrategyRuntime runtime(handler);
    handler.setRuntime(&runtime);
    return runtime.run();
}
