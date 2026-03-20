#include "L2Trader/StrategySDK/ExternalStrategyRuntime.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <utility>

#include "L2Trader/StrategyProtocol/ProtocolVersion.h"
#include "L2Trader/StrategySDK/MessageFraming.h"

namespace L2Trader::StrategySDK
{
    ExternalStrategyRuntime::ExternalStrategyRuntime(ExternalStrategyHandler& p_handler) : m_handler(p_handler) {}

    int ExternalStrategyRuntime::run()
    {
        if (!connectAndHandshake())
        {
            return 1;
        }

        m_ownerThreadId = std::this_thread::get_id();

        while (true)
        {
            executeDueTimers();

            const UnixSocketConnection::WaitStatus waitStatus = m_connection.waitForReadable(nextTimerTimeoutMs());
            if (waitStatus == UnixSocketConnection::WaitStatus::Timeout)
            {
                continue;
            }
            if (waitStatus == UnixSocketConnection::WaitStatus::Error)
            {
                std::cerr << "[ExternalStrategyRuntime] Socket closed unexpectedly while waiting for host message."
                          << std::endl;
                return 1;
            }

            Protocol::HostToStrategyEnvelope envelope;
            if (!readEnvelope(&envelope))
            {
                std::cerr << "[ExternalStrategyRuntime] Socket closed unexpectedly while waiting for host message."
                          << std::endl;
                return 1;
            }

            const DispatchResult result = dispatchEnvelope(envelope);
            switch (result)
            {
            case DispatchResult::Continue:
                continue;
            case DispatchResult::StopRequested:
            case DispatchResult::ShutdownRequested:
                return 0;
            case DispatchResult::Failure:
                return 1;
            }
        }
    }

    void ExternalStrategyRuntime::close()
    {
        m_connection.close();
    }

    bool ExternalStrategyRuntime::log(const std::string& p_message, const Protocol::LogLevel p_level)
    {
        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        auto* const logMessage = envelope.mutable_strategy_log();
        logMessage->set_level(p_level);
        logMessage->set_message(p_message);
        return sendEnvelope(std::move(envelope));
    }

    ClaimSymbolsResult ExternalStrategyRuntime::claimSymbols(const std::vector<std::string>& p_symbols)
    {
        ClaimSymbolsResult result;

        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        const std::string correlationId = nextCorrelationId("claim-symbols");
        envelope.set_correlation_id(correlationId);

        auto* const request = envelope.mutable_claim_symbols_request();
        for (const std::string& symbol: p_symbols)
        {
            request->add_symbols(symbol);
        }

        if (!sendEnvelope(std::move(envelope)))
        {
            return result;
        }

        Protocol::HostToStrategyEnvelope response;
        if (!pumpUntilResponse(correlationId, Protocol::HostToStrategyEnvelope::kClaimSymbolsResponse, &response))
        {
            return result;
        }

        for (const std::string& symbol: response.claim_symbols_response().granted_symbols())
        {
            result.grantedSymbols.push_back(symbol);
        }
        for (const std::string& symbol: response.claim_symbols_response().rejected_symbols())
        {
            result.rejectedSymbols.push_back(symbol);
        }
        return result;
    }

    std::optional<std::int64_t> ExternalStrategyRuntime::requestCurrentTimeUnixNanos()
    {
        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        const std::string correlationId = nextCorrelationId("current-time");
        envelope.set_correlation_id(correlationId);
        envelope.mutable_current_time_request();

        if (!sendEnvelope(std::move(envelope)))
        {
            return std::nullopt;
        }

        Protocol::HostToStrategyEnvelope response;
        if (!pumpUntilResponse(correlationId, Protocol::HostToStrategyEnvelope::kCurrentTimeResponse, &response))
        {
            return std::nullopt;
        }

        return response.current_time_response().current_unix_nanos();
    }

    bool ExternalStrategyRuntime::logToChart(std::string_view p_symbol, std::string_view p_message)
    {
        if (p_symbol.empty() || p_message.empty())
        {
            return false;
        }

        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        envelope.set_correlation_id(nextCorrelationId("chart-log"));

        auto* const intent = envelope.mutable_chart_log_intent();
        intent->set_symbol(std::string(p_symbol));
        intent->set_message(std::string(p_message));
        return sendEnvelope(std::move(envelope));
    }

