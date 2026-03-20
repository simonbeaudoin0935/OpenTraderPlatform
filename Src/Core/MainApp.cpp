#include "MainApp.h"
#include "Assume.h"
#include "DatabaseThread.h"
#include "Logging.h"
#include "PlatformControlProtocol.h"
#include "PlatformControlServer.h"
#include "Settings.h"
#include "Stream.h"
#include "TimeFrame.h"
#include "CONSTANTS.h"
#include "OrdersDatabase.h"
#include "PositionsDatabase.h"
#include "DBClient.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#ifdef GUI_ENABLED
#include "GUIFrontend.h"
#else
#include "TUIFrontend.h"
#endif

QDateTime MainApp::currentAppReplayTime = QDateTime::fromSecsSinceEpoch(0);

// Initialize static members
MainApp* MainApp::m_instance = nullptr;
TradingMode MainApp::m_tradingMode = TradingMode::Sim; // Default to Sim for safety
DataSourceMode MainApp::m_dataSourceMode = DataSourceMode::Live;

MainApp* MainApp::getInstance()
{
    if (m_instance == nullptr)
    {
        // Load trading mode from settings before creating instance
        // (TSClient needs this during construction)
        Q_CHECK_PTR(appStateSettings);
        int savedMode = appStateSettings->value("Trading/Mode", static_cast<int>(TradingMode::Sim)).toInt();
        m_tradingMode = static_cast<TradingMode>(savedMode);
        qInfo() << "Trading mode loaded:" << (m_tradingMode == TradingMode::Sim ? "SIM" : "LIVE");

        qInfo() << "MainApp singleton instance created";
        m_instance = new MainApp();
    }
    return m_instance;
}

void MainApp::destroyInstance()
{
    ASSUME_TRUE(m_instance != nullptr);
    qInfo() << "Destroying MainApp singleton instance";
    delete m_instance;
    m_instance = nullptr;
}

bool MainApp::isInReplayMode()
{
    return m_dataSourceMode == DataSourceMode::Replay;
}

DataSourceMode MainApp::getDataSourceMode()
{
    return m_dataSourceMode;
}

namespace
{
    inline constexpr auto kReplayDateSettingsKey = "Replay/Date";
    inline constexpr auto kReplayStartTimeSettingsKey = "Replay/StartTime";
    inline constexpr auto kReplaySpeedSettingsKey = "Replay/Speed";

    [[nodiscard]] QString tradingModeToString(const TradingMode p_mode)
    {
        switch (p_mode)
        {
        case TradingMode::Live:
            return "live";
        case TradingMode::Sim:
        default:
            return "sim";
        }
    }

    [[nodiscard]] QString dataSourceModeToString(const DataSourceMode p_mode)
    {
        switch (p_mode)
        {
        case DataSourceMode::Replay:
            return "replay";
        case DataSourceMode::Live:
        default:
            return "live";
        }
    }

    [[nodiscard]] QString replayStateToString(const Playback::State p_state)
    {
        switch (p_state)
        {
        case Playback::State::Playing:
            return "playing";
        case Playback::State::Paused:
            return "paused";
        case Playback::State::Stopped:
        default:
            return "stopped";
        }
    }

