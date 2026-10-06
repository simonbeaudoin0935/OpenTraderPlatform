#include "ProcessStrategyRuntimeBackend.h"
#include "StrategyFrameReader.h"

#include <algorithm>
#include <QDir>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QEventLoop>
#include <QMetaObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QPromise>
#include <QTimer>
#include <QUuid>

#include <array>
#include <cerrno>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

#include <fcntl.h>

#ifdef FATAL
#undef FATAL
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef CRITICAL
#undef CRITICAL
#endif

#include <google/protobuf/message_lite.h>
#include <google/protobuf/struct.pb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "OpenTraderPlatform/StrategyProtocol/GeneratedProtocol.h"
#include "OpenTraderPlatform/StrategyProtocol/ProtocolVersion.h"
#include "OpenTraderPlatform/StrategySDK/MessageFraming.h"
#include "../Algo/MainAlgo.h"
#include "../Clients/TSClient/Brokerage/GetOrders/Order.h"
#include "../Clients/TSClient/Brokerage/StreamPositions/Position.h"
#include "../Clients/TSClient/OrderExecution/PlaceOrder/PlaceOrder.h"
#include "../Core/Models/Bar.h"
#include "../Core/Models/Level2.h"
#include "../Core/Models/Trade.h"
#include "../Core/MainApp.h"
#include "../Misc/Assume.h"
#include "../Misc/CONSTANTS.h"
#include "../Misc/TimeFrame.h"
#include "StrategyConfig.h"
#include "StrategyLogger.h"
#include "StrategyManager.h"
#include "StrategySDK.h"

Q_DECLARE_LOGGING_CATEGORY(StrategyManagerLog)

namespace
{
    namespace Protocol = opentraderplatform::strategy::v1;

    constexpr int kStartupTimeoutMs = 5000;
    constexpr int kShutdownTimeoutMs = 5000;
    constexpr int kTerminateTimeoutMs = 2000;
    constexpr int kSocketWriteTimeoutMs = 5000;
    constexpr std::size_t kSocketReadChunkSize = 4096;

    [[nodiscard]] bool waitForFd(const int p_fd, const short p_events, const int p_timeoutMs)
    {
        pollfd descriptor{};
        descriptor.fd = p_fd;
        descriptor.events = p_events;

        while (true)
        {
            const int result = ::poll(&descriptor, 1, p_timeoutMs);
            if (result > 0)
            {
                return (descriptor.revents & p_events) != 0;
            }

            if (result == 0)
            {
                return false;
            }

            if (errno != EINTR)
            {
                return false;
            }
        }
    }

    [[nodiscard]] bool writeAll(const int p_fd, const std::span<const std::uint8_t> p_buffer)
    {
        std::size_t bytesWritten = 0;
        while (bytesWritten < p_buffer.size())
        {
            const ssize_t result = ::write(p_fd, p_buffer.data() + bytesWritten, p_buffer.size() - bytesWritten);
            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    if (!waitForFd(p_fd, POLLOUT, kSocketWriteTimeoutMs))
                    {
                        return false;
                    }
                    continue;
                }

                return false;
            }

            if (result == 0)
            {
                return false;
            }