    HistoricalBarsResult ExternalStrategyRuntime::requestHistoricalBars(std::string_view p_symbol,
                                                                        const std::int64_t p_sessionDayUnixNanos,
                                                                        const std::int64_t p_firstBarUnixNanos,
                                                                        const std::int64_t p_lastBarUnixNanos,
                                                                        std::string_view p_timeframe)
    {
        HistoricalBarsResult result;

        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        const std::string correlationId = nextCorrelationId("historical-bars");
        envelope.set_correlation_id(correlationId);

        auto* const request = envelope.mutable_historical_bars_request();
        request->set_symbol(std::string(p_symbol));
        request->set_session_day_unix_nanos(p_sessionDayUnixNanos);
        request->set_first_bar_unix_nanos(p_firstBarUnixNanos);
        request->set_last_bar_unix_nanos(p_lastBarUnixNanos);
        request->set_timeframe(std::string(p_timeframe));

        if (!sendEnvelope(std::move(envelope)))
        {
            result.errorCode = "socket_write_failed";
            result.errorMessage = "Failed to send historical bars request";
            return result;
        }

        Protocol::HostToStrategyEnvelope response;
        if (!pumpUntilResponse(correlationId, Protocol::HostToStrategyEnvelope::kHistoricalBarsResponse, &response))
        {
            result.errorCode = "socket_read_failed";
            result.errorMessage = "Failed while waiting for historical bars response";
            return result;
        }

        const auto& historical = response.historical_bars_response();
        if (historical.has_error())
        {
            result.errorCode = historical.error().code();
            result.errorMessage = historical.error().message();
            return result;
        }

        result.bars.reserve(static_cast<std::size_t>(historical.bars_size()));
        for (const Protocol::Bar& bar: historical.bars())
        {
            result.bars.push_back(bar);
        }
        return result;
    }

    std::string ExternalStrategyRuntime::placeOrder(std::string_view p_symbol,
                                                    std::string_view p_accountId,
                                                    const Protocol::OrderSide p_side,
                                                    const Protocol::OrderType p_type,
                                                    const std::uint32_t p_quantity,
                                                    const std::optional<double> p_limitPrice,
                                                    const std::optional<double> p_stopPrice,
                                                    std::string_view p_requestId)
    {
        const std::string requestId = p_requestId.empty() ? nextCorrelationId("place-order") : std::string(p_requestId);

        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        envelope.set_correlation_id(requestId);

        auto* const intent = envelope.mutable_place_order_intent();
        intent->set_request_id(requestId);
        intent->set_symbol(std::string(p_symbol));
        intent->set_account_id(std::string(p_accountId));
        intent->set_side(p_side);
        intent->set_type(p_type);
        intent->set_quantity(p_quantity);
        if (p_limitPrice.has_value())
        {
            intent->set_limit_price(*p_limitPrice);
        }
        if (p_stopPrice.has_value())
        {
            intent->set_stop_price(*p_stopPrice);
        }

        if (!sendEnvelope(std::move(envelope)))
        {
            return "";
        }

        return requestId;
    }

    std::string ExternalStrategyRuntime::cancelOrder(std::string_view p_orderId, std::string_view p_requestId)
    {
        const std::string requestId =
            p_requestId.empty() ? nextCorrelationId("cancel-order") : std::string(p_requestId);

        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        envelope.set_correlation_id(requestId);

        auto* const intent = envelope.mutable_cancel_order_intent();
        intent->set_request_id(requestId);
        intent->set_order_id(std::string(p_orderId));

        if (!sendEnvelope(std::move(envelope)))
        {
            return "";
        }

        return requestId;
    }

    ExternalStrategyRuntime::TimerId ExternalStrategyRuntime::startTimer(const std::chrono::milliseconds p_delay,
                                                                         std::function<void()> p_callback,
                                                                         const bool p_repeat)
    {
        if (!p_callback)
        {
            return 0;
        }

        const TimerId timerId = m_nextTimerId++;
        m_timers.push_back({
            .id = timerId,
            .nextFireAt = std::chrono::steady_clock::now() + p_delay,
            .interval = p_delay,
            .repeat = p_repeat,
            .callback = std::move(p_callback),
        });
        return timerId;
    }