    [[nodiscard]] QString tradeSideToString(const TradeSide p_side)
    {
        switch (p_side)
        {
        case TradeSide::Ask:
            return "ask";
        case TradeSide::Bid:
            return "bid";
        case TradeSide::None:
        default:
            return "none";
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

    [[nodiscard]] QJsonValue optionalDateTimeToJsonValue(const std::optional<QDateTime>& p_value)
    {
        if (!p_value.has_value() || !p_value->isValid())
        {
            return QJsonValue(QJsonValue::Null);
        }
        return p_value->toString(Qt::ISODateWithMs);
    }

    [[nodiscard]] QJsonValue dateTimeToJsonValue(const QDateTime& p_value)
    {
        if (!p_value.isValid())
        {
            return QJsonValue(QJsonValue::Null);
        }
        return p_value.toString(Qt::ISODateWithMs);
    }

    [[nodiscard]] QJsonValue optionalDoubleToJsonValue(const std::optional<double>& p_value)
    {
        if (!p_value.has_value())
        {
            return QJsonValue(QJsonValue::Null);
        }
        return p_value.value();
    }

    [[nodiscard]] QJsonObject serializeBar(const Bar& p_bar)
    {
        return QJsonObject{
            {"timestamp", p_bar.getTimestamp().toString(Qt::ISODateWithMs)},
            {"open", p_bar.getOpen()},
            {"high", p_bar.getHigh()},
            {"low", p_bar.getLow()},
            {"close", p_bar.getClose()},
            {"totalVolume", static_cast<qint64>(p_bar.getTotalVolume())},
            {"status", Bar::barStatusToString(p_bar.getBarStatus())},
        };
    }

    [[nodiscard]] QJsonArray serializeBars(const QVector<Bar>& p_bars)
    {
        QJsonArray items;
        for (const Bar& bar: p_bars)
        {
            items.append(serializeBar(bar));
        }
        return items;
    }

    [[nodiscard]] QJsonObject serializeTrade(const Trade& p_trade)
    {
        return QJsonObject{
            {"symbol", p_trade.m_symbol},
            {"timestamp", p_trade.m_timestamp.toString(Qt::ISODateWithMs)},
            {"price", p_trade.m_price},
            {"size", p_trade.m_size},
            {"side", tradeSideToString(p_trade.m_side)},
        };
    }

    [[nodiscard]] QJsonArray serializeTrades(const QVector<Trade>& p_trades)
    {
        QJsonArray items;
        for (const Trade& trade: p_trades)
        {
            items.append(serializeTrade(trade));
        }
        return items;
    }

    [[nodiscard]] QJsonObject serializeLevel2Row(const Level2Row& p_row)
    {
        return QJsonObject{
            {"price", p_row.m_price},
            {"size", p_row.m_size},
            {"orderCount", p_row.m_orderCount},
        };
    }

    [[nodiscard]] QJsonObject serializeLevel2(const Level2& p_level2)
    {
        QJsonArray bids;
        for (const Level2Row& row: p_level2.m_bids)
        {
            bids.append(serializeLevel2Row(row));
        }

        QJsonArray asks;
        for (const Level2Row& row: p_level2.m_asks)
        {
            asks.append(serializeLevel2Row(row));
        }

        return QJsonObject{
            {"symbol", p_level2.m_symbol},
            {"timestamp", p_level2.m_timeStamp.toString(Qt::ISODateWithMs)},
            {"bids", bids},
            {"asks", asks},
        };
    }

    [[nodiscard]] QJsonObject serializeActivityMetrics(const MainAlgo::ActivityMetrics& p_metrics)
    {
        return QJsonObject{
            {"tradeRateHz", p_metrics.tradeRateHz},
            {"l2RateHz", p_metrics.l2RateHz},
            {"isActive", p_metrics.isActive},
        };
    }

    [[nodiscard]] QJsonObject serializeAccountDetail(const AccountDetail& p_detail)
    {
        return QJsonObject{
            {"isStockLocateEligible", p_detail.isStockLocateEligible},
            {"enrolledInRegTProgram", p_detail.enrolledInRegTProgram},
            {"requiresBuyingPowerWarning", p_detail.requiresBuyingPowerWarning},
            {"dayTradingQualified", p_detail.dayTradingQualified},
            {"optionApprovalLevel", p_detail.optionApprovalLevel},
            {"patternDayTrader", p_detail.patternDayTrader},
        };
    }

    [[nodiscard]] QJsonObject serializeAccount(const Account& p_account, const QString& p_activeAccountId)
    {
        QJsonObject item{
            {"accountId", p_account.getAccountId()},
            {"accountType", AccountType::accountTypeToString(p_account.getAccountType().type)},
            {"status", p_account.getStatus()},
            {"currency", p_account.getCurrency()},
            {"isActive", !p_activeAccountId.isEmpty() && p_account.getAccountId() == p_activeAccountId},
        };
        item["accountDetail"] = p_account.getAccountDetail().has_value()
                                    ? QJsonValue(serializeAccountDetail(p_account.getAccountDetail().value()))
                                    : QJsonValue(QJsonValue::Null);
        return item;
    }

    [[nodiscard]] QJsonArray serializeAccounts(const QVector<Account>& p_accounts, const QString& p_activeAccountId)
    {
        QJsonArray items;
        for (const Account& account: p_accounts)
        {
            items.append(serializeAccount(account, p_activeAccountId));
        }
        return items;
    }

    [[nodiscard]] QJsonObject serializeBalance(const Balance& p_balance)
    {
        return QJsonObject{
            {"accountId", p_balance.getAccountID()},
            {"accountType", AccountType::accountTypeToString(p_balance.getAccountType().type)},
            {"buyingPower", p_balance.getBuyingPower()},
            {"cashBalance", p_balance.getCashBalance()},
            {"commission", p_balance.getComission()},
            {"equity", p_balance.getEquity()},
            {"marketValue", p_balance.getMarketValue()},
            {"todaysProfitLoss", p_balance.getTodaysProfitLoss()},
            {"unclearedDeposit", p_balance.getUnclearedDeposit()},
        };
    }

    [[nodiscard]] QJsonObject serializePosition(const Position& p_position)
    {
        return QJsonObject{
            {"positionId", p_position.getPositionID()},
            {"accountId", p_position.getAccountID()},
            {"symbol", p_position.getSymbol()},
            {"quantity", p_position.getQuantity()},
            {"averagePrice", p_position.getAveragePrice()},
            {"last", p_position.getLast()},
            {"bid", p_position.getBid()},
            {"ask", p_position.getAsk()},
            {"markToMarketPrice", p_position.getMarkToMarketPrice()},
            {"marketValue", p_position.getMarketValue()},
            {"totalCost", p_position.getTotalCost()},
            {"unrealizedProfitLoss", p_position.getUnrealizedProfitLoss()},
            {"unrealizedProfitLossPercent", p_position.getUnrealizedProfitLossPercent()},
            {"unrealizedProfitLossQty", p_position.getUnrealizedProfitLossQty()},
            {"todaysProfitLoss", p_position.getTodaysProfitLoss()},
            {"longShort", p_position.getLongShort()},
            {"assetType", p_position.getAssetType()},
            {"conversionRate", p_position.getConversionRate()},
            {"dayTradeRequirement", p_position.getDayTradeRequirement()},
            {"initialRequirement", p_position.getInitialRequirement()},
            {"maintenanceMargin", p_position.getMaintenanceMargin()},
            {"expirationDate", dateTimeToJsonValue(p_position.getExpirationDate())},
            {"timestamp", dateTimeToJsonValue(p_position.getTimestamp())},
            {"deleted", p_position.isDeleted()},
        };
    }

    [[nodiscard]] QJsonArray serializePositions(const QVector<Position>& p_positions)
    {
        QJsonArray items;
        for (const Position& position: p_positions)
        {
            items.append(serializePosition(position));
        }
        return items;
    }

    [[nodiscard]] QJsonObject serializeOrder(const Order& p_order, std::optional<qint64> p_latencyMs = std::nullopt)
    {
        return QJsonObject{
            {"orderId", p_order.getOrderID()},
            {"accountId", p_order.getAccountID()},
            {"symbol", p_order.getSymbol()},
            {"quantity", p_order.getQuantity()},
            {"tradeAction", p_order.getTradeAction()},
            {"duration", p_order.getDuration()},
            {"orderType", OrderType::toString(p_order.getOrderType().type)},
            {"status", QtEnum::toString(p_order.getOrderStatus())},
            {"statusDescription", p_order.getStatusDescription()},
            {"limitPrice", optionalDoubleToJsonValue(p_order.getLimitPrice())},
            {"stopPrice", optionalDoubleToJsonValue(p_order.getStopPrice())},
            {"filledPrice", p_order.getFilledPrice()},
            {"openedDateTime", dateTimeToJsonValue(p_order.getOpenedDateTime())},
            {"closedDateTime", dateTimeToJsonValue(p_order.getClosedDateTime())},
            {"rejectReason",
             p_order.getRejectReason().has_value() ? QJsonValue(p_order.getRejectReason().value())
                                                   : QJsonValue(QJsonValue::Null)},
            {"latencyMs",
             p_latencyMs.has_value()
                 ? QJsonValue(static_cast<qint64>(p_latencyMs.value()))
                 : (p_order.getLatencyMs().has_value() ? QJsonValue(static_cast<qint64>(p_order.getLatencyMs().value()))
                                                       : QJsonValue(QJsonValue::Null))},
            {"strategyLog",
             p_order.getStrategyLog().has_value() ? QJsonValue(p_order.getStrategyLog().value())
                                                  : QJsonValue(QJsonValue::Null)},
        };
    }

    [[nodiscard]] QJsonObject serializeOrderResultItem(const OrderResultItem& p_item)
    {
        QJsonObject item{
            {"orderId", p_item.getOrderID()},
            {"message", p_item.getMessage()},
        };
        if (p_item.getError().has_value())
        {
            item["error"] = p_item.getError().value();
        }
        return item;
    }

    [[nodiscard]] QJsonObject serializePlaceOrderResult(const PlaceOrderResult& p_result)
    {
        QJsonArray orders;
        for (const OrderResultItem& item: p_result.getOrders())
        {
            orders.append(serializeOrderResultItem(item));
        }

        QJsonArray errors;
        for (const OrderResultItem& item: p_result.getErrors())
        {
            errors.append(serializeOrderResultItem(item));
        }

        return QJsonObject{
            {"orders", orders},
            {"errors", errors},
            {"hasErrors", p_result.hasErrors()},
            {"allSuccessful", p_result.isAllSuccessful()},
        };
    }

    [[nodiscard]] QJsonObject serializeCancelOrderResult(const CancelOrderResult& p_result)
    {
        QJsonObject item{
            {"orderId", p_result.getOrderID()},
            {"message", p_result.getMessage()},
            {"isError", p_result.isError()},
        };
        if (p_result.getError().has_value())
        {
            item["error"] = p_result.getError().value();
        }
        return item;
    }

    [[nodiscard]] QString tradeActionToJsonString(TradeAction p_action)
    {
        switch (p_action)
        {
        case TradeAction::Buy:
            return "buy";
        case TradeAction::Sell:
            return "sell";
        case TradeAction::BuyToCover:
            return "buy-to-cover";
        case TradeAction::SellShort:
            return "sell-short";
        case TradeAction::BuyToOpen:
            return "buy-to-open";
        case TradeAction::BuyToClose:
            return "buy-to-close";
        case TradeAction::SellToOpen:
            return "sell-to-open";
        case TradeAction::SellToClose:
            return "sell-to-close";
        }

        return "unknown";
    }

    [[nodiscard]] QString orderDurationToJsonString(OrderDuration p_duration)
    {
        switch (p_duration)
        {
        case OrderDuration::Day:
            return "day";
        case OrderDuration::DayPlus:
            return "day-plus";
        case OrderDuration::GTC:
            return "gtc";
        case OrderDuration::GTCPlus:
            return "gtc-plus";
        case OrderDuration::IOC:
            return "ioc";
        case OrderDuration::FOK:
            return "fok";
        case OrderDuration::GTD:
            return "gtd";
        case OrderDuration::GTDPlus:
            return "gtd-plus";
        case OrderDuration::Opening:
            return "opening";
        case OrderDuration::OnClose:
            return "on-close";
        case OrderDuration::OneMinute:
            return "one-minute";
        case OrderDuration::ThreeMinutes:
            return "three-minutes";
        case OrderDuration::FiveMinutes:
            return "five-minutes";
        }

        return "unknown";
    }

    template<typename T> [[nodiscard]] T waitForFutureResult(QFuture<T> p_future)
    {
        ASSUME_TRUE(p_future.isValid());
        if (!p_future.isFinished())
        {
            QEventLoop loop;
            QFutureWatcher<T> watcher;
            QObject::connect(&watcher, &QFutureWatcherBase::finished, &loop, &QEventLoop::quit);
            watcher.setFuture(p_future);
            if (!watcher.isFinished())
            {
                loop.exec();
            }
        }

        return p_future.result();
    }

    [[nodiscard]] QJsonObject makeControlResponse(bool p_ok,
                                                  const QString& p_message,
                                                  const QJsonObject& p_result = {},
                                                  const QString& p_error = {})
    {
        QJsonObject response;
        response["protocolVersion"] = PlatformControlProtocol::kProtocolVersion;
        response["ok"] = p_ok;
        response["message"] = p_message;
        if (!p_error.isEmpty())
        {
            response["error"] = p_error;
        }
        if (!p_result.isEmpty())
        {
            response["result"] = p_result;
        }
        return response;
    }

    [[nodiscard]] std::expected<QDate, QString> parseRequiredDate(const QJsonObject& p_arguments)
    {
        const QString rawValue = p_arguments.value("date").toString().trimmed();
        if (rawValue.isEmpty())
        {
            return std::unexpected("Missing required 'date' argument (expected YYYY-MM-DD)");
        }

        const QDate date = QDate::fromString(rawValue, Qt::ISODate);
        if (!date.isValid())
        {
            return std::unexpected(QString("Invalid date '%1' (expected YYYY-MM-DD)").arg(rawValue));
        }

        return date;
    }

    [[nodiscard]] std::expected<QTime, QString>
    parseRequiredTimeForKey(const QJsonObject& p_arguments, const QString& p_key, const QString& p_expectedFormat)
    {
        const QString rawValue = p_arguments.value(p_key).toString().trimmed();
        if (rawValue.isEmpty())
        {
            return std::unexpected(
                QString("Missing required '%1' argument (expected %2)").arg(p_key, p_expectedFormat));
        }

        const QTime time = QTime::fromString(rawValue, Qt::ISODate);
        if (!time.isValid())
        {
            return std::unexpected(QString("Invalid %1 '%2' (expected %3)").arg(p_key, rawValue, p_expectedFormat));
        }

        return time;
    }

    [[nodiscard]] std::expected<QTime, QString> parseRequiredTime(const QJsonObject& p_arguments)
    {
        return parseRequiredTimeForKey(p_arguments, "startTime", "HH:MM[:SS]");
    }

    [[nodiscard]] std::expected<Playback::Speed, QString> parseRequiredReplaySpeed(const QJsonObject& p_arguments)
    {
        const QJsonValue speedValue = p_arguments.value("speed");
        if (speedValue.isUndefined())
        {
            return std::unexpected(QString("Missing required 'speed' argument (supported values: %1)")
                                       .arg(PlatformControlProtocol::supportedReplaySpeeds().join(", ")));
        }

        if (speedValue.isDouble())
        {
            return PlatformControlProtocol::replaySpeedFromString(QString::number(speedValue.toInt()));
        }

        return PlatformControlProtocol::replaySpeedFromString(speedValue.toString());
    }

    [[nodiscard]] std::expected<QString, QString> parseOptionalSymbol(const QJsonObject& p_arguments)
    {
        const QJsonValue symbolValue = p_arguments.value("symbol");
        if (symbolValue.isUndefined() || symbolValue.isNull())
        {
            return QString();
        }
        if (!symbolValue.isString())
        {
            return std::unexpected("Argument 'symbol' must be a string");
        }
        return symbolValue.toString().trimmed().toUpper();
    }

    [[nodiscard]] std::expected<QString, QString> parseRequiredStringForKey(const QJsonObject& p_arguments,
                                                                            const QString& p_key)
    {
        const QJsonValue value = p_arguments.value(p_key);
        if (!value.isString())
        {
            return std::unexpected(QString("Missing required '%1' argument").arg(p_key));
        }

        const QString stringValue = value.toString().trimmed();
        if (stringValue.isEmpty())
        {
            return std::unexpected(QString("Missing required '%1' argument").arg(p_key));
        }

        return stringValue;
    }

    [[nodiscard]] std::expected<QString, QString> parseOptionalStringForKey(const QJsonObject& p_arguments,
                                                                            const QString& p_key)
    {
        const QJsonValue value = p_arguments.value(p_key);
        if (value.isUndefined() || value.isNull())
        {
            return QString();
        }
        if (!value.isString())
        {
            return std::unexpected(QString("Argument '%1' must be a string").arg(p_key));
        }
        return value.toString().trimmed();
    }

    [[nodiscard]] QString normalizeToken(QString p_value)
    {
        QString normalized;
        normalized.reserve(p_value.size());
        for (const QChar ch: p_value.trimmed())
        {
            if (ch.isSpace() || ch == '-' || ch == '_')
            {
                continue;
            }
            normalized.append(ch.toLower());
        }
        return normalized;
    }

    [[nodiscard]] std::expected<int, QString> parseRequiredPositiveIntForKey(const QJsonObject& p_arguments,
                                                                             const QString& p_key)
    {
        const QJsonValue value = p_arguments.value(p_key);
        if (value.isUndefined())
        {
            return std::unexpected(QString("Missing required '%1' argument").arg(p_key));
        }

        int parsedValue = 0;
        if (value.isDouble())
        {
            const double doubleValue = value.toDouble();
            parsedValue = static_cast<int>(doubleValue);
            if (doubleValue != static_cast<double>(parsedValue))
            {
                return std::unexpected(QString("Argument '%1' must be an integer").arg(p_key));
            }
        }
        else if (value.isString())
        {
            bool ok = false;
            parsedValue = value.toString().trimmed().toInt(&ok);
            if (!ok)
            {
                return std::unexpected(QString("Argument '%1' must be an integer").arg(p_key));
            }
        }
        else
        {
            return std::unexpected(QString("Argument '%1' must be an integer").arg(p_key));
        }

        if (parsedValue <= 0)
        {
            return std::unexpected(QString("Argument '%1' must be greater than 0").arg(p_key));
        }
        return parsedValue;
    }

    [[nodiscard]] std::expected<std::optional<double>, QString>
    parseOptionalPositiveDoubleForKey(const QJsonObject& p_arguments, const QString& p_key)
    {
        const QJsonValue value = p_arguments.value(p_key);
        if (value.isUndefined() || value.isNull())
        {
            return std::nullopt;
        }

        double parsedValue = 0.0;
        if (value.isDouble())
        {
            parsedValue = value.toDouble();
        }
        else if (value.isString())
        {
            bool ok = false;
            parsedValue = value.toString().trimmed().toDouble(&ok);
            if (!ok)
            {
                return std::unexpected(QString("Argument '%1' must be a number").arg(p_key));
            }
        }
        else
        {
            return std::unexpected(QString("Argument '%1' must be a number").arg(p_key));
        }

        if (parsedValue <= 0.0)
        {
            return std::unexpected(QString("Argument '%1' must be greater than 0").arg(p_key));
        }
        return parsedValue;
    }

    [[nodiscard]] std::expected<int, QString> parseOptionalMaxCount(const QJsonObject& p_arguments)
    {
        const QJsonValue maxCountValue = p_arguments.value("maxCount");
        if (maxCountValue.isUndefined())
        {
            return PlatformControlConstants::DEFAULT_TRADES_SNAPSHOT_MAX_COUNT;
        }

        int maxCount = 0;
        if (maxCountValue.isDouble())
        {
            maxCount = maxCountValue.toInt();
        }
        else if (maxCountValue.isString())
        {
            bool ok = false;
            maxCount = maxCountValue.toString().trimmed().toInt(&ok);
            if (!ok)
            {
                return std::unexpected("Argument 'maxCount' must be an integer");
            }
        }
        else
        {
            return std::unexpected("Argument 'maxCount' must be an integer");
        }

        if (maxCount <= 0)
        {
            return std::unexpected("Argument 'maxCount' must be greater than 0");
        }
        if (maxCount > PlatformControlConstants::MAX_TRADES_SNAPSHOT_MAX_COUNT)
        {
            return std::unexpected(QString("Argument 'maxCount' exceeds the maximum allowed value of %1")
                                       .arg(PlatformControlConstants::MAX_TRADES_SNAPSHOT_MAX_COUNT));
        }
        return maxCount;
    }

    [[nodiscard]] std::expected<TimeFrame, QString> parseOptionalTimeFrame(const QJsonObject& p_arguments)
    {
        const QJsonValue timeFrameValue = p_arguments.value("timeFrame");
        if (timeFrameValue.isUndefined() || timeFrameValue.isNull())
        {
            return TimeFrame::ONE_MINUTE;
        }
        if (!timeFrameValue.isString())
        {
            return std::unexpected("Argument 'timeFrame' must be a string");
        }

        const QString timeFrame = timeFrameValue.toString().trimmed();
        if (!PlatformControlProtocol::supportedBarTimeFrames().contains(timeFrame))
        {
            return std::unexpected(QString("Unsupported timeFrame '%1'. Supported values: %2")
                                       .arg(timeFrame, PlatformControlProtocol::supportedBarTimeFrames().join(", ")));
        }
        return stringToTimeFrame(timeFrame);
    }

    [[nodiscard]] std::expected<TradeAction, QString> parseRequiredTradeAction(const QJsonObject& p_arguments)
    {
        const auto tradeAction = parseRequiredStringForKey(p_arguments, "tradeAction");
        if (!tradeAction.has_value())
        {
            return std::unexpected(tradeAction.error());
        }

        const QString normalized = normalizeToken(tradeAction.value());
        if (normalized == "buy")
            return TradeAction::Buy;
        if (normalized == "sell")
            return TradeAction::Sell;
        if (normalized == "buytocover")
            return TradeAction::BuyToCover;
        if (normalized == "sellshort")
            return TradeAction::SellShort;
        if (normalized == "buytoopen")
            return TradeAction::BuyToOpen;
        if (normalized == "buytoclose")
            return TradeAction::BuyToClose;
        if (normalized == "selltoopen")
            return TradeAction::SellToOpen;
        if (normalized == "selltoclose")
            return TradeAction::SellToClose;

        return std::unexpected(
            QString("Unsupported tradeAction '%1'. Supported values: buy, sell, buy-to-cover, sell-short, "
                    "buy-to-open, buy-to-close, sell-to-open, sell-to-close")
                .arg(tradeAction.value()));
    }

    [[nodiscard]] std::expected<OrderType::Type, QString> parseRequiredOrderType(const QJsonObject& p_arguments)
    {
        const auto orderType = parseRequiredStringForKey(p_arguments, "orderType");
        if (!orderType.has_value())
        {
            return std::unexpected(orderType.error());
        }

        const QString normalized = normalizeToken(orderType.value());
        if (normalized == "market")
            return OrderType::Type::Market;
        if (normalized == "limit")
            return OrderType::Type::Limit;
        if (normalized == "stopmarket")
            return OrderType::Type::StopMarket;
        if (normalized == "stoplimit")
            return OrderType::Type::StopLimit;

        return std::unexpected(
            QString("Unsupported orderType '%1'. Supported values: market, limit, stop-market, stop-limit")
                .arg(orderType.value()));
    }

    [[nodiscard]] std::expected<OrderDuration, QString> parseOptionalOrderDuration(const QJsonObject& p_arguments)
    {
        const auto duration = parseOptionalStringForKey(p_arguments, "duration");
        if (!duration.has_value())
        {
            return std::unexpected(duration.error());
        }
        if (duration->isEmpty())
        {
            return OrderDuration::Day;
        }

        const QString normalized = normalizeToken(duration.value());
        if (normalized == "day")
            return OrderDuration::Day;
        if (normalized == "dayplus")
            return OrderDuration::DayPlus;
        if (normalized == "gtc")
            return OrderDuration::GTC;
        if (normalized == "gtcplus")
            return OrderDuration::GTCPlus;
        if (normalized == "ioc")
            return OrderDuration::IOC;
        if (normalized == "fok")
            return OrderDuration::FOK;

        return std::unexpected(
            QString("Unsupported duration '%1'. Supported values: day, day-plus, gtc, gtc-plus, ioc, fok")
                .arg(duration.value()));
    }

    [[nodiscard]] std::expected<std::optional<Order::Status>, QString>
    parseOptionalOrderStatus(const QJsonObject& p_arguments)
    {
        const auto status = parseOptionalStringForKey(p_arguments, "status");
        if (!status.has_value())
        {
            return std::unexpected(status.error());
        }
        if (status->isEmpty())
        {
            return std::nullopt;
        }

        const QString normalized = status->trimmed().toUpper();
        const QMetaEnum meta = QMetaEnum::fromType<Order::Status>();
        bool ok = false;
        const int value = meta.keyToValue(normalized.toLatin1().constData(), &ok);
        if (ok)
        {
            return static_cast<Order::Status>(value);
        }

        QStringList supported;
        supported.reserve(meta.keyCount());
        for (int index = 0; index < meta.keyCount(); ++index)
        {
            supported.append(QString::fromLatin1(meta.key(index)));
        }

        return std::unexpected(
            QString("Unsupported status '%1'. Supported values: %2").arg(status.value(), supported.join(", ")));
    }

    [[nodiscard]] QDate configuredReplayDate()
    {
        Q_CHECK_PTR(appStateSettings);
        return QDate::fromString(appStateSettings->value(kReplayDateSettingsKey).toString(), Qt::ISODate);
    }

    [[nodiscard]] QTime configuredReplayStartTime()
    {
        Q_CHECK_PTR(appStateSettings);
        return QTime::fromString(appStateSettings->value(kReplayStartTimeSettingsKey).toString(), Qt::ISODate);
    }

    [[nodiscard]] Playback::Speed configuredReplaySpeed()
    {
        Q_CHECK_PTR(appStateSettings);
        const int savedSpeed =
            appStateSettings->value(kReplaySpeedSettingsKey, static_cast<int>(Playback::Speed::Normal)).toInt();
        const auto parsed = PlatformControlProtocol::replaySpeedFromString(QString::number(savedSpeed));
        if (parsed.has_value())
        {
            return parsed.value();
        }

        return Playback::Speed::Normal;
    }

    void persistReplayConfiguration(const QDate& p_date, const QTime& p_startTime, const Playback::Speed p_speed)
    {
        Q_CHECK_PTR(appStateSettings);
        if (p_date.isValid())
        {
            appStateSettings->setValue(kReplayDateSettingsKey, p_date.toString(Qt::ISODate));
        }
        if (p_startTime.isValid())
        {
            appStateSettings->setValue(kReplayStartTimeSettingsKey, p_startTime.toString(Qt::ISODate));
        }
        appStateSettings->setValue(kReplaySpeedSettingsKey, static_cast<int>(p_speed));
        appStateSettings->sync();
    }

    [[nodiscard]] std::expected<QDate, QString> parseReplayDateOrConfigured(const QJsonObject& p_arguments)
    {
        const QString rawValue = p_arguments.value("date").toString().trimmed();
        if (!rawValue.isEmpty())
        {
            const QDate date = QDate::fromString(rawValue, Qt::ISODate);
            if (!date.isValid())
            {
                return std::unexpected(QString("Invalid date '%1' (expected YYYY-MM-DD)").arg(rawValue));
            }

            return date;
        }

        const QDate date = configuredReplayDate();
        if (!date.isValid())
        {
            return std::unexpected("Missing required 'date' argument and no replay date is currently configured");
        }

        return date;
    }

    [[nodiscard]] std::expected<QTime, QString> parseReplayStartTimeOrConfigured(const QJsonObject& p_arguments)
    {
        const QString rawValue = p_arguments.value("startTime").toString().trimmed();
        if (!rawValue.isEmpty())
        {
            const QTime time = QTime::fromString(rawValue, Qt::ISODate);
            if (!time.isValid())
            {
                return std::unexpected(QString("Invalid startTime '%1' (expected HH:MM[:SS])").arg(rawValue));
            }

            return time;
        }

        const QTime time = configuredReplayStartTime();
        if (!time.isValid())
        {
            return std::unexpected(
                "Missing required 'startTime' argument and no replay start time is currently configured");
        }

        return time;
    }

    [[nodiscard]] std::expected<Playback::Speed, QString> parseReplaySpeedOrConfigured(const QJsonObject& p_arguments)
    {
        const QJsonValue speedValue = p_arguments.value("speed");
        if (speedValue.isUndefined())
        {
            return configuredReplaySpeed();
        }

        if (speedValue.isDouble())
        {
            return PlatformControlProtocol::replaySpeedFromString(QString::number(speedValue.toInt()));
        }

        return PlatformControlProtocol::replaySpeedFromString(speedValue.toString());
    }

    [[nodiscard]] std::expected<TradingMode, QString> parseRequiredTradingMode(const QJsonObject& p_arguments)
    {
        const QString rawValue = p_arguments.value("mode").toString().trimmed().toLower();
        if (rawValue.isEmpty())
        {
            return std::unexpected("Missing required 'mode' argument (supported values: sim, live)");
        }

        if (rawValue == "sim")
        {
            return TradingMode::Sim;
        }

        if (rawValue == "live")
        {
            return TradingMode::Live;
        }

        return std::unexpected(QString("Unsupported trading mode '%1' (supported values: sim, live)").arg(rawValue));
    }
} // namespace

// Get the current application time (real or replay)
QDateTime MainApp::getCurrentAppTime()
{
    if (isInReplayMode())
    {
        return currentAppReplayTime;
    }
    return QDateTime::currentDateTime().toTimeZone(TradingHours::MARKET_TIMEZONE);
}

TradingMode MainApp::getTradingMode()
{
    return m_tradingMode;
}

void MainApp::setTradingMode(TradingMode p_mode)
{
    m_tradingMode = p_mode;

    Q_CHECK_PTR(appStateSettings);
    appStateSettings->setValue("Trading/Mode", static_cast<int>(p_mode));
    appStateSettings->sync();

    qInfo() << "Trading mode set to" << (p_mode == TradingMode::Sim ? "SIM" : "LIVE") << "(requires restart)";
}

void MainApp::restartApplication()
{
    // Get the executable path
    QString executablePath = QCoreApplication::applicationFilePath();

    // Get command line arguments (excluding the first which is the program name)
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // Remove program name

    // Convert to C-style arrays for execv()
    QByteArrayList argsByteArrays;
    argsByteArrays.reserve(args.size() + 2); // +2 for program name and null terminator

    // Add program name
    argsByteArrays.append(executablePath.toLocal8Bit());

    // Add other arguments
    for (const QString& arg: args)
    {
        argsByteArrays.append(arg.toLocal8Bit());
    }

    // Build argv array (must be null-terminated)
    std::vector<char*> argv;
    argv.reserve(argsByteArrays.size() + 1);

    for (QByteArray& ba: argsByteArrays)
    {
        argv.push_back(ba.data());
    }
    argv.push_back(nullptr); // Null terminator required by execv()

    qInfo() << "Restarting application via execv()";

    // execv() replaces the current process - doesn't return on success
    execv(executablePath.toLocal8Bit().constData(), argv.data());

    // If we reach here, execv() failed
    qCritical() << "execv() failed:" << strerror(errno);
    QCoreApplication::exit(1);
}

TradingSession MainApp::getCurrentSession()
{
    const QDateTime currentDateTime = getCurrentAppTime();
    const QDate currentDate = currentDateTime.date();
    const Qt::DayOfWeek day = static_cast<Qt::DayOfWeek>(currentDate.dayOfWeek());

    if (day == Qt::Saturday || day == Qt::Sunday)
    {
        return TradingSession::Weekend;
    }

    // Full-day market holiday
    if (MarketCalendar::isHoliday(currentDate))
    {
        return TradingSession::Holiday;
    }

    const QTime currentTime = currentDateTime.time();
    const bool earlyClose = MarketCalendar::isEarlyCloseDay(currentDate);

    // Check each session in order
    if (currentTime >= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION &&
        currentTime <= TradingHours::TIME_LAST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        return TradingSession::EarlyPreMarket;
    }

    if (currentTime >= TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION &&
        currentTime <= TradingHours::TIME_LAST_CANDLE_PRE_MARKET_SESSION)
    {
        return TradingSession::PreMarket;
    }

    // On early-close days the regular session ends at 1:00 PM ET
    const QTime regularEnd =
        earlyClose ? MarketCalendar::EARLY_CLOSE_TIME : TradingHours::TIME_LAST_CANDLE_REGULAR_SESSION;
    if (currentTime >= TradingHours::TIME_FIRST_CANDLE_REGULAR_SESSION && currentTime < regularEnd)
    {
        return TradingSession::Regular;
    }

    // After-hours is suppressed entirely on early-close days (market fully closed after 1 PM)
    if (!earlyClose && currentTime >= TradingHours::TIME_FIRST_CANDLE_AFTER_MARKET_SESSION &&
        currentTime <= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
    {
        return TradingSession::AfterHours;
    }

    return TradingSession::Closed;
}

MainApp::MainApp() : tradeStationClient(TSClient::getInstance()), mainAlgo(MainAlgo::getInstance())
{
    QThread::currentThread()->setObjectName("GUI/Main Thread");

#ifdef GUI_ENABLED
    appFrontend = new GUIFrontend(mainAlgo);
#else
    appFrontend = new TUIFrontend(mainAlgo);
#endif
    // Connect memory usage updates to frontend
    QObject::connect(&memoryMonitor, &MemoryMonitor::memoryUsageUpdated, appFrontend, &FrontEnd::onMemoryUsageUpdate);

    // Connect TradeStation authentication state changes to frontend
    // When the client thread starts and the event loop kicks, there will be an initial
    // emition to signal what is the initial state
    QObject::connect(tradeStationClient,
                     &TSClient::authStateChanged,
                     appFrontend,
                     &FrontEnd::tradeStationAuthStateChanged);

    QObject::connect(tradeStationClient,
                     &TSClient::authStateChanged,
                     mainAlgo,
                     &MainAlgo::onTradeStationAuthStateChanged);

    QObject::connect(mainAlgo,
                     &MainAlgo::tradeStationAccountsReceived,
                     appFrontend,
                     &FrontEnd::onTradeStationAccountsReceived);

    // Connect TradeStation data usage updates to frontend
    QObject::connect(tradeStationClient,
                     &TSClient::totalDataReceivedBytesIncreased,
                     appFrontend,
                     &FrontEnd::onTSClientDataUsageUpdate);

    QObject::connect(mainAlgo,
                     &MainAlgo::replayStarted,
                     appFrontend,
                     [this]() { appFrontend->onReplayPlaybackStateChanged(Playback::State::Playing); });
    QObject::connect(mainAlgo,
                     &MainAlgo::replayResumed,
                     appFrontend,
                     [this]() { appFrontend->onReplayPlaybackStateChanged(Playback::State::Playing); });
    QObject::connect(mainAlgo,
                     &MainAlgo::replayPaused,
                     appFrontend,
                     [this]() { appFrontend->onReplayPlaybackStateChanged(Playback::State::Paused); });
    QObject::connect(mainAlgo,
                     &MainAlgo::replayStopped,
                     appFrontend,
                     [this]() { appFrontend->onReplayPlaybackStateChanged(Playback::State::Stopped); });

    // High-frequency market data (bars, L2, trades, aggregator bars, replay time)
    // is now pulled by GUIFrontend at 30 Hz from DisplaySnapshot — no cross-thread signals needed.

    QObject::connect(mainAlgo, &MainAlgo::receivedNewPosition, appFrontend, &FrontEnd::onNewPositionReceived);

    QObject::connect(mainAlgo, &MainAlgo::positionDeleted, appFrontend, &FrontEnd::onPositionDeleted);

    QObject::connect(mainAlgo, &MainAlgo::receivedNewOrder, appFrontend, &FrontEnd::onNewOrderReceived);

    QObject::connect(mainAlgo, &MainAlgo::balanceUpdated, appFrontend, &FrontEnd::onBalanceUpdated);
}

MainApp::~MainApp()
{
    qInfo() << "MainApp destructor - cleaning up";

    // Stop memory monitoring first to avoid cross-thread timer warnings
    memoryMonitor.stopMonitoring();

    if (m_platformControlServer)
    {
        m_platformControlServer.reset();
    }

    // Delete the frontend first
    delete appFrontend;
    appFrontend = nullptr;

    // The singletons will be cleaned up by their own destructors
}

void MainApp::start()
{
    // Start the database thread first (other threads may depend on it)
    DatabaseThread::getInstance()->start();

    // start the other threads
    tradeStationClient->start();
    mainAlgo->start();

    // Auto-connect to Databento if API key is present
    auto* dbClient = DBClient::getInstance();
    dbClient->start();

    // Connect Databento data usage updates to frontend
    QObject::connect(dbClient, &DBClient::dataUsageUpdated, appFrontend, &FrontEnd::onDBClientDataUsageUpdate);

    if (dbClient->hasApiKey())
    {
        dbClient->connectLive();
    }

    if (m_platformControlServer == nullptr)
    {
        m_platformControlServer = std::make_unique<PlatformControlServer>(this);
        if (!m_platformControlServer->startListening())
        {
            qCritical() << "Platform control socket failed to start;"
                           " l2trader-mcp-server will be unavailable";
        }
    }

    memoryMonitor.startMonitoring(500);

#ifndef GUI_ENABLED
    // Initialize TUI after everything is set up
    static_cast<TUIFrontend*>(appFrontend)->initialize();
#endif
}

void MainApp::shutdown()
{
    qInfo() << "MainApp shutdown initiated - stopping all threads and cleaning up";

    // Stop memory monitoring first
    memoryMonitor.stopMonitoring();
    qInfo() << "Memory monitor stopped";

    // Stop threads in reverse order of startup
    // MainAlgo should stop before TSClient since it depends on it
    if (mainAlgo)
    {
        // stopBalancePolling must be called on MainAlgo's thread
        QMetaObject::invokeMethod(mainAlgo, &MainAlgo::stopBalancePolling, Qt::BlockingQueuedConnection);
        qInfo() << "MainAlgo balance polling stopped";
    }

    // Now quit the application - destructors will be called automatically
    // when the MainApp object goes out of scope in main()
    QCoreApplication::quit();
}

void MainApp::cleanupSingletons()
{
    qInfo() << "Cleaning up singletons";

    // Set shutdown flag to prevent Stream destructors from finishing promises
    Stream::setShuttingDown(true);

    // Delete MainApp singleton first (which will delete the frontend)
    MainApp::destroyInstance();

    // Delete MainAlgo singleton (which will stop its thread)
    MainAlgo::destroyInstance();

    // Delete TSClient singleton (which will stop its thread)
    TSClient::destroyInstance();

    // Delete DBClient singleton
    if (DBClient::isInstantiated())
    {
        DBClient::destroyInstance();
    }

    // Delete DatabaseThread singleton (which will stop its thread)
    DatabaseThread::destroyInstance();

    qInfo() << "All singletons cleaned up";
}

void MainApp::enterReplayMode(QDate p_date, QTime p_startTime, Playback::Speed p_speed)
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Live && "enterReplayMode called when already in replay mode");

    persistReplayConfiguration(p_date, p_startTime, p_speed);
    appFrontend->onReplayConfigurationChanged(p_date, p_startTime, p_speed);

    qInfo() << "Entering replay mode for" << p_date.toString(Qt::ISODate) << "at" << p_startTime.toString("hh:mm:ss");

    // 1. Capture currently displayed symbol before we delete everything
    QString displayedSymbol = mainAlgo->getDisplayedSymbol();
    if (displayedSymbol.isEmpty())
    {
        qWarning() << "No displayed symbol, using default AAPL";
        displayedSymbol = "AAPL";
    }

    // 2. Set data source mode
    m_dataSourceMode = DataSourceMode::Replay;

    // 3. Initialize replay time to start time (will be refined when first bar emits)
    currentAppReplayTime = QDateTime(p_date, p_startTime, TradingHours::MARKET_TIMEZONE);

    // 4. Switch TSClient to replay mode (blocking to ensure mode is set before streams open)
    QMetaObject::invokeMethod(
        tradeStationClient,
        [this]() { tradeStationClient->setMode(TSClient::Mode::Replay); },
        Qt::BlockingQueuedConnection);

    // 5. Destroy and recreate database singletons to pick up new timestamped replay database path
    OrdersDatabase::destroyInstance();
    PositionsDatabase::destroyInstance();

    // 6. Clean slate: stop everything and recreate fresh, then start replay paused (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, displayedSymbol, p_date, p_startTime, p_speed]()
        {
            // Stop all running strategies
            mainAlgo->stopAllStrategies();

            // Close positions/orders streams
            mainAlgo->pauseLiveStreams();

            // Delete all stock instruments (and their streams)
            mainAlgo->deleteAllSymbolContext();

            // Create fresh stock instrument with mock-backed streams
            mainAlgo->createAndSetDisplayedSymbolContext(displayedSymbol);

            // Start replay in paused state - emits first bar to populate chart
            mainAlgo->enterReplayModePaused(displayedSymbol, p_date, p_startTime, p_speed);
        },
        Qt::QueuedConnection);