            bytesWritten += static_cast<std::size_t>(result);
        }

        return true;
    }

    [[nodiscard]] bool readAllWithTimeout(const int p_fd, const std::span<std::uint8_t> p_buffer, const int p_timeoutMs)
    {
        std::size_t bytesRead = 0;
        while (bytesRead < p_buffer.size())
        {
            if (!waitForFd(p_fd, POLLIN, p_timeoutMs))
            {
                return false;
            }

            const ssize_t result = ::read(p_fd, p_buffer.data() + bytesRead, p_buffer.size() - bytesRead);
            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                return false;
            }

            if (result == 0)
            {
                return false;
            }

            bytesRead += static_cast<std::size_t>(result);
        }

        return true;
    }

    [[nodiscard]] bool
    readFrameWithTimeout(const int p_fd, std::vector<std::uint8_t>* const p_payload, const int p_timeoutMs)
    {
        if (p_payload == nullptr)
        {
            return false;
        }

        std::array<std::uint8_t, OpenTraderPlatform::StrategySDK::kFramePrefixSize> prefix{};
        if (!readAllWithTimeout(p_fd, prefix, p_timeoutMs))
        {
            return false;
        }

        const std::uint32_t payloadSize = OpenTraderPlatform::StrategySDK::decodeFrameSize(prefix);
        if (payloadSize > OpenTraderPlatform::StrategySDK::kMaxFramePayloadSize)
        {
            qWarning("Strategy handshake frame exceeds the 16 MiB payload limit");
            return false;
        }
        p_payload->assign(payloadSize, std::uint8_t{0});
        if (payloadSize == 0)
        {
            return true;
        }

        return readAllWithTimeout(p_fd, *p_payload, p_timeoutMs);
    }

    [[nodiscard]] bool writeMessage(const int p_fd, const google::protobuf::MessageLite& p_message)
    {
        const std::vector<std::uint8_t> frame = OpenTraderPlatform::StrategySDK::serializeFramedMessage(p_message);
        if (frame.empty())
        {
            return false;
        }

        return writeAll(p_fd, frame);
    }

    template<typename Result, typename Fn>
    void attachProcessSafeContinuation(QFuture<Result> p_future, QProcess& p_process, Fn&& p_fn)
    {
        const QPointer<QProcess> process(&p_process);
        p_future.then(
            [process, fn = std::forward<Fn>(p_fn)](Result p_result) mutable
            {
                if (process.isNull())
                {
                    return;
                }

                QMetaObject::invokeMethod(
                    process.data(),
                    [process, fn = std::move(fn), p_result = std::move(p_result)]() mutable
                    {
                        if (process.isNull())
                        {
                            return;
                        }

                        fn(std::move(p_result));
                    },
                    Qt::QueuedConnection);
            });
    }

    [[nodiscard]] QtMsgType toQtMessageType(const Protocol::LogLevel p_level)
    {
        switch (p_level)
        {
        case Protocol::LOG_LEVEL_DEBUG:
            return QtDebugMsg;
        case Protocol::LOG_LEVEL_WARNING:
            return QtWarningMsg;
        case Protocol::LOG_LEVEL_ERROR:
            return QtCriticalMsg;
        case Protocol::LOG_LEVEL_INFO:
        case Protocol::LOG_LEVEL_UNSPECIFIED:
        default:
            return QtInfoMsg;
        }
    }

    void populateProtobufValue(const QJsonValue& p_jsonValue, google::protobuf::Value* const p_value)
    {
        if (p_value == nullptr)
        {
            return;
        }

        switch (p_jsonValue.type())
        {
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            p_value->set_null_value(google::protobuf::NullValue::NULL_VALUE);
            return;

        case QJsonValue::Bool:
            p_value->set_bool_value(p_jsonValue.toBool());
            return;

        case QJsonValue::Double:
            p_value->set_number_value(p_jsonValue.toDouble());
            return;

        case QJsonValue::String:
            p_value->set_string_value(p_jsonValue.toString().toStdString());
            return;

        case QJsonValue::Array:
        {
            auto* const listValue = p_value->mutable_list_value();
            for (const QJsonValue& entry: p_jsonValue.toArray())
            {
                populateProtobufValue(entry, listValue->add_values());
            }
            return;
        }

        case QJsonValue::Object:
        {
            auto* const structValue = p_value->mutable_struct_value();
            const QJsonObject jsonObject = p_jsonValue.toObject();
            for (auto it = jsonObject.begin(); it != jsonObject.end(); ++it)
            {
                populateProtobufValue(it.value(), &(*structValue->mutable_fields())[it.key().toStdString()]);
            }
            return;
        }
        }
    }

    void populateFieldValues(const std::map<QString, QJsonValue>& p_fieldValues,
                             google::protobuf::Struct* const p_struct)
    {
        if (p_struct == nullptr)
        {
            return;
        }

        for (const auto& [key, value]: p_fieldValues)
        {
            populateProtobufValue(value, &(*p_struct->mutable_fields())[key.toStdString()]);
        }
    }

    [[nodiscard]] qint64 toUnixNanos(const QDateTime& p_timestamp)
    {
        return p_timestamp.isValid() ? p_timestamp.toMSecsSinceEpoch() * 1000000LL : 0LL;
    }

    [[nodiscard]] QDateTime fromUnixNanos(const std::int64_t p_unixNanos)
    {
        return p_unixNanos == 0
                   ? QDateTime()
                   : QDateTime::fromMSecsSinceEpoch(p_unixNanos / 1000000LL, TradingHours::MARKET_TIMEZONE);
    }

    [[nodiscard]] Protocol::OrderStatus toProtocolOrderStatus(const Order::Status p_status)
    {
        switch (p_status)
        {
        case Order::Status::ACK:
            return Protocol::ORDER_STATUS_ACKNOWLEDGED;
        case Order::Status::OPN:
        case Order::Status::DON:
        case Order::Status::FLP:
        case Order::Status::FPR:
        case Order::Status::UCH:
        case Order::Status::RSN:
        case Order::Status::CND:
        case Order::Status::OSO:
        case Order::Status::SUS:
            return Protocol::ORDER_STATUS_OPEN;
        case Order::Status::FLL:
            return Protocol::ORDER_STATUS_FILLED;
        case Order::Status::CAN:
        case Order::Status::EXP:
        case Order::Status::LAT:
        case Order::Status::OUT:
        case Order::Status::UCN:
        case Order::Status::TSC:
        case Order::Status::RJC:
            return Protocol::ORDER_STATUS_CANCELLED;
        case Order::Status::REJ:
        case Order::Status::BRO:
            return Protocol::ORDER_STATUS_REJECTED;
        default:
            return Protocol::ORDER_STATUS_PENDING;
        }
    }

    [[nodiscard]] QString tsClientErrorToString(const TSClient::Error p_error)
    {
        switch (p_error)
        {
        case TSClient::Error::Timeout:
            return "timeout";
        case TSClient::Error::JSONError:
            return "json_error";
        case TSClient::Error::RejectedByValidator:
            return "rejected_by_validator";
        case TSClient::Error::Other:
        default:
            return "other";
        }
    }

    [[nodiscard]] Protocol::OrderConfirmationDecision
    toProtocolManualOrderDecision(const StrategyManualOrderDecision p_decision)
    {
        switch (p_decision)
        {
        case StrategyManualOrderDecision::Accepted:
            return Protocol::ORDER_CONFIRMATION_DECISION_ACCEPTED;
        case StrategyManualOrderDecision::Rejected:
            return Protocol::ORDER_CONFIRMATION_DECISION_REJECTED;
        case StrategyManualOrderDecision::TimedOut:
            return Protocol::ORDER_CONFIRMATION_DECISION_TIMED_OUT;
        case StrategyManualOrderDecision::Cancelled:
            return Protocol::ORDER_CONFIRMATION_DECISION_CANCELLED;
        }

        return Protocol::ORDER_CONFIRMATION_DECISION_UNSPECIFIED;
    }

    [[nodiscard]] std::optional<TradeAction> toTradeAction(const Protocol::OrderSide p_side)
    {
        switch (p_side)
        {
        case Protocol::ORDER_SIDE_BUY:
            return TradeAction::Buy;
        case Protocol::ORDER_SIDE_SELL:
            return TradeAction::Sell;
        case Protocol::ORDER_SIDE_SELL_SHORT:
            return TradeAction::SellShort;
        case Protocol::ORDER_SIDE_BUY_TO_COVER:
            return TradeAction::BuyToCover;
        case Protocol::ORDER_SIDE_UNSPECIFIED:
        default:
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<OrderType::Type> toHostOrderType(const Protocol::OrderType p_type)
    {
        switch (p_type)
        {
        case Protocol::ORDER_TYPE_MARKET:
            return OrderType::Type::Market;
        case Protocol::ORDER_TYPE_LIMIT:
            return OrderType::Type::Limit;
        case Protocol::ORDER_TYPE_STOP_MARKET:
            return OrderType::Type::StopMarket;
        case Protocol::ORDER_TYPE_STOP_LIMIT:
            return OrderType::Type::StopLimit;
        case Protocol::ORDER_TYPE_UNSPECIFIED:
        default:
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<OrderDuration> toHostOrderDuration(const Protocol::OrderDuration p_duration)
    {
        switch (p_duration)
        {
        case Protocol::ORDER_DURATION_DAY:
            return OrderDuration::Day;
        case Protocol::ORDER_DURATION_DAY_PLUS:
            return OrderDuration::DayPlus;
        case Protocol::ORDER_DURATION_GTC:
            return OrderDuration::GTC;
        case Protocol::ORDER_DURATION_GTC_PLUS:
            return OrderDuration::GTCPlus;
        case Protocol::ORDER_DURATION_IOC:
            return OrderDuration::IOC;
        case Protocol::ORDER_DURATION_FOK:
            return OrderDuration::FOK;
        case Protocol::ORDER_DURATION_UNSPECIFIED:
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<StrategyManualOrderExecutionMode>
    toHostManualExecutionMode(const Protocol::OrderConfirmationExecutionMode p_mode)
    {
        switch (p_mode)
        {
        case Protocol::ORDER_CONFIRMATION_EXECUTION_MODE_UNSPECIFIED:
        case Protocol::ORDER_CONFIRMATION_EXECUTION_MODE_FIXED:
            return StrategyManualOrderExecutionMode::Fixed;
        case Protocol::ORDER_CONFIRMATION_EXECUTION_MODE_MARKETABLE_ON_ACCEPT:
            return StrategyManualOrderExecutionMode::MarketableOnAccept;
        default:
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<StrategyBracketSide> toHostBracketSide(const Protocol::BracketSide p_side)
    {
        switch (p_side)
        {
        case Protocol::BRACKET_SIDE_LONG:
            return StrategyBracketSide::Long;
        case Protocol::BRACKET_SIDE_SHORT:
            return StrategyBracketSide::Short;
        case Protocol::BRACKET_SIDE_UNSPECIFIED:
        default:
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<StrategyBracketExecutionPolicy>
    toHostBracketExecutionPolicy(const Protocol::BracketExecutionPolicy p_policy)
    {
        switch (p_policy)
        {
        case Protocol::BRACKET_EXECUTION_POLICY_AUTO:
            return StrategyBracketExecutionPolicy::Auto;
        case Protocol::BRACKET_EXECUTION_POLICY_VIRTUAL_ONLY:
            return StrategyBracketExecutionPolicy::VirtualOnly;
        case Protocol::BRACKET_EXECUTION_POLICY_NATIVE_ONLY:
            return StrategyBracketExecutionPolicy::NativeOnly;
        case Protocol::BRACKET_EXECUTION_POLICY_UNSPECIFIED:
        default:
            return std::nullopt;
        }
    }

    [[nodiscard]] Protocol::OrderSide toProtocolOrderSide(const TradeAction p_action)
    {
        switch (p_action)
        {
        case TradeAction::Buy:
            return Protocol::ORDER_SIDE_BUY;
        case TradeAction::Sell:
            return Protocol::ORDER_SIDE_SELL;
        case TradeAction::SellShort:
            return Protocol::ORDER_SIDE_SELL_SHORT;
        case TradeAction::BuyToCover:
            return Protocol::ORDER_SIDE_BUY_TO_COVER;
        default:
            return Protocol::ORDER_SIDE_UNSPECIFIED;
        }
    }

    [[nodiscard]] Protocol::OrderType toProtocolOrderType(const OrderType::Type p_type)
    {
        switch (p_type)
        {
        case OrderType::Type::Market:
            return Protocol::ORDER_TYPE_MARKET;
        case OrderType::Type::Limit:
            return Protocol::ORDER_TYPE_LIMIT;
        case OrderType::Type::StopMarket:
            return Protocol::ORDER_TYPE_STOP_MARKET;
        case OrderType::Type::StopLimit:
            return Protocol::ORDER_TYPE_STOP_LIMIT;
        default:
            return Protocol::ORDER_TYPE_UNSPECIFIED;
        }
    }

    void populateBarMessage(const QString& p_symbol, const Bar& p_bar, Protocol::Bar* const p_message)
    {
        if (p_message == nullptr)
        {
            return;
        }

        p_message->set_symbol(p_symbol.toStdString());
        p_message->set_open_unix_nanos(toUnixNanos(p_bar.getTimestamp()));
        p_message->set_open(p_bar.getOpen());
        p_message->set_high(p_bar.getHigh());
        p_message->set_low(p_bar.getLow());
        p_message->set_close(p_bar.getClose());
        p_message->set_volume(p_bar.getTotalVolume());
    }

    void populateLevel2Rows(const std::array<Level2Row, 10>& p_levels,
                            google::protobuf::RepeatedPtrField<Protocol::PriceLevel>* const p_out)
    {
        if (p_out == nullptr)
        {
            return;
        }

        for (const Level2Row& row: p_levels)
        {
            auto* const level = p_out->Add();
            level->set_price(row.m_price);
            level->set_size(std::max(row.m_size, 0));
            level->set_order_count(std::max(row.m_orderCount, 0));
        }
    }

    void
    populateLevel2Message(const QString& p_symbol, const Level2& p_level2, Protocol::Level2Snapshot* const p_message)
    {
        if (p_message == nullptr)
        {
            return;
        }

        p_message->set_symbol(p_symbol.toStdString());
        p_message->set_snapshot_unix_nanos(toUnixNanos(p_level2.m_timeStamp));
        populateLevel2Rows(p_level2.m_bids, p_message->mutable_bids());
        populateLevel2Rows(p_level2.m_asks, p_message->mutable_asks());
    }

    void populateClosePositionItemMessage(const ClosePositionItemResult& p_item,
                                          Protocol::ClosePositionItemResult* const p_message)
    {
        if (p_message == nullptr)
        {
            return;
        }

        p_message->set_position_id(p_item.positionId.toStdString());
        p_message->set_account_id(p_item.accountId.toStdString());
        p_message->set_symbol(p_item.symbol.toStdString());
        p_message->set_long_short(p_item.longShort.toStdString());
        p_message->set_quantity(static_cast<std::uint32_t>(std::max(p_item.quantity, 0)));
        p_message->set_side(toProtocolOrderSide(p_item.tradeAction));
        p_message->set_type(toProtocolOrderType(p_item.orderType));
        if (p_item.limitPrice.has_value())
        {
            p_message->set_limit_price(p_item.limitPrice.value());
        }
        p_message->set_submitted(p_item.submitted);
        p_message->set_placement_succeeded(p_item.placementSucceeded);

        for (const QString& orderId: p_item.orderIds)
        {
            p_message->add_order_ids(orderId.toStdString());
        }
        for (const QString& brokerMessage: p_item.brokerMessages)
        {
            p_message->add_broker_messages(brokerMessage.toStdString());
        }
        for (const QString& brokerError: p_item.brokerErrors)
        {
            p_message->add_broker_errors(brokerError.toStdString());
        }

        if (!p_item.failureMessage.isEmpty())
        {
            auto* const error = p_message->mutable_error();
            error->set_code(
                (p_item.failureCode.has_value() ? p_item.failureCode.value() : QString("close_position_failed"))
                    .toStdString());
            error->set_message(p_item.failureMessage.toStdString());
        }
    }

    void populateClosePositionsResponseMessage(const ::ClosePositionsResult& p_result,
                                               Protocol::ClosePositionsResponse* const p_message)
    {
        if (p_message == nullptr)
        {
            return;
        }

        p_message->set_account_id(p_result.accountId.toStdString());
        for (const QString& symbol: p_result.requestedSymbols)
        {
            p_message->add_requested_symbols(symbol.toStdString());
        }
        p_message->set_session(p_result.session.toStdString());
        p_message->set_uses_aggressive_limit_orders(p_result.usesAggressiveLimitOrders);
        p_message->set_forced_day_plus(p_result.forcedDayPlus);
        p_message->set_aggressivity_offset_cents(p_result.aggressivityOffsetCents);
        p_message->set_matched_position_count(static_cast<std::uint32_t>(std::max(p_result.matchedPositionCount, 0)));
        p_message->set_submitted_order_count(static_cast<std::uint32_t>(std::max(p_result.submittedOrderCount, 0)));
        for (const ::ClosePositionItemResult& item: p_result.items)
        {
            populateClosePositionItemMessage(item, p_message->add_items());
        }
    }

    void populateTradeMessage(const QString& p_symbol, const Trade& p_trade, Protocol::Trade* const p_message)
    {
        if (p_message == nullptr)
        {
            return;
        }

        p_message->set_symbol(p_symbol.toStdString());
        p_message->set_trade_unix_nanos(toUnixNanos(p_trade.m_timestamp));
        p_message->set_price(p_trade.m_price);
        p_message->set_size(std::max(p_trade.m_size, 0));
    }

    void populateOrderUpdateMessage(const Order& p_order, Protocol::OrderUpdate* const p_message)
    {
        if (p_message == nullptr)
        {
            return;
        }

        p_message->set_order_id(p_order.getOrderID().toStdString());
        p_message->set_symbol(p_order.getSymbol().toStdString());
        p_message->set_status(toProtocolOrderStatus(p_order.getOrderStatus()));
        p_message->set_quantity(p_order.getQuantity().toUInt());

        if (p_order.getFilledPrice() > 0.0)
        {
            p_message->set_average_fill_price(p_order.getFilledPrice());
        }

        if (const auto rejectReason = p_order.getRejectReason(); rejectReason.has_value())
        {
            auto* const error = p_message->mutable_error();
            error->set_code("order_rejected");
            error->set_message(rejectReason->toStdString());
        }
    }

    void populatePositionUpdateMessage(const Position& p_position, Protocol::PositionUpdate* const p_message)
    {
        if (p_message == nullptr)
        {
            return;
        }

        p_message->set_symbol(p_position.getSymbol().toStdString());
        p_message->set_quantity(p_position.getQuantity().toLongLong());
        p_message->set_average_price(p_position.getAveragePrice().toDouble());
        p_message->set_unrealized_pnl(p_position.getUnrealizedProfitLoss().toDouble());
    }

    void
    populateHistoricalBarsResponseMessage(const QString& p_symbol,
                                          const std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>& p_result,
                                          Protocol::HistoricalBarsResponse* const p_response)
    {
        if (p_response == nullptr)
        {
            return;
        }

        if (!p_result.has_value())
        {
            auto* const error = p_response->mutable_error();
            error->set_code(tsClientErrorToString(p_result.error()).toStdString());
            error->set_message(QString("Historical bar request failed with %1")
                                   .arg(tsClientErrorToString(p_result.error()))
                                   .toStdString());
            return;
        }

        const std::shared_ptr<QVector<Bar>>& bars = p_result.value();
        if (!bars)
        {
            return;
        }

        for (const Bar& bar: *bars)
        {
            populateBarMessage(p_symbol, bar, p_response->add_bars());
        }
    }
} // namespace

std::expected<std::unique_ptr<ProcessStrategyRuntimeBackend>, QString>
ProcessStrategyRuntimeBackend::create(MainAlgo* p_mainAlgo, const QString& p_strategyID, const StrategyConfig& p_config)
{
    const QFileInfo executableInfo(p_config.executablePath);
    if (p_config.executablePath.isEmpty())
    {
        return std::unexpected("Strategy executable path is empty");
    }

    if (!executableInfo.exists())
    {
        return std::unexpected("Strategy executable does not exist: " + p_config.executablePath);
    }

    if (!executableInfo.isExecutable())
    {
        return std::unexpected("Strategy executable is not executable: " + p_config.executablePath);
    }

    auto logger = std::make_unique<StrategyLogger>(p_config.name);
    auto* sdk = new StrategySDK(p_mainAlgo, p_strategyID, p_config, logger.get());

    return std::unique_ptr<ProcessStrategyRuntimeBackend>(
        new ProcessStrategyRuntimeBackend(p_mainAlgo, p_strategyID, p_config, sdk, std::move(logger)));
}

ProcessStrategyRuntimeBackend::ProcessStrategyRuntimeBackend(MainAlgo* p_mainAlgo,
                                                             QString p_strategyID,
                                                             const StrategyConfig& p_config,
                                                             StrategySDK* p_sdk,
                                                             std::unique_ptr<StrategyLogger>&& p_logger)
    : m_mainAlgo(p_mainAlgo)
    , m_strategyID(std::move(p_strategyID))
    , m_config(p_config)
    , m_sdk(p_sdk)
    , m_logger(std::move(p_logger))
{
    m_process.setObjectName(QString("StrategyProcess_%1_%2").arg(m_config.name).arg(m_strategyID.left(8)));
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    setupProcessObservers();
}

ProcessStrategyRuntimeBackend::~ProcessStrategyRuntimeBackend()
{
    destroyRuntime();
}

StrategySDK* ProcessStrategyRuntimeBackend::sdk() const
{
    return m_sdk;
}

StrategyLogger* ProcessStrategyRuntimeBackend::logger() const
{
    return m_logger.get();
}

Qt::HANDLE ProcessStrategyRuntimeBackend::threadHandle() const
{
    if (m_process.processId() <= 0)
    {
        return nullptr;
    }

    return reinterpret_cast<Qt::HANDLE>(static_cast<uintptr_t>(m_process.processId()));
}

void ProcessStrategyRuntimeBackend::setThreadHandle(Qt::HANDLE p_handle)
{
    Q_UNUSED(p_handle);
}

bool ProcessStrategyRuntimeBackend::isThreadRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

QString ProcessStrategyRuntimeBackend::start(const std::function<void()>& p_onStarted)
{
    if (m_process.state() != QProcess::NotRunning)
    {
        return "Strategy process backend is already running";
    }

    resetStartAttemptState();

    QString error = openListeningSocket();
    if (!error.isEmpty())
    {
        return error;
    }

    error = ensureProcessStarted();
    if (!error.isEmpty())
    {
        resetStartAttemptState();
        return error;
    }

    error = acceptClientConnection();
    if (!error.isEmpty())
    {
        markShutdownRequested();
        m_process.kill();
        m_process.waitForFinished(kTerminateTimeoutMs);
        resetStartAttemptState();
        return error;
    }

    error = completeHandshake();
    if (!error.isEmpty())
    {
        markShutdownRequested();
        const QString shutdownError = sendStopLikeCommand(true, "Handshake failed");
        if (!shutdownError.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send handshake-failure shutdown command:" << shutdownError;
        }
        m_process.kill();
        m_process.waitForFinished(kTerminateTimeoutMs);
        resetStartAttemptState();
        return error;
    }

    error = waitForStartReady();
    if (!error.isEmpty())
    {
        markShutdownRequested();
        const QString shutdownError = sendStopLikeCommand(true, "Strategy startup did not reach ready state");
        if (!shutdownError.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send startup-failure shutdown command:" << shutdownError;
        }
        m_process.kill();
        m_process.waitForFinished(kTerminateTimeoutMs);
        resetStartAttemptState();
        return error;
    }

    if (p_onStarted)
    {
        p_onStarted();
    }

    qInfo(StrategyManagerLog) << "Started external strategy process:" << m_config.name << "ID:" << m_strategyID
                              << "PID:" << m_process.processId();
    return "";
}

QString ProcessStrategyRuntimeBackend::pause(const QString& p_reason)
{
    if (m_process.state() == QProcess::NotRunning)
    {
        return "Strategy process backend is not running";
    }

    return sendPauseCommand(p_reason);
}

QString ProcessStrategyRuntimeBackend::resume()
{
    if (m_process.state() == QProcess::NotRunning)
    {
        return "Strategy process backend is not running";
    }

    return sendResumeCommand();
}

QString ProcessStrategyRuntimeBackend::updateConfig(const StrategyConfig& p_config)
{
    if (m_process.state() != QProcess::NotRunning)
    {
        return QStringLiteral("Cannot update strategy config while strategy process is running");
    }

    m_config = p_config;
    if (m_sdk != nullptr)
    {
        m_sdk->setConfig(p_config);
    }

    return QString();
}

void ProcessStrategyRuntimeBackend::trackMonitoredSymbol(const QString& p_symbol)
{
    Q_UNUSED(p_symbol);
}

void ProcessStrategyRuntimeBackend::publishBar(const QString& p_symbol, const Bar& p_bar)
{
    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    populateBarMessage(p_symbol, p_bar, envelope.mutable_bar());
    [[maybe_unused]] const bool sent = sendHostEnvelope(envelope, QString("send bar update for %1").arg(p_symbol));
}

void ProcessStrategyRuntimeBackend::publishLevel2(const QString& p_symbol, const Level2& p_level2)
{
    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    populateLevel2Message(p_symbol, p_level2, envelope.mutable_level2_snapshot());
    [[maybe_unused]] const bool sent = sendHostEnvelope(envelope, QString("send level2 update for %1").arg(p_symbol));
}

void ProcessStrategyRuntimeBackend::publishTrade(const QString& p_symbol, const Trade& p_trade)
{
    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    populateTradeMessage(p_symbol, p_trade, envelope.mutable_trade());
    [[maybe_unused]] const bool sent = sendHostEnvelope(envelope, QString("send trade update for %1").arg(p_symbol));
}

void ProcessStrategyRuntimeBackend::publishSubscriptionError(const MarketDataSubscriptionError& p_error)
{
    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    auto* error = envelope.mutable_error();
    error->set_code(p_error.terminal ? "market_data_subscription_rejected" : "market_data_subscription_retrying");
    error->set_message(QString("%1 %2 subscription failed (%3): %4")
                           .arg(p_error.symbol, p_error.feed, p_error.reason, p_error.message)
                           .toStdString());
    auto* subscription = error->mutable_subscription_error();
    subscription->set_symbol(p_error.symbol.toStdString());
    subscription->set_feed(p_error.feed.toStdString());
    subscription->set_reason(p_error.reason.toStdString());
    subscription->set_terminal(p_error.terminal);
    subscription->set_retry_delay_ms(p_error.retryDelayMs);
    ASSUME_DIFF(m_logger.get(), nullptr);
    m_logger->log(QtWarningMsg, QString::fromStdString(error->message()));
    [[maybe_unused]] const bool sent = sendHostEnvelope(envelope, "send subscription error");
}

void ProcessStrategyRuntimeBackend::publishOrder(const Order& p_order)
{
    if (m_sdk != nullptr)
    {
        m_sdk->updateOrder(p_order);
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    populateOrderUpdateMessage(p_order, envelope.mutable_order_update());
    [[maybe_unused]] const bool sent =
        sendHostEnvelope(envelope, QString("send order update %1").arg(p_order.getOrderID()));
}

void ProcessStrategyRuntimeBackend::publishClaimedSymbols(const QStringList& p_claimedSymbols)
{
    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    auto* const update = envelope.mutable_claimed_symbols_update();
    for (const QString& symbol: p_claimedSymbols)
    {
        update->add_active_symbols(symbol.toStdString());
    }
    [[maybe_unused]] const bool sent = sendHostEnvelope(envelope, "send claimed symbols update");
}

void ProcessStrategyRuntimeBackend::publishManualOrderDecision(const QString& p_requestID,
                                                               const QString& p_symbol,
                                                               const StrategyManualOrderDecision p_decision,
                                                               const QString& p_reason)
{
    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    auto* const update = envelope.mutable_order_confirmation_update();
    update->set_request_id(p_requestID.toStdString());
    update->set_symbol(p_symbol.toStdString());
    update->set_decision(toProtocolManualOrderDecision(p_decision));
    if (!p_reason.isEmpty())
    {
        update->set_reason(p_reason.toStdString());
    }
    [[maybe_unused]] const bool sent =
        sendHostEnvelope(envelope, QString("send manual order decision %1").arg(p_requestID));
}

void ProcessStrategyRuntimeBackend::publishPosition(const Position& p_position)
{
    if (m_sdk != nullptr)
    {
        m_sdk->updatePosition(p_position);
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    populatePositionUpdateMessage(p_position, envelope.mutable_position_update());
    [[maybe_unused]] const bool sent =
        sendHostEnvelope(envelope, QString("send position update for %1").arg(p_position.getSymbol()));
}

void ProcessStrategyRuntimeBackend::publishBalance(const double p_balance)
{
    if (m_sdk != nullptr)
    {
        m_sdk->updateBalance(p_balance);
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    envelope.mutable_balance_update()->set_cash(p_balance);
    [[maybe_unused]] const bool sent = sendHostEnvelope(envelope, "send balance update");
}

void ProcessStrategyRuntimeBackend::invokeOnStop()
{
    if (m_process.state() == QProcess::NotRunning)
    {
        return;
    }

    markShutdownRequested();
    const QString error = sendStopLikeCommand(false, "Strategy unload requested");
    if (!error.isEmpty())
    {
        qWarning(StrategyManagerLog) << "Failed to send stop command to strategy process:" << m_strategyID << error;
    }
}

void ProcessStrategyRuntimeBackend::shutdownExecutionThread()
{
    if (m_process.state() == QProcess::NotRunning)
    {
        return;
    }

    if (!m_shutdownRequested)
    {
        markShutdownRequested();
        const QString error = sendStopLikeCommand(true, "Strategy shutdown requested");
        if (!error.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send shutdown command to strategy process:" << m_strategyID
                                         << error;
        }
    }

    if (waitForProcessExitWithEvents(kShutdownTimeoutMs))
    {
        return;
    }

    qWarning(StrategyManagerLog) << "Strategy process did not exit within timeout, terminating:" << m_strategyID;
    m_process.terminate();
    if (!waitForProcessExitWithEvents(kTerminateTimeoutMs))
    {
        qWarning(StrategyManagerLog) << "Strategy process ignored terminate, killing:" << m_strategyID;
        m_process.kill();
        (void)waitForProcessExitWithEvents(kTerminateTimeoutMs);
    }
}

void ProcessStrategyRuntimeBackend::destroyRuntime()
{
    if (m_runtimeDestroying || m_runtimeDestroyed)
    {
        return;
    }
    m_runtimeDestroying = true;

    if (m_process.state() != QProcess::NotRunning && !m_shutdownRequested)
    {
        markShutdownRequested();
        const QString error = sendStopLikeCommand(true, "Strategy runtime backend destroyed");
        if (!error.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send final shutdown command to strategy process:" << m_strategyID
                                         << error;
        }
    }

    shutdownExecutionThread();
    cleanupSocketResources();

    delete m_sdk;
    m_sdk = nullptr;
    m_runtimeDestroyed = true;
    m_runtimeDestroying = false;
}

void ProcessStrategyRuntimeBackend::setupProcessObservers()
{
    QObject::connect(&m_process,
                     &QProcess::readyReadStandardError,
                     &m_process,
                     [this]() { captureProcessStream(QProcess::StandardError); });

    QObject::connect(&m_process,
                     &QProcess::errorOccurred,
                     &m_process,
                     [this](const QProcess::ProcessError p_error)
                     {
                         if (m_shutdownRequested || m_runtimeDestroyed || p_error == QProcess::UnknownError)
                         {
                             return;
                         }

                         reportFailure("Strategy process error: " + m_process.errorString());
                     });

    QObject::connect(&m_process,
                     qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     &m_process,
                     [this](const int p_exitCode, const QProcess::ExitStatus p_exitStatus)
                     {
                         captureProcessStream(QProcess::StandardError);
                         // A child can report its failure immediately before exiting.
                         drainInboundSocket();
                         cleanupSocketResources();

                         if (m_shutdownRequested || m_runtimeDestroyed)
                         {
                             if (p_exitStatus == QProcess::CrashExit || p_exitCode != 0)
                             {
                                 qWarning(StrategyManagerLog)
                                     << "Strategy process exited during shutdown with status:" << m_strategyID
                                     << "code:" << p_exitCode << "exitStatus:" << p_exitStatus;
                             }
                             else
                             {
                                 qInfo(StrategyManagerLog)
                                     << "Strategy process exited cleanly:" << m_strategyID << "code:" << p_exitCode;
                             }
                             return;
                         }

                         const QString errorMessage =
                             p_exitStatus == QProcess::CrashExit
                                 ? QString("Strategy process crashed with exit code %1").arg(p_exitCode)
                                 : QString("Strategy process exited unexpectedly with exit code %1").arg(p_exitCode);
                         reportFailure(errorMessage);
                     });
}

void ProcessStrategyRuntimeBackend::captureProcessStream(const QProcess::ProcessChannel p_channel)
{
    const QByteArray output =
        p_channel == QProcess::StandardOutput ? m_process.readAllStandardOutput() : m_process.readAllStandardError();
    if (output.isEmpty() || m_logger == nullptr)
    {
        return;
    }

    const QtMsgType level = p_channel == QProcess::StandardError ? QtWarningMsg : QtInfoMsg;
    const QString prefix = p_channel == QProcess::StandardError ? "[stderr] " : "[stdout] ";
    const QString text = QString::fromUtf8(output);
    for (const QString& line: text.split('\n', Qt::SkipEmptyParts))
    {
        m_logger->log(level, prefix + line.trimmed());
    }
}

void ProcessStrategyRuntimeBackend::drainInboundSocket()
{
    if (m_clientFd < 0)
    {
        return;
    }

    std::array<char, kSocketReadChunkSize> buffer{};
    QString readError;
    while (true)
    {
        const ssize_t result = ::read(m_clientFd, buffer.data(), buffer.size());
        if (result > 0)
        {
            m_receiveBuffer.append(buffer.data(), static_cast<qsizetype>(result));
            while (true)
            {
                auto frame = StrategyFrameReader::takeFrame(m_receiveBuffer);
                if (!frame.has_value())
                {
                    readError = frame.error();
                    break;
                }
                if (!frame->has_value())
                    break;
                handleInboundPayload(frame->value());
                if (m_failureReported)
                {
                    readError = m_lastFailureMessage;
                    break;
                }
            }
            if (!readError.isEmpty())
                break;
            continue;
        }

        if (result == 0)
        {
            readError = "Strategy process socket disconnected unexpectedly";
            break;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            break;
        }

        readError = QString("Failed to read strategy socket: %1").arg(QString::fromUtf8(std::strerror(errno)));
        break;
    }

    if (!readError.isEmpty())
    {
        scheduleSocketCleanup(readError);
    }
}

void ProcessStrategyRuntimeBackend::handleInboundPayload(const std::span<const std::uint8_t> p_payload)
{
    Protocol::StrategyToHostEnvelope envelope;
    if (!OpenTraderPlatform::StrategySDK::parseMessagePayload(p_payload, &envelope))
    {
        reportFailure("Failed to parse inbound strategy payload");
        return;
    }

    switch (envelope.payload_case())
    {
    case Protocol::StrategyToHostEnvelope::kStrategyLog:
        if (m_logger)
        {
            m_logger->log(toQtMessageType(envelope.strategy_log().level()),
                          QString::fromStdString(envelope.strategy_log().message()));
        }
        break;

    case Protocol::StrategyToHostEnvelope::kHeartbeat:
        qDebug(StrategyManagerLog) << "Received heartbeat reply from strategy:" << m_strategyID
                                   << "sequence:" << envelope.heartbeat().monotonic_sequence();
        break;

    case Protocol::StrategyToHostEnvelope::kError:
        if (m_logger)
        {
            m_logger->log(QtCriticalMsg,
                          QString("[protocol-error] %1: %2")
                              .arg(QString::fromStdString(envelope.error().code()),
                                   QString::fromStdString(envelope.error().message())));
        }
        if (envelope.error().strategy_failure())
        {
            reportFailure(QString::fromStdString(envelope.error().message()));
        }
        break;

    case Protocol::StrategyToHostEnvelope::kStrategyReady:
        if (m_waitingForStartReady)
        {
            m_startReadyReceived = true;
            qInfo(StrategyManagerLog) << "Strategy reported ready:" << m_strategyID;
        }
        else
        {
            qWarning(StrategyManagerLog) << "Received unexpected strategy_ready after startup:" << m_strategyID;
        }
        break;

    case Protocol::StrategyToHostEnvelope::kClaimSymbolsRequest:
        if (m_mainAlgo == nullptr || m_mainAlgo->getStrategyManager() == nullptr)
        {
            sendErrorMessage("claim_symbols_unavailable",
                             "Strategy manager unavailable for claim_symbols_request",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            QStringList requestedSymbols;
            requestedSymbols.reserve(envelope.claim_symbols_request().symbols_size());
            for (const std::string& symbol: envelope.claim_symbols_request().symbols())
            {
                requestedSymbols.append(QString::fromStdString(symbol));
            }

            auto promise = std::make_shared<QPromise<QStringList>>();
            promise->start();
            auto future = promise->future();
            m_mainAlgo->getStrategyManager()->processClaimSymbols(m_strategyID, requestedSymbols, promise);
            future.waitForFinished();

            const QStringList approved = future.result();
            Protocol::HostToStrategyEnvelope response;
            response.set_sequence(m_outboundSequence++);
            response.set_correlation_id(envelope.correlation_id());
            auto* const claimResponse = response.mutable_claim_symbols_response();
            for (const QString& approvedSymbol: approved)
            {
                claimResponse->add_granted_symbols(approvedSymbol.toStdString());
            }
            for (const QString& requestedSymbol: requestedSymbols)
            {
                if (!approved.contains(requestedSymbol))
                {
                    claimResponse->add_rejected_symbols(requestedSymbol.toStdString());
                }
            }
            [[maybe_unused]] const bool sent = sendHostEnvelope(response, "send claim symbols response");
        }
        break;

    case Protocol::StrategyToHostEnvelope::kHistoricalBarsRequest:
        if (m_mainAlgo == nullptr)
        {
            Protocol::HostToStrategyEnvelope response;
            response.set_sequence(m_outboundSequence++);
            response.set_correlation_id(envelope.correlation_id());
            auto* const historicalResponse = response.mutable_historical_bars_response();
            historicalResponse->mutable_error()->set_code("historical_bars_unavailable");
            historicalResponse->mutable_error()->set_message("MainAlgo unavailable for historical bars request");
            [[maybe_unused]] const bool sent = sendHostEnvelope(response, "send historical bars failure");
            break;
        }
        else
        {
            const auto& request = envelope.historical_bars_request();
            const QString symbol = QString::fromStdString(request.symbol());
            const QDateTime sessionDay = fromUnixNanos(request.session_day_unix_nanos());
            const QDateTime firstBar = fromUnixNanos(request.first_bar_unix_nanos());
            const QDateTime lastBar = fromUnixNanos(request.last_bar_unix_nanos());
            const QString correlationID = QString::fromStdString(envelope.correlation_id());

            auto sendInvalidRequest = [this, &symbol, &correlationID](const QString& p_message)
            {
                Protocol::HostToStrategyEnvelope response;
                response.set_sequence(m_outboundSequence++);
                response.set_correlation_id(correlationID.toStdString());
                auto* const historicalResponse = response.mutable_historical_bars_response();
                historicalResponse->mutable_error()->set_code("invalid_historical_bars_request");
                historicalResponse->mutable_error()->set_message(p_message.toStdString());
                [[maybe_unused]] const bool sent =
                    sendHostEnvelope(response,
                                     QString("send invalid historical bars request response for %1").arg(symbol));
            };

            if (symbol.isEmpty())
            {
                sendInvalidRequest("Historical bars request symbol is empty");
                break;
            }

            if (!firstBar.isValid() || !lastBar.isValid())
            {
                sendInvalidRequest("Historical bars request timestamps are invalid");
                break;
            }

            if (firstBar > lastBar)
            {
                sendInvalidRequest("Historical bars request start is after end");
                break;
            }

            const QDate day = sessionDay.isValid() ? sessionDay.date() : firstBar.date();
            const TimeFrame timeFrame = stringToTimeFrame(QString::fromStdString(request.timeframe()));

            auto sendHistoricalResponse =
                [this, correlationID, symbol](
                    const std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>& p_result)
            {
                Protocol::HostToStrategyEnvelope response;
                response.set_sequence(m_outboundSequence++);
                response.set_correlation_id(correlationID.toStdString());
                populateHistoricalBarsResponseMessage(symbol, p_result, response.mutable_historical_bars_response());
                [[maybe_unused]] const bool sent =
                    sendHostEnvelope(response, QString("send historical bars response for %1").arg(symbol));
            };

            auto result =
                m_mainAlgo->requestHistoricalBarsForSymbol(symbol, day, firstBar.time(), lastBar.time(), timeFrame);
            if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(result))
            {
                sendHistoricalResponse(std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>(
                    std::get<std::shared_ptr<QVector<Bar>>>(result)));
            }
            else
            {
                attachProcessSafeContinuation(
                    std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result),
                    m_process,
                    [sendHistoricalResponse = std::move(sendHistoricalResponse)](
                        std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error> p_result)
                    { sendHistoricalResponse(p_result); });
            }
        }
        break;

    case Protocol::StrategyToHostEnvelope::kCurrentTimeRequest:
    {
        Protocol::HostToStrategyEnvelope response;
        response.set_sequence(m_outboundSequence++);
        response.set_correlation_id(envelope.correlation_id());
        response.mutable_current_time_response()->set_current_unix_nanos(toUnixNanos(MainApp::getCurrentAppTime()));
        [[maybe_unused]] const bool sent = sendHostEnvelope(response, "send current time response");
        break;
    }

    case Protocol::StrategyToHostEnvelope::kChartLogIntent:
        if (m_mainAlgo == nullptr)
        {
            sendErrorMessage("chart_log_unavailable",
                             "MainAlgo unavailable for chart_log_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.chart_log_intent();
            const QString symbol = QString::fromStdString(intent.symbol());
            const QString message = QString::fromStdString(intent.message());
            if (symbol.isEmpty() || message.isEmpty())
            {
                sendErrorMessage("invalid_chart_log_intent",
                                 "chart_log_intent requires non-empty symbol and message",
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            const StrategyLogEntry entry{
                .strategyID = m_strategyID,
                .symbol = symbol,
                .timestamp = MainApp::getCurrentAppTime(),
                .message = message,
            };
            QMetaObject::invokeMethod(
                m_mainAlgo,
                [mainAlgo = m_mainAlgo, entry]()
                {
                    ASSUME_DIFF(mainAlgo, nullptr);
                    mainAlgo->processStrategyLog(entry);
                },
                Qt::QueuedConnection);
        }
        break;

    case Protocol::StrategyToHostEnvelope::kChartStatusIntent:
        if (m_mainAlgo == nullptr)
        {
            sendErrorMessage("chart_status_unavailable",
                             "MainAlgo unavailable for chart_status_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.chart_status_intent();
            const QString symbol = QString::fromStdString(intent.symbol()).trimmed().toUpper();
            const QString message = QString::fromStdString(intent.message());
            if (symbol.isEmpty() || message.isEmpty())
            {
                sendErrorMessage("invalid_chart_status_intent",
                                 "chart_status_intent requires non-empty symbol and message",
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            const StrategyStatusEntry entry{
                .strategyID = m_strategyID,
                .symbol = symbol,
                .timestamp = MainApp::getCurrentAppTime(),
                .message = message,
                .action = StrategyStatusEntry::Action::Upsert,
            };
            QMetaObject::invokeMethod(
                m_mainAlgo,
                [mainAlgo = m_mainAlgo, entry]()
                {
                    ASSUME_DIFF(mainAlgo, nullptr);
                    mainAlgo->processStrategyStatus(entry);
                },
                Qt::QueuedConnection);
        }
        break;

    case Protocol::StrategyToHostEnvelope::kChartStatusClearIntent:
        if (m_mainAlgo == nullptr)
        {
            sendErrorMessage("clear_chart_status_unavailable",
                             "MainAlgo unavailable for clear_chart_status_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.chart_status_clear_intent();
            const QString symbol = QString::fromStdString(intent.symbol()).trimmed().toUpper();
            if (symbol.isEmpty())
            {
                sendErrorMessage("invalid_clear_chart_status_intent",
                                 "clear_chart_status_intent requires non-empty symbol",
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            QMetaObject::invokeMethod(
                m_mainAlgo,
                [mainAlgo = m_mainAlgo, strategyID = m_strategyID, symbol]()
                {
                    ASSUME_DIFF(mainAlgo, nullptr);
                    mainAlgo->processStrategyStatusClear(strategyID, symbol);
                },
                Qt::QueuedConnection);
        }
        break;

    case Protocol::StrategyToHostEnvelope::kChartDisplaySwitchIntent:
        if (m_sdk == nullptr)
        {
            sendErrorMessage("chart_display_switch_unavailable",
                             "Strategy SDK unavailable for chart_display_switch_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.chart_display_switch_intent();
            const QString symbol = QString::fromStdString(intent.symbol()).trimmed().toUpper();
            const QString reason = QString::fromStdString(intent.reason()).trimmed();
            if (symbol.isEmpty())
            {
                sendErrorMessage("invalid_chart_display_switch_intent",
                                 "chart_display_switch_intent requires non-empty symbol",
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            const bool accepted = m_sdk->requestChartDisplaySwitch(symbol, reason);
            if (!accepted)
            {
                sendErrorMessage(
                    "chart_display_switch_rejected",
                    QString("chart_display_switch_intent for %1 was rejected by host-side validation").arg(symbol),
                    QString::fromStdString(envelope.correlation_id()));
                qWarning(StrategyManagerLog)
                    << "Rejected chart-display switch intent from strategy" << m_strategyID << "symbol=" << symbol
                    << "reason=" << (reason.isEmpty() ? QStringLiteral("<none>") : reason);
            }
            else
            {
                qInfo(StrategyManagerLog)
                    << "Accepted chart-display switch intent from strategy" << m_strategyID << "symbol=" << symbol
                    << "reason=" << (reason.isEmpty() ? QStringLiteral("<none>") : reason);
            }
        }
        break;

    case Protocol::StrategyToHostEnvelope::kUpsertBracketIntent:
        if (m_sdk == nullptr)
        {
            sendErrorMessage("upsert_bracket_unavailable",
                             "Strategy SDK unavailable for upsert_bracket_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.upsert_bracket_intent();
            const QString symbol = QString::fromStdString(intent.symbol());
            const QString accountID = QString::fromStdString(intent.account_id());
            const auto side = toHostBracketSide(intent.side());
            const auto executionPolicy = toHostBracketExecutionPolicy(intent.execution_policy());
            std::optional<double> referenceEntryPrice = std::nullopt;
            if (intent.has_reference_entry_price())
            {
                if (intent.reference_entry_price() <= 0.0)
                {
                    sendErrorMessage("invalid_upsert_bracket_intent",
                                     "upsert_bracket_intent reference_entry_price must be > 0 when provided",
                                     QString::fromStdString(envelope.correlation_id()));
                    break;
                }
                referenceEntryPrice = intent.reference_entry_price();
            }
            if (symbol.isEmpty() || accountID.isEmpty() || !side.has_value() || intent.stop_price() <= 0.0 ||
                intent.take_price() <= 0.0 || !executionPolicy.has_value())
            {
                sendErrorMessage(
                    "invalid_upsert_bracket_intent",
                    "upsert_bracket_intent requires symbol/account_id/side/stop_price/take_price/execution_policy",
                    QString::fromStdString(envelope.correlation_id()));
                break;
            }

            const bool accepted = m_sdk->upsertManagedBracket(symbol,
                                                              accountID,
                                                              side.value(),
                                                              intent.stop_price(),
                                                              intent.take_price(),
                                                              executionPolicy.value(),
                                                              referenceEntryPrice);
            if (!accepted)
            {
                sendErrorMessage("upsert_bracket_rejected",
                                 QString("upsert_bracket_intent %1 was rejected by host-side validation")
                                     .arg(QString::fromStdString(intent.request_id())),
                                 QString::fromStdString(envelope.correlation_id()));
            }
        }
        break;

    case Protocol::StrategyToHostEnvelope::kCancelBracketIntent:
        if (m_sdk == nullptr)
        {
            sendErrorMessage("cancel_bracket_unavailable",
                             "Strategy SDK unavailable for cancel_bracket_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.cancel_bracket_intent();
            const QString symbol = QString::fromStdString(intent.symbol());
            const QString accountID = QString::fromStdString(intent.account_id());
            if (symbol.isEmpty() || accountID.isEmpty())
            {
                sendErrorMessage("invalid_cancel_bracket_intent",
                                 "cancel_bracket_intent requires non-empty symbol/account_id",
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            const bool accepted = m_sdk->cancelManagedBracket(symbol, accountID);
            if (!accepted)
            {
                sendErrorMessage("cancel_bracket_rejected",
                                 QString("cancel_bracket_intent %1 was rejected by host-side validation")
                                     .arg(QString::fromStdString(intent.request_id())),
                                 QString::fromStdString(envelope.correlation_id()));
            }
        }
        break;

    case Protocol::StrategyToHostEnvelope::kPlaceOrderIntent:
        if (m_sdk == nullptr)
        {
            sendErrorMessage("place_order_unavailable",
                             "Strategy SDK unavailable for place_order_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.place_order_intent();
            const auto hostSide = toTradeAction(intent.side());
            const auto hostType = toHostOrderType(intent.type());
            const auto hostDuration = toHostOrderDuration(intent.duration());
            if (!hostSide.has_value() || !hostType.has_value() ||
                (intent.duration() != Protocol::ORDER_DURATION_UNSPECIFIED && !hostDuration.has_value()))
            {
                sendErrorMessage("invalid_order_intent",
                                 QString("Unsupported order side/type/duration in place_order_intent request_id=%1")
                                     .arg(QString::fromStdString(intent.request_id())),
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            if (intent.account_id().empty())
            {
                sendErrorMessage("invalid_order_intent",
                                 QString("place_order_intent %1 missing account_id")
                                     .arg(QString::fromStdString(intent.request_id())),
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            PlaceOrderRequest request;
            request.setAccountID(QString::fromStdString(intent.account_id()));
            request.setSymbol(QString::fromStdString(intent.symbol()));
            request.setQuantity(static_cast<int>(intent.quantity()));
            request.setTradeAction(*hostSide);
            request.setOrderType(*hostType);
            request.setStrategyLog(QString::fromStdString(intent.request_id()));
            if (hostDuration.has_value())
            {
                request.setTimeInForce(TimeInForce(*hostDuration));
            }
            if (intent.has_limit_price())
            {
                request.setLimitPrice(intent.limit_price());
            }
            if (intent.has_stop_price())
            {
                request.setStopPrice(intent.stop_price());
            }

            attachProcessSafeContinuation(m_sdk->placeOrder(request),
                                          m_process,
                                          [this,
                                           correlationID = QString::fromStdString(envelope.correlation_id()),
                                           requestID = QString::fromStdString(intent.request_id())](
                                              std::expected<PlaceOrderResult, TSClient::Error> p_result)
                                          {
                                              if (!p_result.has_value())
                                              {
                                                  sendErrorMessage(
                                                      "place_order_failed",
                                                      QString("place_order_intent %1 failed: %2")
                                                          .arg(requestID, tsClientErrorToString(p_result.error())),
                                                      correlationID);
                                              }
                                          });
        }
        break;

    case Protocol::StrategyToHostEnvelope::kPlaceOrderWithConfirmationIntent:
        if (m_sdk == nullptr)
        {
            sendErrorMessage("place_order_confirmation_unavailable",
                             "Strategy SDK unavailable for place_order_with_confirmation_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.place_order_with_confirmation_intent();
            const auto hostSide = toTradeAction(intent.side());
            const auto hostType = toHostOrderType(intent.type());
            const auto hostDuration = toHostOrderDuration(intent.duration());
            const auto hostExecutionMode = toHostManualExecutionMode(intent.execution_mode());
            if (!hostSide.has_value() || !hostType.has_value() ||
                (intent.duration() != Protocol::ORDER_DURATION_UNSPECIFIED && !hostDuration.has_value()) ||
                !hostExecutionMode.has_value())
            {
                sendErrorMessage("invalid_order_confirmation_intent",
                                 QString("Unsupported order side/type/duration/execution_mode in "
                                         "place_order_with_confirmation_intent request_id=%1")
                                     .arg(QString::fromStdString(intent.request_id())),
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            if (intent.account_id().empty())
            {
                sendErrorMessage("invalid_order_confirmation_intent",
                                 QString("place_order_with_confirmation_intent %1 missing account_id")
                                     .arg(QString::fromStdString(intent.request_id())),
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            if (intent.request_id().empty())
            {
                sendErrorMessage("invalid_order_confirmation_intent",
                                 "place_order_with_confirmation_intent requires a non-empty request_id",
                                 QString::fromStdString(envelope.correlation_id()));
                break;
            }

            PlaceOrderRequest request;
            request.setAccountID(QString::fromStdString(intent.account_id()));
            request.setSymbol(QString::fromStdString(intent.symbol()));
            request.setQuantity(static_cast<int>(intent.quantity()));
            request.setTradeAction(*hostSide);
            request.setOrderType(*hostType);
            request.setStrategyLog(QString::fromStdString(intent.request_id()));
            if (hostDuration.has_value())
            {
                request.setTimeInForce(TimeInForce(*hostDuration));
            }
            if (intent.has_limit_price())
            {
                request.setLimitPrice(intent.limit_price());
            }
            if (intent.has_stop_price())
            {
                request.setStopPrice(intent.stop_price());
            }

            // Manual confirmations report outcomes through order_confirmation_update messages.
            // Keep this continuation only to ensure process-safe future consumption.
            attachProcessSafeContinuation(
                m_sdk->placeOrderWithUserConfirmation(QString::fromStdString(intent.request_id()),
                                                      request,
                                                      QString::fromStdString(intent.prompt_text()),
                                                      hostExecutionMode.value()),
                m_process,
                [](std::expected<PlaceOrderResult, TSClient::Error>) {});
        }
        break;

    case Protocol::StrategyToHostEnvelope::kCancelOrderIntent:
        if (m_sdk == nullptr)
        {
            sendErrorMessage("cancel_order_unavailable",
                             "Strategy SDK unavailable for cancel_order_intent",
                             QString::fromStdString(envelope.correlation_id()));
            break;
        }
        else
        {
            const auto& intent = envelope.cancel_order_intent();
            attachProcessSafeContinuation(m_sdk->cancelOrder(QString::fromStdString(intent.order_id())),
                                          m_process,
                                          [this,
                                           correlationID = QString::fromStdString(envelope.correlation_id()),
                                           requestID = QString::fromStdString(intent.request_id())](
                                              std::expected<CancelOrderResult, TSClient::Error> p_result)
                                          {
                                              if (!p_result.has_value())
                                              {
                                                  sendErrorMessage(
                                                      "cancel_order_failed",
                                                      QString("cancel_order_intent %1 failed: %2")
                                                          .arg(requestID, tsClientErrorToString(p_result.error())),
                                                      correlationID);
                                              }
                                          });
        }
        break;

    case Protocol::StrategyToHostEnvelope::kClosePositionsRequest:
        if (m_sdk == nullptr)
        {
            Protocol::HostToStrategyEnvelope response;
            response.set_sequence(m_outboundSequence++);
            response.set_correlation_id(envelope.correlation_id());
            auto* const closeResponse = response.mutable_close_positions_response();
            closeResponse->set_request_id(envelope.close_positions_request().request_id());
            closeResponse->mutable_error()->set_code("close_positions_unavailable");
            closeResponse->mutable_error()->set_message("Strategy SDK unavailable for close_positions_request");
            [[maybe_unused]] const bool sent = sendHostEnvelope(response, "send close positions failure");
            break;
        }
        else
        {
            const auto& request = envelope.close_positions_request();
            const QString requestID = QString::fromStdString(request.request_id());
            const QString correlationID = QString::fromStdString(envelope.correlation_id());
            const QString accountID = QString::fromStdString(request.account_id()).trimmed();

            auto sendClosePositionsError =
                [this, correlationID, requestID](const QString& p_code, const QString& p_message)
            {
                Protocol::HostToStrategyEnvelope response;
                response.set_sequence(m_outboundSequence++);
                response.set_correlation_id(correlationID.toStdString());
                auto* const closeResponse = response.mutable_close_positions_response();
                closeResponse->set_request_id(requestID.toStdString());
                closeResponse->mutable_error()->set_code(p_code.toStdString());
                closeResponse->mutable_error()->set_message(p_message.toStdString());
                [[maybe_unused]] const bool sent = sendHostEnvelope(response, "send close positions error response");
            };

            if (requestID.isEmpty())
            {
                sendClosePositionsError("invalid_close_positions_request",
                                        "close_positions_request requires a non-empty request_id");
                break;
            }

            if (accountID.isEmpty())
            {
                sendClosePositionsError("invalid_close_positions_request",
                                        "close_positions_request requires a non-empty account_id");
                break;
            }

            QStringList symbols;
            symbols.reserve(request.symbols_size());
            for (const std::string& symbol: request.symbols())
            {
                symbols.append(QString::fromStdString(symbol));
            }

            attachProcessSafeContinuation(
                m_sdk->closePositions(accountID, symbols),
                m_process,
                [this, correlationID, requestID](std::expected<::ClosePositionsResult, QString> p_result)
                {
                    Protocol::HostToStrategyEnvelope response;
                    response.set_sequence(m_outboundSequence++);
                    response.set_correlation_id(correlationID.toStdString());
                    auto* const closeResponse = response.mutable_close_positions_response();
                    closeResponse->set_request_id(requestID.toStdString());

                    if (!p_result.has_value())
                    {
                        closeResponse->mutable_error()->set_code("close_positions_failed");
                        closeResponse->mutable_error()->set_message(p_result.error().toStdString());
                    }
                    else
                    {
                        populateClosePositionsResponseMessage(p_result.value(), closeResponse);
                        closeResponse->set_request_id(requestID.toStdString());
                    }

                    [[maybe_unused]] const bool sent =
                        sendHostEnvelope(response, QString("send close positions response for %1").arg(requestID));
                });
        }
        break;

    default:
        qDebug(StrategyManagerLog) << "Ignoring unsupported strategy-to-host envelope payload:"
                                   << envelope.payload_case() << "for strategy" << m_strategyID;
        break;
    }
}

void ProcessStrategyRuntimeBackend::reportFailure(const QString& p_errorMessage)
{
    if (m_shutdownRequested || m_failureReported)
    {
        return;
    }
    m_failureReported = true;
    m_lastFailureMessage = p_errorMessage;

    qCritical(StrategyManagerLog) << "External strategy process failed:" << m_strategyID << p_errorMessage;
    if (m_logger)
    {
        m_logger->log(QtCriticalMsg, p_errorMessage);
    }

    if (m_mainAlgo == nullptr || m_mainAlgo->getStrategyManager() == nullptr)
    {
        return;
    }

    StrategyManager* const strategyManager = m_mainAlgo->getStrategyManager();
    QMetaObject::invokeMethod(
        strategyManager,
        [strategyManager, strategyID = m_strategyID, errorMessage = p_errorMessage]()
        { strategyManager->markStrategyFailed(strategyID, errorMessage); },
        Qt::QueuedConnection);
}

void ProcessStrategyRuntimeBackend::scheduleSocketCleanup(const QString& p_errorMessage)
{
    if (m_socketNotifier)
    {
        m_socketNotifier->setEnabled(false);
    }

    QTimer::singleShot(0,
                       &m_process,
                       [this, errorMessage = p_errorMessage]()
                       {
                           cleanupSocketResources();
                           if (!m_shutdownRequested && !errorMessage.isEmpty())
                           {
                               reportFailure(errorMessage);
                           }
                       });
}

QString ProcessStrategyRuntimeBackend::waitForStartReady()
{
    m_waitingForStartReady = true;
    m_startReadyReceived = false;

    QElapsedTimer deadline;
    deadline.start();

    while (!m_startReadyReceived)
    {
        if (m_process.state() == QProcess::NotRunning)
        {
            m_waitingForStartReady = false;
            return m_lastFailureMessage.isEmpty() ? QStringLiteral("Strategy process exited before reporting ready")
                                                  : m_lastFailureMessage;
        }

        if (m_failureReported)
        {
            m_waitingForStartReady = false;
            return m_lastFailureMessage.isEmpty() ? QStringLiteral("Strategy failed while waiting for ready")
                                                  : m_lastFailureMessage;
        }

        if (deadline.elapsed() >= kStartupTimeoutMs)
        {
            m_waitingForStartReady = false;
            return QStringLiteral("Timed out waiting for strategy ready acknowledgement");
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }

    m_waitingForStartReady = false;
    return "";
}

QString ProcessStrategyRuntimeBackend::openListeningSocket()
{
    cleanupSocketResources();

    m_socketPath =
        QDir::tempPath() + QString("/l2t_%1.sock").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(12));
    if (m_socketPath.size() >= static_cast<int>(sizeof(sockaddr_un::sun_path)))
    {
        return "Generated strategy socket path is too long: " + m_socketPath;
    }

    const int serverFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        return QString("Failed to create strategy socket: %1").arg(QString::fromUtf8(std::strerror(errno)));
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const QByteArray encodedPath = QFile::encodeName(m_socketPath);
    std::strncpy(address.sun_path, encodedPath.constData(), sizeof(address.sun_path) - 1);

    ::unlink(address.sun_path);
    if (::bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        const QString error =
            QString("Failed to bind strategy socket: %1").arg(QString::fromUtf8(std::strerror(errno)));
        ::close(serverFd);
        ::unlink(address.sun_path);
        return error;
    }

    if (::listen(serverFd, 1) != 0)
    {
        const QString error =
            QString("Failed to listen on strategy socket: %1").arg(QString::fromUtf8(std::strerror(errno)));
        ::close(serverFd);
        ::unlink(address.sun_path);
        return error;
    }

    m_serverFd = serverFd;
    return "";
}

QString ProcessStrategyRuntimeBackend::ensureProcessStarted()
{
    m_process.setProgram(m_config.executablePath);
    m_process.setArguments({});
    m_process.setWorkingDirectory(QFileInfo(m_config.executablePath).absolutePath());

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert("OPENTRADERPLATFORM_STRATEGY_ID", m_strategyID);
    environment.insert("OPENTRADERPLATFORM_STRATEGY_SOCKET", m_socketPath);
    m_process.setProcessEnvironment(environment);

    m_process.start();
    if (!m_process.waitForStarted(kStartupTimeoutMs))
    {
        return "Failed to start strategy process: " + m_process.errorString();
    }

    return "";
}

QString ProcessStrategyRuntimeBackend::acceptClientConnection()
{
    if (m_serverFd < 0)
    {
        return "Strategy socket listener is not open";
    }

    if (!waitForFd(m_serverFd, POLLIN, kStartupTimeoutMs))
    {
        return "Timed out waiting for strategy process to connect";
    }

    const int clientFd = ::accept(m_serverFd, nullptr, nullptr);
    if (clientFd < 0)
    {
        return QString("Failed to accept strategy connection: %1").arg(QString::fromUtf8(std::strerror(errno)));
    }

    ::close(m_serverFd);
    m_serverFd = -1;
    m_clientFd = clientFd;
    return "";
}

QString ProcessStrategyRuntimeBackend::completeHandshake()
{
    if (m_clientFd < 0)
    {
        return "Strategy socket client is not connected";
    }

    std::vector<std::uint8_t> payload;
    if (!readFrameWithTimeout(m_clientFd, &payload, kStartupTimeoutMs))
    {
        return "Timed out waiting for strategy handshake";
    }

    Protocol::StrategyToHostEnvelope envelope;
    if (!OpenTraderPlatform::StrategySDK::parseMessagePayload(payload, &envelope))
    {
        return "Failed to parse strategy handshake payload";
    }

    if (envelope.payload_case() != Protocol::StrategyToHostEnvelope::kHandshakeHello)
    {
        return "Strategy sent a non-handshake payload before initialization completed";
    }

    const auto& handshake = envelope.handshake_hello();
    if (!OpenTraderPlatform::StrategyProtocol::isCompatibleProtocol(handshake.protocol_version()))
    {
        const QString rejectionReason = QString("Unsupported strategy protocol version for %1").arg(m_config.name);
        const QString ackError =
            sendHandshakeAck(false, rejectionReason, QString::fromStdString(envelope.correlation_id()));
        if (!ackError.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send handshake rejection:" << ackError;
        }
        return rejectionReason;
    }

    const QString handshakeName = QString::fromStdString(handshake.strategy_name());
    const QString handshakeVersion = QString::fromStdString(handshake.strategy_version());
    if (handshakeName != m_config.name || handshakeVersion != m_config.version)
    {
        const QString rejectionReason =
            QString("Strategy metadata mismatch for %1 (expected %2 %3, got %4 %5)")
                .arg(m_strategyID, m_config.name, m_config.version, handshakeName, handshakeVersion);
        const QString ackError =
            sendHandshakeAck(false, rejectionReason, QString::fromStdString(envelope.correlation_id()));
        if (!ackError.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to send handshake rejection:" << ackError;
        }
        return rejectionReason;
    }

    QString error = sendHandshakeAck(true, "", QString::fromStdString(envelope.correlation_id()));
    if (!error.isEmpty())
    {
        return error;
    }

    const int socketFlags = ::fcntl(m_clientFd, F_GETFL, 0);
    if (socketFlags < 0 || ::fcntl(m_clientFd, F_SETFL, socketFlags | O_NONBLOCK) != 0)
    {
        return QString("Failed to configure strategy socket as non-blocking: %1")
            .arg(QString::fromUtf8(std::strerror(errno)));
    }

    m_socketNotifier = std::make_unique<QSocketNotifier>(m_clientFd, QSocketNotifier::Read);
    QObject::connect(m_socketNotifier.get(),
                     &QSocketNotifier::activated,
                     &m_process,
                     [this]()
                     {
                         if (m_socketNotifier)
                         {
                             drainInboundSocket();
                         }
                     });

    error = sendStartCommand();
    if (!error.isEmpty())
    {
        return error;
    }

    qInfo(StrategyManagerLog) << "Accepted strategy process handshake:" << m_strategyID
                              << "SDK:" << QString::fromStdString(handshake.sdk_name())
                              << QString::fromStdString(handshake.sdk_version())
                              << "strategy:" << QString::fromStdString(handshake.strategy_name())
                              << QString::fromStdString(handshake.strategy_version());
    return "";
}

QString ProcessStrategyRuntimeBackend::sendHandshakeAck(const bool p_accepted,
                                                        const QString& p_rejectionReason,
                                                        const QString& p_correlationID)
{
    if (m_clientFd < 0)
    {
        return "Strategy socket is not connected";
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    envelope.set_correlation_id(p_correlationID.toStdString());

    auto* const handshakeAck = envelope.mutable_handshake_ack();
    OpenTraderPlatform::StrategyProtocol::populateProtocolVersion(handshakeAck->mutable_protocol_version());
    handshakeAck->mutable_endpoint()->set_strategy_id(m_strategyID.toStdString());
    handshakeAck->mutable_endpoint()->set_socket_path(m_socketPath.toStdString());
    handshakeAck->set_accepted(p_accepted);
    handshakeAck->set_rejection_reason(p_rejectionReason.toStdString());

    if (!writeMessage(m_clientFd, envelope))
    {
        return "Failed to send handshake acknowledgement to strategy process";
    }

    return "";
}

QString ProcessStrategyRuntimeBackend::sendStartCommand()
{
    if (m_clientFd < 0)
    {
        return "Strategy socket is not connected";
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);

    auto* const startCommand = envelope.mutable_start();
    auto* const configuration = startCommand->mutable_configuration();
    configuration->set_name(m_config.name.toStdString());
    configuration->set_executable_path(m_config.executablePath.toStdString());
    populateFieldValues(m_config.fieldValues, configuration->mutable_custom_params());

    if (!writeMessage(m_clientFd, envelope))
    {
        return "Failed to send start command to strategy process";
    }

    return "";
}

QString ProcessStrategyRuntimeBackend::sendPauseCommand(const QString& p_reason)
{
    if (m_clientFd < 0)
    {
        return "Strategy socket is not connected";
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    envelope.mutable_pause()->set_reason(p_reason.toStdString());

    if (!writeMessage(m_clientFd, envelope))
    {
        return "Failed to send pause command to strategy process";
    }

    return "";
}

QString ProcessStrategyRuntimeBackend::sendResumeCommand()
{
    if (m_clientFd < 0)
    {
        return "Strategy socket is not connected";
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    envelope.mutable_resume();

    if (!writeMessage(m_clientFd, envelope))
    {
        return "Failed to send resume command to strategy process";
    }

    return "";
}

QString ProcessStrategyRuntimeBackend::sendStopLikeCommand(const bool p_shutdown, const QString& p_reason)
{
    if (m_clientFd < 0)
    {
        return "";
    }

    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);

    if (p_shutdown)
    {
        envelope.mutable_shutdown()->set_reason(p_reason.toStdString());
    }
    else
    {
        envelope.mutable_stop()->set_reason(p_reason.toStdString());
    }

    if (!writeMessage(m_clientFd, envelope))
    {
        return QString("Failed to send %1 command to strategy process").arg(p_shutdown ? "shutdown" : "stop");
    }

    return "";
}

bool ProcessStrategyRuntimeBackend::sendHostEnvelope(const Protocol::HostToStrategyEnvelope& p_envelope,
                                                     const QString& p_context)
{
    if (m_clientFd < 0)
    {
        return false;
    }

    if (writeMessage(m_clientFd, p_envelope))
    {
        return true;
    }

    scheduleSocketCleanup(QString("Failed to %1").arg(p_context));
    return false;
}

bool ProcessStrategyRuntimeBackend::waitForProcessExitWithEvents(const int p_timeoutMs)
{
    if (m_process.state() == QProcess::NotRunning)
    {
        return true;
    }

    QElapsedTimer deadline;
    deadline.start();

    while (m_process.state() != QProcess::NotRunning)
    {
        if (m_process.waitForFinished(25))
        {
            return true;
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        if (deadline.elapsed() >= p_timeoutMs)
        {
            return m_process.state() == QProcess::NotRunning;
        }
    }

    return true;
}

void ProcessStrategyRuntimeBackend::sendErrorMessage(const QString& p_code,
                                                     const QString& p_message,
                                                     const QString& p_correlationID)
{
    Protocol::HostToStrategyEnvelope envelope;
    envelope.set_sequence(m_outboundSequence++);
    envelope.set_correlation_id(p_correlationID.toStdString());
    auto* const error = envelope.mutable_error();
    error->set_code(p_code.toStdString());
    error->set_message(p_message.toStdString());
    [[maybe_unused]] const bool sent = sendHostEnvelope(envelope, "send error message");
}

void ProcessStrategyRuntimeBackend::cleanupSocketResources()
{
    m_socketNotifier.reset();

    if (m_clientFd >= 0)
    {
        ::close(m_clientFd);
        m_clientFd = -1;
    }

    if (m_serverFd >= 0)
    {
        ::close(m_serverFd);
        m_serverFd = -1;
    }

    if (!m_socketPath.isEmpty())
    {
        const QByteArray encodedPath = QFile::encodeName(m_socketPath);
        ::unlink(encodedPath.constData());
        m_socketPath.clear();
    }

    m_receiveBuffer.clear();
}

void ProcessStrategyRuntimeBackend::resetStartAttemptState()
{
    cleanupSocketResources();
    m_outboundSequence = 1;
    m_shutdownRequested = false;
    m_failureReported = false;
    m_waitingForStartReady = false;
    m_startReadyReceived = false;
    m_lastFailureMessage.clear();
}

void ProcessStrategyRuntimeBackend::markShutdownRequested()
{
    m_shutdownRequested = true;
}