    bool ExternalStrategyRuntime::cancelTimer(const TimerId p_timerId)
    {
        const auto originalSize = m_timers.size();
        std::erase_if(m_timers, [p_timerId](const TimerEntry& p_timer) { return p_timer.id == p_timerId; });
        return m_timers.size() != originalSize;
    }

    bool ExternalStrategyRuntime::connectAndHandshake()
    {
        if (m_connection.isOpen())
        {
            return true;
        }

        m_environment = loadRuntimeEnvironment();
        if (!m_environment.has_value())
        {
            std::cerr << "[ExternalStrategyRuntime] Missing strategy runtime environment." << std::endl;
            return false;
        }

        if (!m_connection.connectTo(m_environment->socketPath))
        {
            std::cerr << "[ExternalStrategyRuntime] Failed to connect to " << m_environment->socketPath << std::endl;
            return false;
        }

        if (!sendHandshakeHello())
        {
            std::cerr << "[ExternalStrategyRuntime] Failed to send handshake." << std::endl;
            return false;
        }

        return true;
    }

    bool ExternalStrategyRuntime::sendHandshakeHello()
    {
        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        envelope.set_correlation_id("handshake");

        auto* const handshake = envelope.mutable_handshake_hello();
        L2Trader::StrategyProtocol::populateProtocolVersion(handshake->mutable_protocol_version());
        handshake->set_sdk_name(std::string(kStrategySdkName));
        handshake->set_sdk_version(std::string(kStrategySdkVersion));
        handshake->set_strategy_name(m_handler.strategyName());
        handshake->set_strategy_version(m_handler.strategyVersion());

        return sendEnvelope(std::move(envelope));
    }

    bool ExternalStrategyRuntime::readEnvelope(Protocol::HostToStrategyEnvelope* const p_envelope)
    {
        if (p_envelope == nullptr)
        {
            return false;
        }

        std::vector<std::uint8_t> payload;
        if (!m_connection.readFrame(&payload))
        {
            return false;
        }

        return parseMessagePayload(payload, p_envelope);
    }