    // 7. Update UI
    appFrontend->onReplayModeEntered();

    qInfo() << "Replay mode entered with chart pre-populated";
}

void MainApp::exitReplayMode()
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "exitReplayMode called when not in replay mode");

    qInfo() << "Exiting replay mode";

    // 1. Capture currently displayed symbol before we delete everything
    QString displayedSymbol = mainAlgo->getDisplayedSymbol();
    if (displayedSymbol.isEmpty())
    {
        qWarning() << "No displayed symbol, using default AAPL";
        displayedSymbol = "AAPL";
    }

    // 2. Tell MainAlgo to stop replay and clean up (blocking to ensure clean stop)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this]()
        {
            // Stop replay engine if running
            mainAlgo->exitReplayMode();

            // Stop all replay strategies
            mainAlgo->stopAllStrategies();

            // Delete all replay stock instruments
            mainAlgo->deleteAllSymbolContext();
        },
        Qt::BlockingQueuedConnection);

    // 3. Reset data source mode
    m_dataSourceMode = DataSourceMode::Live;

    // 4. Switch TSClient back to live mode (blocking)
    QMetaObject::invokeMethod(
        tradeStationClient,
        [this]() { tradeStationClient->setMode(TSClient::Mode::Live); },
        Qt::BlockingQueuedConnection);

    // 5. Destroy and recreate database singletons to switch back to Live/Sim database
    OrdersDatabase::destroyInstance();
    PositionsDatabase::destroyInstance();

    // 6. Recreate fresh live instruments and resume streams (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, displayedSymbol]()
        {
            // Reopen positions/orders streams
            mainAlgo->resumeLiveStreams();

            // Create fresh stock instrument with live streams
            mainAlgo->createAndSetDisplayedSymbolContext(displayedSymbol);
        },
        Qt::QueuedConnection);

    // 6. Update UI
    appFrontend->onReplayModeExited();

    qInfo() << "Replay mode exited, live mode resumed";
}

void MainApp::startReplayPlayback(QDate p_date, QTime p_startTime, Playback::Speed p_speed)
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "startReplayPlayback called when not in replay mode");

    persistReplayConfiguration(p_date, p_startTime, p_speed);
    appFrontend->onReplayConfigurationChanged(p_date, p_startTime, p_speed);

    qInfo() << "Starting replay playback for" << p_date.toString(Qt::ISODate) << "at"
            << p_startTime.toString("hh:mm:ss");

    // Tell MainAlgo to start replay (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, p_date, p_startTime, p_speed]()
        {
            QString symbol = mainAlgo->getDisplayedSymbol();
            mainAlgo->enterReplayMode(symbol, p_date, p_startTime, p_speed);
        },
        Qt::QueuedConnection);

    qInfo() << "Replay playback start initiated";
}

void MainApp::pauseReplayPlayback()
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "pauseReplayPlayback called when not in replay mode");

    qInfo() << "Pausing replay playback";

    QMetaObject::invokeMethod(mainAlgo, [this]() { mainAlgo->pauseReplay(); }, Qt::QueuedConnection);
}

void MainApp::resumeReplayPlayback()
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "resumeReplayPlayback called when not in replay mode");

    qInfo() << "Resuming replay playback";

    QMetaObject::invokeMethod(mainAlgo, [this]() { mainAlgo->resumeReplay(); }, Qt::QueuedConnection);
}