    bool ExternalStrategyRuntime::sendEnvelope(Protocol::StrategyToHostEnvelope p_envelope)
    {
        if (!m_connection.isOpen())
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_writeMutex);
        return m_connection.writeMessage(p_envelope);
    }

    bool ExternalStrategyRuntime::sendHeartbeatReply(const Protocol::Heartbeat& p_heartbeat)
    {
        Protocol::StrategyToHostEnvelope envelope;
        envelope.set_sequence(m_outboundSequence++);
        envelope.mutable_heartbeat()->set_monotonic_sequence(p_heartbeat.monotonic_sequence());
        return sendEnvelope(std::move(envelope));
    }

    bool ExternalStrategyRuntime::pumpUntilResponse(std::string_view p_correlationId,
                                                    const Protocol::HostToStrategyEnvelope::PayloadCase p_payloadCase,
                                                    Protocol::HostToStrategyEnvelope* const p_responseEnvelope)
    {
        if (p_responseEnvelope == nullptr)
        {
            return false;
        }

        while (true)
        {
            executeDueTimers();

            const UnixSocketConnection::WaitStatus waitStatus = m_connection.waitForReadable(nextTimerTimeoutMs());
            if (waitStatus == UnixSocketConnection::WaitStatus::Timeout)
            {
                continue;
            }
            if (waitStatus == UnixSocketConnection::WaitStatus::Error)
            {
                return false;
            }

            Protocol::HostToStrategyEnvelope envelope;
            if (!readEnvelope(&envelope))
            {
                return false;
            }

            if (envelope.correlation_id() == p_correlationId && envelope.payload_case() == p_payloadCase)
            {
                *p_responseEnvelope = std::move(envelope);
                return true;
            }

            const DispatchResult result = dispatchEnvelope(envelope);
            if (result != DispatchResult::Continue)
            {
                return false;
            }
        }
    }

    ExternalStrategyRuntime::DispatchResult
    ExternalStrategyRuntime::dispatchEnvelope(const Protocol::HostToStrategyEnvelope& p_envelope)
    {
        switch (p_envelope.payload_case())
        {
        case Protocol::HostToStrategyEnvelope::kHandshakeAck:
            if (!p_envelope.handshake_ack().accepted())
            {
                std::cerr << "[ExternalStrategyRuntime] Handshake rejected: "
                          << p_envelope.handshake_ack().rejection_reason() << std::endl;
                return DispatchResult::Failure;
            }
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kHeartbeat:
            m_handler.onHeartbeat(p_envelope.heartbeat());
            return sendHeartbeatReply(p_envelope.heartbeat()) ? DispatchResult::Continue : DispatchResult::Failure;

        case Protocol::HostToStrategyEnvelope::kStart:
            m_configuration = p_envelope.start().configuration();
            m_handler.onStart(*m_configuration);
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kPause:
            m_handler.onPause(p_envelope.pause().reason());
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kResume:
            m_handler.onResume({});
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kStop:
            m_handler.onStop(p_envelope.stop().reason());
            return DispatchResult::StopRequested;

        case Protocol::HostToStrategyEnvelope::kShutdown:
            m_handler.onShutdown(p_envelope.shutdown().reason());
            return DispatchResult::ShutdownRequested;

        case Protocol::HostToStrategyEnvelope::kBar:
            m_handler.onBar(p_envelope.bar());
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kLevel2Snapshot:
            m_handler.onLevel2Snapshot(p_envelope.level2_snapshot());
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kTrade:
            m_handler.onTrade(p_envelope.trade());
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kOrderUpdate:
        {
            const Protocol::OrderUpdate& order = p_envelope.order_update();
            m_orders[order.order_id()] = order;
            m_handler.onOrderUpdate(order);
            return DispatchResult::Continue;
        }

        case Protocol::HostToStrategyEnvelope::kPositionUpdate:
        {
            const Protocol::PositionUpdate& position = p_envelope.position_update();
            m_positions[position.symbol()] = position;
            m_handler.onPositionUpdate(position);
            return DispatchResult::Continue;
        }

        case Protocol::HostToStrategyEnvelope::kBalanceUpdate:
            m_cashBalance = p_envelope.balance_update().cash();
            m_handler.onBalanceUpdate(p_envelope.balance_update());
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kError:
            m_handler.onHostError(p_envelope.error());
            return DispatchResult::Continue;

        case Protocol::HostToStrategyEnvelope::kClaimSymbolsResponse:
        case Protocol::HostToStrategyEnvelope::kHistoricalBarsResponse:
        case Protocol::HostToStrategyEnvelope::kCurrentTimeResponse:
        case Protocol::HostToStrategyEnvelope::kHostLog:
        case Protocol::HostToStrategyEnvelope::PAYLOAD_NOT_SET:
        default:
            return DispatchResult::Continue;
        }
    }

    void ExternalStrategyRuntime::executeDueTimers()
    {
        while (true)
        {
            const auto now = std::chrono::steady_clock::now();
            auto timerIt = std::find_if(m_timers.begin(),
                                        m_timers.end(),
                                        [now](const TimerEntry& p_timer) { return p_timer.nextFireAt <= now; });
            if (timerIt == m_timers.end())
            {
                break;
            }

            TimerEntry timer = *timerIt;
            if (timer.repeat)
            {
                timerIt->nextFireAt = now + timer.interval;
            }
            else
            {
                m_timers.erase(timerIt);
            }

            timer.callback();
        }
    }

    int ExternalStrategyRuntime::nextTimerTimeoutMs() const
    {
        if (m_timers.empty())
        {
            return -1;
        }

        const auto soonestTimer = std::min_element(m_timers.begin(),
                                                   m_timers.end(),
                                                   [](const TimerEntry& p_left, const TimerEntry& p_right)
                                                   { return p_left.nextFireAt < p_right.nextFireAt; });
        const auto now = std::chrono::steady_clock::now();
        if (soonestTimer->nextFireAt <= now)
        {
            return 0;
        }

        const auto timeout =
            std::chrono::duration_cast<std::chrono::milliseconds>(soonestTimer->nextFireAt - now).count();
        if (timeout > static_cast<long long>(std::numeric_limits<int>::max()))
        {
            return std::numeric_limits<int>::max();
        }
        return static_cast<int>(timeout);
    }

    std::string ExternalStrategyRuntime::nextCorrelationId(const std::string_view p_prefix)
    {
        return std::string(p_prefix) + "-" + std::to_string(m_outboundSequence);
    }
} // namespace L2Trader::StrategySDK