void MainApp::setReplaySpeed(Playback::Speed p_speed)
{
    persistReplayConfiguration(configuredReplayDate(), configuredReplayStartTime(), p_speed);
    appFrontend->onReplayConfigurationChanged(configuredReplayDate(), configuredReplayStartTime(), p_speed);
    QMetaObject::invokeMethod(mainAlgo, [this, p_speed]() { mainAlgo->setReplaySpeed(p_speed); }, Qt::QueuedConnection);
}

bool MainApp::isReplayPaused() const
{
    return mainAlgo->getReplayState() == Playback::State::Paused;
}

QJsonObject MainApp::getControlStatus() const
{
    QJsonObject status;
    const QDate replayDate = configuredReplayDate();
    const QTime replayStartTime = configuredReplayStartTime();
    QString displayedSymbol;
    QString activeAccountId;
    Playback::State replayState = Playback::State::Stopped;
    const bool invoked = QMetaObject::invokeMethod(
        mainAlgo,
        [&]()
        {
            displayedSymbol = this->mainAlgo->getDisplayedSymbol();
            activeAccountId = this->mainAlgo->getActiveAccountId();
            replayState = this->mainAlgo->getReplayState();
        },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);

    status["dataSourceMode"] = dataSourceModeToString(m_dataSourceMode);
    status["tradingMode"] = tradingModeToString(m_tradingMode);
    status["tradingModeChangeRequiresRestart"] = true;
    status["displayedSymbol"] = displayedSymbol;
    status["activeAccountId"] = activeAccountId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(activeAccountId);
    status["replayState"] = replayStateToString(replayState);
    status["replayPaused"] = isReplayPaused();
    status["currentAppTime"] = getCurrentAppTime().toString(Qt::ISODateWithMs);
    status["currentReplayTime"] =
        isInReplayMode() ? QJsonValue(currentAppReplayTime.toString(Qt::ISODateWithMs)) : QJsonValue(QJsonValue::Null);
    status["controlSocketPath"] = PlatformControlProtocol::socketPath();
    status["supportedReplaySpeeds"] = PlatformControlProtocol::supportedReplaySpeedsJson();
    status["configuredReplayDate"] =
        replayDate.isValid() ? QJsonValue(replayDate.toString(Qt::ISODate)) : QJsonValue(QJsonValue::Null);
    status["configuredReplayStartTime"] =
        replayStartTime.isValid() ? QJsonValue(replayStartTime.toString(Qt::ISODate)) : QJsonValue(QJsonValue::Null);
    status["configuredReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(configuredReplaySpeed());
    return status;
}

QJsonObject MainApp::handleControlRequest(const QJsonObject& p_request)
{
    const int protocolVersion = p_request.value("protocolVersion").toInt(-1);
    if (protocolVersion != PlatformControlProtocol::kProtocolVersion)
    {
        return makeControlResponse(false,
                                   "Platform control request rejected.",
                                   {},
                                   QString("Unsupported control protocol version %1 (expected %2)")
                                       .arg(protocolVersion)
                                       .arg(PlatformControlProtocol::kProtocolVersion));
    }

    const QString command = p_request.value("command").toString().trimmed();
    if (command.isEmpty())
    {
        return makeControlResponse(false, "Platform control request rejected.", {}, "Missing required 'command' field");
    }

    const QJsonObject arguments = p_request.value("arguments").toObject();

    if (command == PlatformControlProtocol::kCommandStatus)
    {
        return makeControlResponse(true, "Platform status retrieved.", getControlStatus());
    }

    if (command == PlatformControlProtocol::kCommandGetDisplayedSymbol)
    {
        QString displayedSymbol;
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&displayedSymbol, this]() { displayedSymbol = this->mainAlgo->getDisplayedSymbol(); },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);

        QJsonObject result;
        result["displayedSymbol"] =
            displayedSymbol.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(displayedSymbol);
        return makeControlResponse(true,
                                   displayedSymbol.isEmpty() ? "No symbol is currently displayed."
                                                             : "Displayed symbol retrieved.",
                                   result);
    }

    if (command == PlatformControlProtocol::kCommandGetAccounts)
    {
        QString activeAccountId;
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&activeAccountId, this]() { activeAccountId = this->mainAlgo->getActiveAccountId(); },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);

        const auto accountsResult = waitForFutureResult(TSClient::getInstance()->getAccounts());
        if (!accountsResult.has_value())
        {
            return makeControlResponse(
                false,
                "Accounts request failed.",
                {},
                QString("Account lookup failed: %1").arg(tsClientErrorToString(accountsResult.error())));
        }

        QJsonObject result;
        result["activeAccountId"] =
            activeAccountId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(activeAccountId);
        result["accountCount"] = accountsResult->size();
        result["accounts"] = serializeAccounts(accountsResult.value(), activeAccountId);
        return makeControlResponse(true,
                                   accountsResult->isEmpty() ? "No accounts are currently available."
                                                             : "Accounts retrieved.",
                                   result);
    }

    if (command == PlatformControlProtocol::kCommandGetBalance)
    {
        const auto parsedAccountId = parseOptionalStringForKey(arguments, "accountId");
        if (!parsedAccountId.has_value())
        {
            return makeControlResponse(false, "Balance request rejected.", {}, parsedAccountId.error());
        }

        QString activeAccountId;
        Balance currentBalance;
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&]()
            {
                activeAccountId = this->mainAlgo->getActiveAccountId();
                currentBalance = this->mainAlgo->getCurrentBalance();
            },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);

        QString resolvedAccountId = parsedAccountId.value();
        if (resolvedAccountId.isEmpty())
        {
            resolvedAccountId = activeAccountId;
        }
        if (resolvedAccountId.isEmpty())
        {
            return makeControlResponse(false,
                                       "Balance request rejected.",
                                       {},
                                       "No accountId was provided and the platform has no active account selected");
        }

        bool usedCachedActiveBalance = false;
        Balance resolvedBalance;
        if (!currentBalance.getAccountID().isEmpty() && currentBalance.getAccountID() == resolvedAccountId)
        {
            resolvedBalance = currentBalance;
            usedCachedActiveBalance = true;
        }
        else
        {
            const auto balancesResult =
                waitForFutureResult(TSClient::getInstance()->getBalances(QStringList{resolvedAccountId}));
            if (!balancesResult.has_value())
            {
                return makeControlResponse(
                    false,
                    "Balance request failed.",
                    {},
                    QString("Balance lookup failed: %1").arg(tsClientErrorToString(balancesResult.error())));
            }
            if (balancesResult->isEmpty())
            {
                return makeControlResponse(
                    false,
                    "Balance request failed.",
                    {},
                    QString("No balance was returned for accountId '%1'").arg(resolvedAccountId));
            }

            resolvedBalance = balancesResult->first();
        }

        QJsonObject result;
        result["requestedAccountId"] =
            parsedAccountId->isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(parsedAccountId.value());
        result["activeAccountId"] =
            activeAccountId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(activeAccountId);
        result["usedCachedActiveBalance"] = usedCachedActiveBalance;
        result["balance"] = serializeBalance(resolvedBalance);
        return makeControlResponse(true, "Balance retrieved.", result);
    }

    if (command == PlatformControlProtocol::kCommandGetPositions)
    {
        const auto parsedAccountId = parseOptionalStringForKey(arguments, "accountId");
        if (!parsedAccountId.has_value())
        {
            return makeControlResponse(false, "Positions request rejected.", {}, parsedAccountId.error());
        }

        const auto parsedSymbol = parseOptionalSymbol(arguments);
        if (!parsedSymbol.has_value())
        {
            return makeControlResponse(false, "Positions request rejected.", {}, parsedSymbol.error());
        }

        QString activeAccountId;
        QVector<Position> positions;
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&]()
            {
                activeAccountId = this->mainAlgo->getActiveAccountId();
                positions = this->mainAlgo->getCurrentPositionsSnapshot();
            },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);

        const QString resolvedAccountId = parsedAccountId->isEmpty() ? activeAccountId : parsedAccountId.value();
        QVector<Position> filteredPositions;
        filteredPositions.reserve(positions.size());
        for (const Position& position: positions)
        {
            const QString quantity = position.getQuantity().trimmed();
            if (position.isDeleted() || quantity == "0" || quantity == "0.0" || quantity == "0.00")
            {
                continue;
            }
            if (!resolvedAccountId.isEmpty() && position.getAccountID() != resolvedAccountId)
            {
                continue;
            }
            if (!parsedSymbol->isEmpty() && position.getSymbol() != parsedSymbol.value())
            {
                continue;
            }
            filteredPositions.append(position);
        }

        std::sort(filteredPositions.begin(),
                  filteredPositions.end(),
                  [](const Position& lhs, const Position& rhs)
                  {
                      if (lhs.getSymbol() == rhs.getSymbol())
                      {
                          return lhs.getPositionID() < rhs.getPositionID();
                      }
                      return lhs.getSymbol() < rhs.getSymbol();
                  });

        QJsonObject result;
        result["requestedAccountId"] =
            parsedAccountId->isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(parsedAccountId.value());
        result["accountId"] =
            resolvedAccountId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(resolvedAccountId);
        result["activeAccountId"] =
            activeAccountId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(activeAccountId);
        result["symbol"] = parsedSymbol->isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(parsedSymbol.value());
        result["positionCount"] = filteredPositions.size();
        result["positions"] = serializePositions(filteredPositions);
        return makeControlResponse(true,
                                   filteredPositions.isEmpty() ? "No matching positions found."
                                                               : "Positions retrieved.",
                                   result);
    }

    if (command == PlatformControlProtocol::kCommandGetOrders)
    {
        const auto parsedAccountId = parseOptionalStringForKey(arguments, "accountId");
        if (!parsedAccountId.has_value())
        {
            return makeControlResponse(false, "Orders request rejected.", {}, parsedAccountId.error());
        }

        const auto parsedSymbol = parseOptionalSymbol(arguments);
        if (!parsedSymbol.has_value())
        {
            return makeControlResponse(false, "Orders request rejected.", {}, parsedSymbol.error());
        }

        const auto parsedStatus = parseOptionalOrderStatus(arguments);
        if (!parsedStatus.has_value())
        {
            return makeControlResponse(false, "Orders request rejected.", {}, parsedStatus.error());
        }

        const auto parsedMaxCount = parseOptionalMaxCount(arguments);
        if (!parsedMaxCount.has_value())
        {
            return makeControlResponse(false, "Orders request rejected.", {}, parsedMaxCount.error());
        }

        QString activeAccountId;
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&activeAccountId, this]() { activeAccountId = this->mainAlgo->getActiveAccountId(); },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);

        OrdersDatabase* ordersDb = OrdersDatabase::getInstance();
        ASSUME_DIFF(ordersDb, nullptr);
        const auto ordersById = ordersDb->loadAllOrders();

        const QString resolvedAccountId = parsedAccountId->isEmpty() ? activeAccountId : parsedAccountId.value();
        QVector<std::tuple<Order, std::optional<qint64>>> filteredOrders;
        filteredOrders.reserve(ordersById.size());
        for (auto it = ordersById.constBegin(); it != ordersById.constEnd(); ++it)
        {
            const Order& order = std::get<Order>(it.value());
            const std::optional<qint64>& latencyMs = std::get<std::optional<qint64>>(it.value());

            if (!resolvedAccountId.isEmpty() && order.getAccountID() != resolvedAccountId)
            {
                continue;
            }
            if (!parsedSymbol->isEmpty() && order.getSymbol() != parsedSymbol.value())
            {
                continue;
            }
            if (parsedStatus.value().has_value() && order.getOrderStatus() != parsedStatus.value().value())
            {
                continue;
            }

            filteredOrders.append(std::make_tuple(order, latencyMs));
        }

        std::sort(filteredOrders.begin(),
                  filteredOrders.end(),
                  [](const auto& lhs, const auto& rhs)
                  {
                      const Order& leftOrder = std::get<Order>(lhs);
                      const Order& rightOrder = std::get<Order>(rhs);
                      if (leftOrder.getOpenedDateTime() == rightOrder.getOpenedDateTime())
                      {
                          return leftOrder.getOrderID() > rightOrder.getOrderID();
                      }
                      return leftOrder.getOpenedDateTime() > rightOrder.getOpenedDateTime();
                  });

        const qsizetype limitedCount = std::min(filteredOrders.size(), static_cast<qsizetype>(parsedMaxCount.value()));
        QJsonArray serializedOrders;
        for (qsizetype index = 0; index < limitedCount; ++index)
        {
            const auto& [order, latencyMs] = filteredOrders.at(index);
            serializedOrders.append(serializeOrder(order, latencyMs));
        }

        QJsonObject result;
        result["requestedAccountId"] =
            parsedAccountId->isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(parsedAccountId.value());
        result["accountId"] =
            resolvedAccountId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(resolvedAccountId);
        result["activeAccountId"] =
            activeAccountId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(activeAccountId);
        result["symbol"] = parsedSymbol->isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(parsedSymbol.value());
        result["status"] = parsedStatus.value().has_value() ? QJsonValue(QtEnum::toString(parsedStatus.value().value()))
                                                            : QJsonValue(QJsonValue::Null);
        result["maxCount"] = parsedMaxCount.value();
        result["orderCount"] = limitedCount;
        result["totalMatches"] = filteredOrders.size();
        result["orders"] = serializedOrders;
        return makeControlResponse(true,
                                   filteredOrders.isEmpty() ? "No matching orders found." : "Orders retrieved.",
                                   result);
    }

    if (command == PlatformControlProtocol::kCommandGetLevel2)
    {
        const auto parsedSymbol = parseOptionalSymbol(arguments);
        if (!parsedSymbol.has_value())
        {
            return makeControlResponse(false, "Level 2 snapshot request rejected.", {}, parsedSymbol.error());
        }

        std::expected<MainAlgo::MarketDataSnapshot, QString> snapshot =
            std::unexpected(QStringLiteral("Level 2 snapshot request did not run"));
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&snapshot, this, symbol = parsedSymbol.value()]()
            { snapshot = this->mainAlgo->getMarketDataSnapshot(symbol, 1); },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);
        if (!snapshot.has_value())
        {
            return makeControlResponse(false, "Level 2 snapshot request rejected.", {}, snapshot.error());
        }

        QJsonObject result;
        result["symbol"] = snapshot->symbol;
        result["replayTime"] = optionalDateTimeToJsonValue(snapshot->replayTime);
        result["level2"] = snapshot->latestLevel2.has_value()
                               ? QJsonValue(serializeLevel2(snapshot->latestLevel2.value()))
                               : QJsonValue(QJsonValue::Null);
        return makeControlResponse(true, "Level 2 snapshot retrieved.", result);
    }

    if (command == PlatformControlProtocol::kCommandGetTradesSnapshot)
    {
        const auto parsedSymbol = parseOptionalSymbol(arguments);
        if (!parsedSymbol.has_value())
        {
            return makeControlResponse(false, "Trade snapshot request rejected.", {}, parsedSymbol.error());
        }

        const auto parsedMaxCount = parseOptionalMaxCount(arguments);
        if (!parsedMaxCount.has_value())
        {
            return makeControlResponse(false, "Trade snapshot request rejected.", {}, parsedMaxCount.error());
        }

        std::expected<MainAlgo::MarketDataSnapshot, QString> snapshot =
            std::unexpected(QStringLiteral("Trade snapshot request did not run"));
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&snapshot, this, symbol = parsedSymbol.value(), maxCount = parsedMaxCount.value()]()
            { snapshot = this->mainAlgo->getMarketDataSnapshot(symbol, maxCount); },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);
        if (!snapshot.has_value())
        {
            return makeControlResponse(false, "Trade snapshot request rejected.", {}, snapshot.error());
        }

        QJsonObject result;
        result["symbol"] = snapshot->symbol;
        result["maxCount"] = parsedMaxCount.value();
        result["tradeCount"] = snapshot->recentTrades.size();
        result["replayTime"] = optionalDateTimeToJsonValue(snapshot->replayTime);
        result["trades"] = serializeTrades(snapshot->recentTrades);
        return makeControlResponse(true, "Trade snapshot retrieved.", result);
    }

    if (command == PlatformControlProtocol::kCommandGetActivityMetrics)
    {
        const auto parsedSymbol = parseOptionalSymbol(arguments);
        if (!parsedSymbol.has_value())
        {
            return makeControlResponse(false, "Activity metrics request rejected.", {}, parsedSymbol.error());
        }

        std::expected<MainAlgo::MarketDataSnapshot, QString> snapshot =
            std::unexpected(QStringLiteral("Activity metrics request did not run"));
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&snapshot, this, symbol = parsedSymbol.value()]()
            { snapshot = this->mainAlgo->getMarketDataSnapshot(symbol, 1); },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);
        if (!snapshot.has_value())
        {
            return makeControlResponse(false, "Activity metrics request rejected.", {}, snapshot.error());
        }

        QJsonObject result;
        result["symbol"] = snapshot->symbol;
        result["activity"] = serializeActivityMetrics(snapshot->activity);
        return makeControlResponse(true, "Activity metrics retrieved.", result);
    }

    if (command == PlatformControlProtocol::kCommandGetBars)
    {
        const auto parsedSymbol = parseOptionalSymbol(arguments);
        if (!parsedSymbol.has_value())
        {
            return makeControlResponse(false, "Historical bar request rejected.", {}, parsedSymbol.error());
        }

        const auto date = parseRequiredDate(arguments);
        if (!date.has_value())
        {
            return makeControlResponse(false, "Historical bar request rejected.", {}, date.error());
        }

        const auto startTime = parseRequiredTime(arguments);
        if (!startTime.has_value())
        {
            return makeControlResponse(false, "Historical bar request rejected.", {}, startTime.error());
        }

        const auto endTime = parseRequiredTimeForKey(arguments, "endTime", "HH:MM[:SS]");
        if (!endTime.has_value())
        {
            return makeControlResponse(false, "Historical bar request rejected.", {}, endTime.error());
        }

        if (startTime.value() > endTime.value())
        {
            return makeControlResponse(false,
                                       "Historical bar request rejected.",
                                       {},
                                       "Historical bar request startTime must be before or equal to endTime");
        }

        const auto timeFrame = parseOptionalTimeFrame(arguments);
        if (!timeFrame.has_value())
        {
            return makeControlResponse(false, "Historical bar request rejected.", {}, timeFrame.error());
        }

        QString resolvedSymbol;
        QString requestError;
        BarCache::GetBarsResult_t barsResult;
        const bool invoked = QMetaObject::invokeMethod(
            mainAlgo,
            [&]()
            {
                resolvedSymbol = parsedSymbol.value();
                if (resolvedSymbol.isEmpty())
                {
                    resolvedSymbol = this->mainAlgo->getDisplayedSymbol();
                }
                if (resolvedSymbol.isEmpty())
                {
                    requestError = "No symbol was provided and no symbol is currently displayed";
                    return;
                }

                barsResult = this->mainAlgo->requestHistoricalBarsForSymbol(resolvedSymbol,
                                                                            date.value(),
                                                                            startTime.value(),
                                                                            endTime.value(),
                                                                            timeFrame.value());
            },
            Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);

        if (!requestError.isEmpty())
        {
            return makeControlResponse(false, "Historical bar request rejected.", {}, requestError);
        }

        std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error> resolvedBars =
            std::unexpected(TSClient::Error::Other);
        if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(barsResult))
        {
            resolvedBars = std::get<std::shared_ptr<QVector<Bar>>>(barsResult);
        }
        else
        {
            resolvedBars = waitForFutureResult(
                std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(barsResult));
        }

        if (!resolvedBars.has_value())
        {
            return makeControlResponse(
                false,
                "Historical bar request rejected.",
                {},
                QString("Historical bar lookup failed: %1").arg(tsClientErrorToString(resolvedBars.error())));
        }

        ASSUME_DIFF(resolvedBars.value().get(), nullptr);
        QJsonObject result;
        result["symbol"] = resolvedSymbol;
        result["date"] = date->toString(Qt::ISODate);
        result["startTime"] = startTime->toString(Qt::ISODate);
        result["endTime"] = endTime->toString(Qt::ISODate);
        result["timeFrame"] = timeFrameToString(timeFrame.value());
        result["barCount"] = resolvedBars.value()->size();
        result["bars"] = serializeBars(*resolvedBars.value());
        return makeControlResponse(true, "Historical bars retrieved.", result);
    }

    if (command == PlatformControlProtocol::kCommandPlaceOrder)
    {
        const auto parsedAccountId = parseOptionalStringForKey(arguments, "accountId");
        if (!parsedAccountId.has_value())
        {
            return makeControlResponse(false, "Order placement request rejected.", {}, parsedAccountId.error());
        }

        const auto parsedSymbol = parseRequiredStringForKey(arguments, "symbol");
        if (!parsedSymbol.has_value())
        {
            return makeControlResponse(false, "Order placement request rejected.", {}, parsedSymbol.error());
        }

        const auto parsedTradeAction = parseRequiredTradeAction(arguments);
        if (!parsedTradeAction.has_value())
        {
            return makeControlResponse(false, "Order placement request rejected.", {}, parsedTradeAction.error());
        }

        const auto parsedOrderType = parseRequiredOrderType(arguments);
        if (!parsedOrderType.has_value())
        {
            return makeControlResponse(false, "Order placement request rejected.", {}, parsedOrderType.error());
        }

        const auto parsedQuantity = parseRequiredPositiveIntForKey(arguments, "quantity");
        if (!parsedQuantity.has_value())
        {
            return makeControlResponse(false, "Order placement request rejected.", {}, parsedQuantity.error());
        }

        const auto parsedDuration = parseOptionalOrderDuration(arguments);
        if (!parsedDuration.has_value())
        {
            return makeControlResponse(false, "Order placement request rejected.", {}, parsedDuration.error());
        }

        const auto parsedLimitPrice = parseOptionalPositiveDoubleForKey(arguments, "limitPrice");
        if (!parsedLimitPrice.has_value())
        {
            return makeControlResponse(false, "Order placement request rejected.", {}, parsedLimitPrice.error());
        }

        const auto parsedStopPrice = parseOptionalPositiveDoubleForKey(arguments, "stopPrice");
        if (!parsedStopPrice.has_value())
        {
            return makeControlResponse(false, "Order placement request rejected.", {}, parsedStopPrice.error());
        }

        QString resolvedAccountId = parsedAccountId.value();
        if (resolvedAccountId.isEmpty())
        {
            const bool invoked = QMetaObject::invokeMethod(
                mainAlgo,
                [&resolvedAccountId, this]() { resolvedAccountId = this->mainAlgo->getActiveAccountId(); },
                Qt::BlockingQueuedConnection);
            ASSUME_TRUE(invoked);
        }

        if (resolvedAccountId.isEmpty())
        {
            return makeControlResponse(false,
                                       "Order placement request rejected.",
                                       {},
                                       "No accountId was provided and the platform has no active account selected");
        }

        PlaceOrderRequest orderRequest;
        orderRequest.setAccountID(resolvedAccountId);
        orderRequest.setSymbol(parsedSymbol->trimmed().toUpper());
        orderRequest.setTradeAction(parsedTradeAction.value());
        orderRequest.setOrderType(parsedOrderType.value());
        orderRequest.setQuantity(parsedQuantity.value());
        orderRequest.setTimeInForce(TimeInForce(parsedDuration.value()));
        if (parsedLimitPrice.value().has_value())
        {
            orderRequest.setLimitPrice(parsedLimitPrice.value().value());
        }
        if (parsedStopPrice.value().has_value())
        {
            orderRequest.setStopPrice(parsedStopPrice.value().value());
        }

        if (!orderRequest.isValid())
        {
            return makeControlResponse(false,
                                       "Order placement request rejected.",
                                       {},
                                       "The order request is invalid for the selected orderType/duration combination");
        }

        const auto orderResultFuture = TSClient::getInstance()->placeOrder(orderRequest);
        const auto orderResult = waitForFutureResult(orderResultFuture);
        if (!orderResult.has_value())
        {
            return makeControlResponse(
                false,
                "Order placement request failed.",
                {},
                QString("Order placement failed: %1").arg(tsClientErrorToString(orderResult.error())));
        }

        const QJsonObject result = QJsonObject{
            {"accountId", resolvedAccountId},
            {"symbol", orderRequest.getSymbol()},
            {"tradeAction", tradeActionToJsonString(orderRequest.getTradeAction())},
            {"orderType", OrderType::toString(orderRequest.getOrderType().type)},
            {"quantity", orderRequest.getQuantity()},
            {"duration", orderDurationToJsonString(orderRequest.getTimeInForce().getDuration())},
            {"limitPrice", optionalDoubleToJsonValue(orderRequest.getLimitPrice())},
            {"stopPrice", optionalDoubleToJsonValue(orderRequest.getStopPrice())},
            {"result", serializePlaceOrderResult(orderResult.value())},
        };

        if (orderResult->hasErrors())
        {
            QStringList errors;
            for (const OrderResultItem& item: orderResult->getErrors())
            {
                if (item.getError().has_value())
                {
                    errors.append(item.getError().value());
                }
                else if (!item.getMessage().isEmpty())
                {
                    errors.append(item.getMessage());
                }
            }

            return makeControlResponse(false, "Order placement completed with errors.", result, errors.join(" | "));
        }

        return makeControlResponse(true, "Order placed successfully.", result);
    }

    if (command == PlatformControlProtocol::kCommandCancelOrder)
    {
        const auto parsedOrderId = parseRequiredStringForKey(arguments, "orderId");
        if (!parsedOrderId.has_value())
        {
            return makeControlResponse(false, "Cancel order request rejected.", {}, parsedOrderId.error());
        }

        const auto cancelResultFuture = TSClient::getInstance()->cancelOrder(parsedOrderId.value());
        const auto cancelResult = waitForFutureResult(cancelResultFuture);
        if (!cancelResult.has_value())
        {
            return makeControlResponse(
                false,
                "Cancel order request failed.",
                {},
                QString("Cancel order failed: %1").arg(tsClientErrorToString(cancelResult.error())));
        }

        const QJsonObject result = QJsonObject{
            {"orderId", parsedOrderId.value()},
            {"result", serializeCancelOrderResult(cancelResult.value())},
        };

        if (cancelResult->isError())
        {
            return makeControlResponse(false,
                                       "Cancel order completed with errors.",
                                       result,
                                       cancelResult->getError().value_or(cancelResult->getMessage()));
        }

        return makeControlResponse(true, "Order canceled successfully.", result);
    }

    if (command == PlatformControlProtocol::kCommandEnterReplay)
    {
        if (isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay mode change rejected.",
                                       {},
                                       "Application is already in replay mode");
        }

        const auto date = parseRequiredDate(arguments);
        if (!date.has_value())
        {
            return makeControlResponse(false, "Replay mode change rejected.", {}, date.error());
        }

        const auto startTime = parseRequiredTime(arguments);
        if (!startTime.has_value())
        {
            return makeControlResponse(false, "Replay mode change rejected.", {}, startTime.error());
        }

        const auto speed = parseRequiredReplaySpeed(arguments);
        if (!speed.has_value())
        {
            return makeControlResponse(false, "Replay mode change rejected.", {}, speed.error());
        }

        enterReplayMode(date.value(), startTime.value(), speed.value());
        QJsonObject status = getControlStatus();
        status["requestedReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(speed.value());
        return makeControlResponse(true, "Replay mode entry requested.", status);
    }

    if (command == PlatformControlProtocol::kCommandStartReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay playback request rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        const auto date = parseReplayDateOrConfigured(arguments);
        if (!date.has_value())
        {
            return makeControlResponse(false, "Replay playback request rejected.", {}, date.error());
        }

        const auto startTime = parseReplayStartTimeOrConfigured(arguments);
        if (!startTime.has_value())
        {
            return makeControlResponse(false, "Replay playback request rejected.", {}, startTime.error());
        }

        const auto speed = parseReplaySpeedOrConfigured(arguments);
        if (!speed.has_value())
        {
            return makeControlResponse(false, "Replay playback request rejected.", {}, speed.error());
        }

        startReplayPlayback(date.value(), startTime.value(), speed.value());
        QJsonObject status = getControlStatus();
        status["requestedReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(speed.value());
        return makeControlResponse(true, "Replay playback start requested.", status);
    }

    if (command == PlatformControlProtocol::kCommandPauseReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay pause request rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        if (isReplayPaused())
        {
            return makeControlResponse(true, "Replay is already paused.", getControlStatus());
        }

        pauseReplayPlayback();
        return makeControlResponse(true, "Replay pause requested.", getControlStatus());
    }

    if (command == PlatformControlProtocol::kCommandResumeReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay resume request rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        if (!isReplayPaused())
        {
            return makeControlResponse(true, "Replay is already running.", getControlStatus());
        }

        resumeReplayPlayback();
        return makeControlResponse(true, "Replay resume requested.", getControlStatus());
    }

    if (command == PlatformControlProtocol::kCommandSetReplaySpeed)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay speed update rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        const auto speed = parseRequiredReplaySpeed(arguments);
        if (!speed.has_value())
        {
            return makeControlResponse(false, "Replay speed update rejected.", {}, speed.error());
        }

        setReplaySpeed(speed.value());
        QJsonObject status = getControlStatus();
        status["requestedReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(speed.value());
        return makeControlResponse(true, "Replay speed update requested.", status);
    }

    if (command == PlatformControlProtocol::kCommandPreloadReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay preload request rejected.",
                                       {},
                                       "Application must be in replay mode first");
        }

        const auto date = parseRequiredDate(arguments);
        if (!date.has_value())
        {
            return makeControlResponse(false, "Replay preload request rejected.", {}, date.error());
        }

        const auto startTime = parseRequiredTime(arguments);
        if (!startTime.has_value())
        {
            return makeControlResponse(false, "Replay preload request rejected.", {}, startTime.error());
        }

        const auto speed = parseRequiredReplaySpeed(arguments);
        if (!speed.has_value())
        {
            return makeControlResponse(false, "Replay preload request rejected.", {}, speed.error());
        }

        preloadChartForReplay(date.value(), startTime.value(), speed.value());
        QJsonObject status = getControlStatus();
        status["requestedReplaySpeed"] = PlatformControlProtocol::replaySpeedToString(speed.value());
        return makeControlResponse(true, "Replay chart preload requested.", status);
    }

    if (command == PlatformControlProtocol::kCommandExitReplay)
    {
        if (!isInReplayMode())
        {
            return makeControlResponse(false,
                                       "Replay exit request rejected.",
                                       {},
                                       "Application is not currently in replay mode");
        }

        exitReplayMode();
        return makeControlResponse(true, "Replay mode exit requested.", getControlStatus());
    }

    if (command == PlatformControlProtocol::kCommandSetTradingMode)
    {
        const auto mode = parseRequiredTradingMode(arguments);
        if (!mode.has_value())
        {
            return makeControlResponse(false, "Trading mode update rejected.", {}, mode.error());
        }

        if (m_tradingMode == mode.value())
        {
            QJsonObject status = getControlStatus();
            status["restartRequired"] = false;
            return makeControlResponse(true, "Trading mode already set.", status);
        }

        setTradingMode(mode.value());
#ifdef GUI_ENABLED
        if (auto* const guiFrontend = qobject_cast<GUIFrontend*>(appFrontend))
        {
            guiFrontend->onTradingModeConfigured(mode.value());
        }
#endif
        QJsonObject status = getControlStatus();
        status["restartRequired"] = true;
        status["requestedTradingMode"] = tradingModeToString(mode.value());
        return makeControlResponse(true, "Trading mode updated. Restart required to apply it.", status);
    }

    return makeControlResponse(false,
                               "Platform control request rejected.",
                               {},
                               QString("Unknown control command '%1'").arg(command));
}

void MainApp::preloadChartForReplay(QDate p_date, QTime p_startTime, Playback::Speed p_speed)
{
    ASSUME_TRUE(m_dataSourceMode == DataSourceMode::Replay && "preloadChartForReplay called when not in replay mode");

    persistReplayConfiguration(p_date, p_startTime, p_speed);
    appFrontend->onReplayConfigurationChanged(p_date, p_startTime, p_speed);

    qInfo() << "Preloading chart for replay:" << p_date.toString(Qt::ISODate) << "at"
            << p_startTime.toString("hh:mm:ss");

    // Get currently displayed symbol
    QString displayedSymbol = mainAlgo->getDisplayedSymbol();

    // Reload chart data for new day/time (MainAlgo thread)
    QMetaObject::invokeMethod(
        mainAlgo,
        [this, displayedSymbol, p_date, p_startTime, p_speed]()
        {
            // Re-enter replay paused with new date/time
            // This stops existing replay, reloads data, and emits first bar to update chart
            mainAlgo->enterReplayModePaused(displayedSymbol, p_date, p_startTime, p_speed);
        },
        Qt::QueuedConnection);

    qInfo() << "Chart preload initiated for" << displayedSymbol;
}
