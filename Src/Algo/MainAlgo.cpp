#include <QRunnable>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QSemaphore>
#include <QMetaEnum>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "MainAlgo.h"
#include "MainApp.h"
#include "StrategyManager.h"
#include "TSClient.h"
#include "DBClient.h"
#include "Logging.h"
#include "Assume.h"
#include "BarUtils.h"
#include "DatabaseThread.h"
#include "LTTng/LTTngTracepoints.h"
#include "OrderEmulator.h"
#include "CONSTANTS.h"
#include "OrdersDatabase.h"
#include "Settings.h"
#include "ThreadNames.h"
#include "RiskManager.h"

#define LOGGING_CATEGORY MainAlgoLog

Q_LOGGING_CATEGORY(MainAlgoLog, "MainAlgo")

// Initialize static member outside class
MainAlgo* MainAlgo::m_instance = nullptr;

namespace
{
    constexpr int kMarketDataSnapshotWaitMs = 500;
    constexpr int kMarketDataSnapshotPollMs = 25;
    constexpr int kStrategyViewStatesQueryWaitMs = 25;
    constexpr qint64 kManagedBracketVirtualExitStaleMs = 1500;
    constexpr qint64 kManagedBracketVirtualExitCancelRetryMs = 500;
    constexpr qint64 kManagedBracketVirtualExitCancelTimeoutMs = 4000;

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

    [[nodiscard]] QString tradeActionToText(const TradeAction p_action)
    {
        switch (p_action)
        {
        case TradeAction::Buy:
            return "BUY";
        case TradeAction::Sell:
            return "SELL";
        case TradeAction::BuyToCover:
            return "BUY TO COVER";
        case TradeAction::SellShort:
            return "SELL SHORT";
        case TradeAction::BuyToOpen:
            return "BUY TO OPEN";
        case TradeAction::BuyToClose:
            return "BUY TO CLOSE";
        case TradeAction::SellToOpen:
            return "SELL TO OPEN";
        case TradeAction::SellToClose:
            return "SELL TO CLOSE";
        }

        return "ORDER";
    }

    [[nodiscard]] QString managedBracketPolicyToString(const StrategyBracketExecutionPolicy p_policy)
    {
        switch (p_policy)
        {
        case StrategyBracketExecutionPolicy::Auto:
            return "auto";
        case StrategyBracketExecutionPolicy::VirtualOnly:
            return "virtual-only";
        case StrategyBracketExecutionPolicy::NativeOnly:
            return "native-only";
        }

        return "unknown";
    }

    [[nodiscard]] StrategyBracketOverlayEntry::Side toOverlaySide(const StrategyBracketSide p_side)
    {
        return (p_side == StrategyBracketSide::Short) ? StrategyBracketOverlayEntry::Side::Short
                                                      : StrategyBracketOverlayEntry::Side::Long;
    }

    [[nodiscard]] bool isExtendedHoursSession(const TradingSession p_session)
    {
        return p_session == TradingSession::EarlyPreMarket || p_session == TradingSession::PreMarket ||
               p_session == TradingSession::AfterHours;
    }

    [[nodiscard]] bool isTradableCloseSession(const TradingSession p_session)
    {
        return p_session == TradingSession::Regular || isExtendedHoursSession(p_session);
    }

    [[nodiscard]] bool isWithinSupportedIntradayBarSession(const QDateTime& p_timestamp)
    {
        const QTime time = p_timestamp.time();
        return time >= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION &&
               time <= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;
    }

    [[nodiscard]] QString tradingSessionToString(const TradingSession p_session)
    {
        switch (p_session)
        {
        case TradingSession::EarlyPreMarket:
            return "early-pre-market";
        case TradingSession::PreMarket:
            return "pre-market";
        case TradingSession::Regular:
            return "regular";
        case TradingSession::AfterHours:
            return "after-hours";
        case TradingSession::Closed:
            return "closed";
        case TradingSession::Weekend:
            return "weekend";
        case TradingSession::Holiday:
            return "holiday";
        }

        return "unknown";
    }

    [[nodiscard]] QStringList normalizeClosePositionSymbols(const QStringList& p_symbols)
    {
        QStringList normalizedSymbols;
        for (const QString& rawSymbol: p_symbols)
        {
            const QString symbol = rawSymbol.trimmed().toUpper();
            if (!symbol.isEmpty() && !normalizedSymbols.contains(symbol))
            {
                normalizedSymbols.append(symbol);
            }
        }
        return normalizedSymbols;
    }

    [[nodiscard]] std::expected<int, QString> parsePositionQuantityShares(const Position& p_position)
    {
        bool ok = false;
        const double rawQuantity = p_position.getQuantity().trimmed().toDouble(&ok);
        if (!ok)
        {
            return std::unexpected(QString("Position %1 has an invalid quantity '%2'")
                                       .arg(p_position.getPositionID(), p_position.getQuantity()));
        }

        const double absoluteQuantity = std::abs(rawQuantity);
        const qint64 roundedQuantity = std::llround(absoluteQuantity);
        if (std::abs(absoluteQuantity - static_cast<double>(roundedQuantity)) > 0.0001)
        {
            return std::unexpected(QString("Position %1 has a non-integer quantity '%2'")
                                       .arg(p_position.getPositionID(), p_position.getQuantity()));
        }
        if (roundedQuantity <= 0)
        {
            return std::unexpected(QString("Position %1 has no shares to close").arg(p_position.getPositionID()));
        }
        if (roundedQuantity > std::numeric_limits<int>::max())
        {
            return std::unexpected(QString("Position %1 quantity '%2' exceeds the supported order size range")
                                       .arg(p_position.getPositionID(), p_position.getQuantity()));
        }

        return static_cast<int>(roundedQuantity);
    }

    [[nodiscard]] int signedPositionShares(const Position& p_position)
    {
        bool ok = false;
        const double rawQuantity = p_position.getQuantity().trimmed().toDouble(&ok);
        if (!ok)
        {
            return 0;
        }

        const int shares = static_cast<int>(std::llround(std::abs(rawQuantity)));
        if (shares == 0)
        {
            return 0;
        }

        const QString side = p_position.getLongShort().trimmed().toUpper();
        if (side == QStringLiteral("SHORT"))
        {
            return -shares;
        }
        if (side == QStringLiteral("LONG"))
        {
            return shares;
        }

        return rawQuantity < 0.0 ? -shares : shares;
    }

    [[nodiscard]] PlaceOrderResult makeRejectedPlaceOrderResult(const QString& p_reasonCode, const QString& p_message)
    {
        QJsonObject resultJson;
        QJsonArray errors;
        QJsonObject errorItem;
        errorItem["OrderID"] = QString();
        errorItem["Message"] = p_message;
        errorItem["Error"] = p_reasonCode;
        errors.append(errorItem);
        resultJson["Errors"] = errors;
        return PlaceOrderResult(resultJson);
    }

    [[nodiscard]] bool isFillLikeStatus(const Order::Status p_status)
    {
        return p_status == Order::Status::FLL || p_status == Order::Status::FLP || p_status == Order::Status::FPR;
    }

    [[nodiscard]] bool isTerminalRejectLikeStatus(const Order::Status p_status)
    {
        return p_status == Order::Status::CAN || p_status == Order::Status::EXP || p_status == Order::Status::OUT ||
               p_status == Order::Status::REJ || p_status == Order::Status::BRO || p_status == Order::Status::TSC;
    }

    [[nodiscard]] bool isTerminalOrderStatus(const Order::Status p_status)
    {
        return isFillLikeStatus(p_status) || isTerminalRejectLikeStatus(p_status) || p_status == Order::Status::UCN ||
               p_status == Order::Status::DON;
    }

    [[nodiscard]] std::optional<int> parsePositiveShareCount(const QString& p_rawValue)
    {
        bool ok = false;
        const int value = p_rawValue.trimmed().toInt(&ok);
        if (!ok || value <= 0)
        {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] std::optional<int> parseAbsoluteShareCount(const QString& p_rawValue)
    {
        bool ok = false;
        const int value = p_rawValue.trimmed().toInt(&ok);
        if (!ok || value == 0 || value == std::numeric_limits<int>::min())
        {
            return std::nullopt;
        }
        return std::abs(value);
    }

    [[nodiscard]] bool isCloseActionForPosition(const QString& p_tradeAction, const QString& p_longShort)
    {
        QString normalizedAction = p_tradeAction.trimmed().toUpper();
        normalizedAction.remove(' ');
        const QString normalizedSide = p_longShort.trimmed().toUpper();

        if (normalizedSide == "LONG")
        {
            return normalizedAction == "SELL" || normalizedAction == "SELLTOCLOSE";
        }
        if (normalizedSide == "SHORT")
        {
            return normalizedAction == "BUY" || normalizedAction == "BUYTOCOVER" || normalizedAction == "BUYTOCLOSE";
        }

        return false;
    }

    [[nodiscard]] bool isOpenActionForPosition(const QString& p_tradeAction, const QString& p_longShort)
    {
        QString normalizedAction = p_tradeAction.trimmed().toUpper();
        normalizedAction.remove(' ');
        const QString normalizedSide = p_longShort.trimmed().toUpper();

        if (normalizedSide == "LONG")
        {
            return normalizedAction == "BUY" || normalizedAction == "BUYTOOPEN";
        }
        if (normalizedSide == "SHORT")
        {
            return normalizedAction == "SELL" || normalizedAction == "SELLSHORT" || normalizedAction == "SELLTOOPEN";
        }

        return false;
    }

    [[nodiscard]] std::optional<Order>
    findBestCloseOrderReconciliationCandidate(const QMap<QString, Order>& p_latestOrdersById,
                                              const Position& p_closedPosition,
                                              const QString& p_accountID,
                                              const QDateTime& p_referenceTimestamp,
                                              const QSet<QString>* p_allowedOrderIds = nullptr)
    {
        const QString symbol = p_closedPosition.getSymbol().trimmed().toUpper();
        if (symbol.isEmpty())
        {
            return std::nullopt;
        }

        const QString positionSide = p_closedPosition.getLongShort().trimmed();
        if (positionSide.isEmpty())
        {
            return std::nullopt;
        }

        const QString normalizedAccount = p_accountID.trimmed().toUpper();
        const auto closedQuantity = parseAbsoluteShareCount(p_closedPosition.getQuantity());

        constexpr qint64 kCloseReconciliationMaxSkewMs = 10 * 60 * 1000;
        std::optional<Order> bestCandidate;
        bool bestHasValidSkew = false;
        qint64 bestSkewMs = std::numeric_limits<qint64>::max();

        for (auto it = p_latestOrdersById.constBegin(); it != p_latestOrdersById.constEnd(); ++it)
        {
            const QString orderId = it.key().trimmed();
            if (orderId.isEmpty())
            {
                continue;
            }

            if (p_allowedOrderIds != nullptr && !p_allowedOrderIds->contains(orderId))
            {
                continue;
            }

            const Order& order = it.value();
            if (isTerminalOrderStatus(order.getOrderStatus()))
            {
                continue;
            }

            if (order.getSymbol().trimmed().toUpper() != symbol)
            {
                continue;
            }

            const QString orderAccountID = order.getAccountID().trimmed().toUpper();
            if (!normalizedAccount.isEmpty() && !orderAccountID.isEmpty() && orderAccountID != normalizedAccount)
            {
                continue;
            }

            if (!isCloseActionForPosition(order.getTradeAction(), positionSide))
            {
                continue;
            }

            const auto orderQuantity = parsePositiveShareCount(order.getQuantity());
            if (closedQuantity.has_value())
            {
                if (!orderQuantity.has_value() || orderQuantity.value() != closedQuantity.value())
                {
                    continue;
                }
            }

            const QDateTime openedDateTime = order.getOpenedDateTime();
            bool hasValidSkew = false;
            qint64 skewMs = std::numeric_limits<qint64>::max();
            if (openedDateTime.isValid() && p_referenceTimestamp.isValid())
            {
                hasValidSkew = true;
                const qint64 deltaMs = openedDateTime.msecsTo(p_referenceTimestamp);
                skewMs = deltaMs >= 0 ? deltaMs : -deltaMs;
                if (skewMs > kCloseReconciliationMaxSkewMs)
                {
                    continue;
                }
            }
            else if (p_allowedOrderIds == nullptr)
            {
                // Unrestricted fallback matching should require valid timing to avoid stale-order collisions.
                continue;
            }

            if (!bestCandidate.has_value())
            {
                bestCandidate = order;
                bestHasValidSkew = hasValidSkew;
                bestSkewMs = skewMs;
                continue;
            }

            const QDateTime bestOpenedDateTime = bestCandidate->getOpenedDateTime();
            const bool shouldReplace =
                (hasValidSkew && !bestHasValidSkew) || (hasValidSkew == bestHasValidSkew && skewMs < bestSkewMs) ||
                (hasValidSkew == bestHasValidSkew && skewMs == bestSkewMs && openedDateTime.isValid() &&
                 (!bestOpenedDateTime.isValid() || openedDateTime > bestOpenedDateTime));
            if (shouldReplace)
            {
                bestCandidate = order;
                bestHasValidSkew = hasValidSkew;
                bestSkewMs = skewMs;
            }
        }

        return bestCandidate;
    }

    [[nodiscard]] double bestEffortSyntheticFillPrice(const Order& p_order,
                                                      const Position& p_closedPosition,
                                                      QString* p_priceSource = nullptr)
    {
        auto setSource = [&](const char* p_source)
        {
            if (p_priceSource != nullptr)
            {
                *p_priceSource = QLatin1String(p_source);
            }
        };

        if (p_order.getFilledPrice() >= 0.01)
        {
            setSource("order.filledPrice");
            return p_order.getFilledPrice();
        }

        bool ok = false;
        if (p_closedPosition.getLongShort().compare("Long", Qt::CaseInsensitive) == 0)
        {
            const double bid = p_closedPosition.getBid().toDouble(&ok);
            if (ok && bid >= 0.01)
            {
                setSource("position.bid");
                return bid;
            }
        }
        else if (p_closedPosition.getLongShort().compare("Short", Qt::CaseInsensitive) == 0)
        {
            const double ask = p_closedPosition.getAsk().toDouble(&ok);
            if (ok && ask >= 0.01)
            {
                setSource("position.ask");
                return ask;
            }
        }

        const double last = p_closedPosition.getLast().toDouble(&ok);
        if (ok && last >= 0.01)
        {
            setSource("position.last");
            return last;
        }

        if (p_order.getLimitPrice().has_value() && p_order.getLimitPrice().value() >= 0.01)
        {
            setSource("order.limitPrice");
            return p_order.getLimitPrice().value();
        }

        setSource("fallback.minTick");
        return 0.01;
    }

    [[nodiscard]] double bestEffortSyntheticEntryFillPrice(const Order& p_order, const Position& p_openPosition)
    {
        if (p_order.getFilledPrice() >= 0.01)
        {
            return p_order.getFilledPrice();
        }

        bool ok = false;
        const double averagePrice = p_openPosition.getAveragePrice().toDouble(&ok);
        if (ok && averagePrice >= 0.01)
        {
            return averagePrice;
        }

        const double mark = p_openPosition.getMarkToMarketPrice().toDouble(&ok);
        if (ok && mark >= 0.01)
        {
            return mark;
        }

        const double last = p_openPosition.getLast().toDouble(&ok);
        if (ok && last >= 0.01)
        {
            return last;
        }

        if (p_order.getLimitPrice().has_value() && p_order.getLimitPrice().value() >= 0.01)
        {
            return p_order.getLimitPrice().value();
        }

        return 0.01;
    }

    [[nodiscard]] TradeAction closeTradeActionForPosition(const Position& p_position)
    {
        if (p_position.getLongShort() == "Long")
        {
            return TradeAction::Sell;
        }
        if (p_position.getLongShort() == "Short")
        {
            return TradeAction::BuyToCover;
        }

        ASSUME_TRUE(false);
        return TradeAction::Sell;
    }

    [[nodiscard]] bool isBuySideTradeAction(const TradeAction p_tradeAction)
    {
        return p_tradeAction == TradeAction::Buy || p_tradeAction == TradeAction::BuyToCover ||
               p_tradeAction == TradeAction::BuyToOpen || p_tradeAction == TradeAction::BuyToClose;
    }

    [[nodiscard]] bool isSellSideTradeAction(const TradeAction p_tradeAction)
    {
        return p_tradeAction == TradeAction::Sell || p_tradeAction == TradeAction::SellShort ||
               p_tradeAction == TradeAction::SellToOpen || p_tradeAction == TradeAction::SellToClose;
    }

    [[nodiscard]] QPair<QString, QString> orderLegDirectionForTradeAction(const TradeAction p_tradeAction)
    {
        switch (p_tradeAction)
        {
        case TradeAction::Buy:
        case TradeAction::BuyToOpen:
            return {QStringLiteral("Buy"), QStringLiteral("Open")};
        case TradeAction::BuyToCover:
        case TradeAction::BuyToClose:
            return {QStringLiteral("Buy"), QStringLiteral("Close")};
        case TradeAction::Sell:
        case TradeAction::SellToClose:
            return {QStringLiteral("Sell"), QStringLiteral("Close")};
        case TradeAction::SellShort:
        case TradeAction::SellToOpen:
            return {QStringLiteral("Sell"), QStringLiteral("Open")};
        }

        ASSUME_TRUE(false);
        return {QStringLiteral("Buy"), QStringLiteral("Open")};
    }

    [[nodiscard]] QJsonObject buildSyntheticPlacedOrderJson(const PlaceOrderRequest& p_orderRequest,
                                                            const QString& p_orderID)
    {
        QJsonObject json;
        json["AccountID"] = p_orderRequest.getAccountID();
        json["OpenedDateTime"] = MainApp::getCurrentAppTime().toUTC().toString(Qt::ISODate);
        json["OrderID"] = p_orderID;
        json["OrderType"] = OrderType::toString(p_orderRequest.getOrderType().type);
        json["Status"] = QStringLiteral("OPN");
        json["StatusDescription"] = Order::getStatusDescriptionForStatus(Order::Status::OPN);
        json["Duration"] = p_orderRequest.getTimeInForce().toJson().value("Duration").toString();
        json["FilledPrice"] = QStringLiteral("0");

        if (p_orderRequest.getLimitPrice().has_value())
        {
            json["LimitPrice"] = QString::number(p_orderRequest.getLimitPrice().value(), 'f', 2);
        }
        if (p_orderRequest.getStopPrice().has_value())
        {
            json["StopPrice"] = QString::number(p_orderRequest.getStopPrice().value(), 'f', 2);
        }

        const auto [buyOrSell, openOrClose] = orderLegDirectionForTradeAction(p_orderRequest.getTradeAction());
        QJsonObject leg;
        leg["Symbol"] = p_orderRequest.getSymbol();
        leg["QuantityOrdered"] = QString::number(p_orderRequest.getQuantity());
        leg["QuantityRemaining"] = QString::number(p_orderRequest.getQuantity());
        leg["BuyOrSell"] = buyOrSell;
        leg["OpenOrClose"] = openOrClose;
        QJsonArray legs;
        legs.append(leg);
        json["Legs"] = legs;

        return json;
    }

    [[nodiscard]] std::expected<double, QString> bestExecutableBookPrice(const Level2& p_level2,
                                                                         const TradeAction p_tradeAction)
    {
        if (isBuySideTradeAction(p_tradeAction))
        {
            if (p_level2.m_asks[0].m_price <= 0.0)
            {
                return std::unexpected("Best ask is unavailable");
            }
            return p_level2.m_asks[0].m_price;
        }

        if (isSellSideTradeAction(p_tradeAction))
        {
            if (p_level2.m_bids[0].m_price <= 0.0)
            {
                return std::unexpected("Best bid is unavailable");
            }
            return p_level2.m_bids[0].m_price;
        }

        return std::unexpected("Unsupported trade action for best-price lookup");
    }

    [[nodiscard]] std::expected<double, QString>
    calculateAggressiveMarketableLimitPrice(const Level2& p_level2,
                                            const TradeAction p_tradeAction,
                                            const double p_offsetCents)
    {
        const double offsetDollars = p_offsetCents / 100.0;
        auto bestPriceResult = bestExecutableBookPrice(p_level2, p_tradeAction);
        if (!bestPriceResult.has_value())
        {
            return std::unexpected(bestPriceResult.error());
        }

        double price = bestPriceResult.value();
        if (isBuySideTradeAction(p_tradeAction))
        {
            price += offsetDollars;
        }
        else if (isSellSideTradeAction(p_tradeAction))
        {
            price -= offsetDollars;
            if (price < 0.01)
            {
                price = 0.01;
            }
        }
        else
        {
            return std::unexpected("Unsupported trade action for aggressive marketable limit pricing");
        }

        return price;
    }

    [[nodiscard]] bool
    isLimitPriceMarketable(const Level2& p_level2, const TradeAction p_tradeAction, const double p_limitPrice)
    {
        if (isBuySideTradeAction(p_tradeAction))
        {
            const double bestAsk = p_level2.m_asks[0].m_price;
            return bestAsk > 0.0 && bestAsk <= p_limitPrice;
        }

        if (isSellSideTradeAction(p_tradeAction))
        {
            const double bestBid = p_level2.m_bids[0].m_price;
            return bestBid > 0.0 && bestBid >= p_limitPrice;
        }

        return false;
    }

    [[nodiscard]] std::expected<double, QString> calculatePassiveRestingLimitPrice(const Level2& p_level2,
                                                                                   const TradeAction p_tradeAction,
                                                                                   const double p_offsetCents)
    {
        const double offsetDollars = p_offsetCents / 100.0;

        if (isBuySideTradeAction(p_tradeAction))
        {
            const double bestBid = p_level2.m_bids[0].m_price;
            if (bestBid <= 0.0)
            {
                return std::unexpected("Best bid is unavailable");
            }

            return bestBid + offsetDollars;
        }

        if (isSellSideTradeAction(p_tradeAction))
        {
            const double bestAsk = p_level2.m_asks[0].m_price;
            if (bestAsk <= 0.0)
            {
                return std::unexpected("Best ask is unavailable");
            }

            const double restingPrice = std::max(0.01, bestAsk - offsetDollars);
            return restingPrice;
        }

        return std::unexpected("Unsupported trade action for passive resting limit pricing");
    }

    [[nodiscard]] std::optional<Level2> level2FromPositionBbo(const Position& p_position)
    {
        bool bidOk = false;
        const double bidPrice = p_position.getBid().toDouble(&bidOk);
        bool askOk = false;
        const double askPrice = p_position.getAsk().toDouble(&askOk);

        if ((!bidOk || bidPrice <= 0.0) && (!askOk || askPrice <= 0.0))
        {
            return std::nullopt;
        }

        Level2 level2;
        level2.m_symbol = p_position.getSymbol();
        level2.m_timeStamp = MainApp::getCurrentAppTime();
        if (bidOk && bidPrice > 0.0)
        {
            level2.m_bids[0].m_price = bidPrice;
        }
        if (askOk && askPrice > 0.0)
        {
            level2.m_asks[0].m_price = askPrice;
        }

        return level2;
    }

    void appendPlaceOrderItems(const QVector<OrderResultItem>& p_items,
                               QStringList* const p_orderIds,
                               QStringList* const p_messages,
                               QStringList* const p_errors)
    {
        ASSUME_DIFF(p_orderIds, nullptr);
        ASSUME_DIFF(p_messages, nullptr);
        ASSUME_DIFF(p_errors, nullptr);

        for (const OrderResultItem& item: p_items)
        {
            if (!item.getOrderID().isEmpty())
            {
                p_orderIds->append(item.getOrderID());
            }
            if (!item.getMessage().isEmpty())
            {
                p_messages->append(item.getMessage());
            }
            if (item.getError().has_value())
            {
                p_errors->append(item.getError().value());
            }
        }
    }

    [[nodiscard]] QString summarizeClosePositionFailure(const ClosePositionItemResult& p_item)
    {
        if (!p_item.brokerErrors.isEmpty())
        {
            return p_item.brokerErrors.join(" | ");
        }
        if (!p_item.brokerMessages.isEmpty())
        {
            return p_item.brokerMessages.join(" | ");
        }
        return "Order placement returned errors";
    }
} // namespace

MainAlgo* MainAlgo::getInstance()
{
    if (m_instance == nullptr)
    {
        m_instance = new MainAlgo();
    }
    return m_instance;
}

void MainAlgo::destroyInstance()
{
    ASSUME_TRUE(m_instance != nullptr);

    delete m_instance;
    m_instance = nullptr;
}


MainAlgo::MainAlgo()
{
    thread.setObjectName("MainAlgoThread");

    this->moveToThread(&thread);

    this->setObjectName("MainAlgo");

    m_strategyManager = std::make_unique<StrategyManager>(this);
    m_strategyManager->moveToThread(&thread);
    m_riskManager = std::make_unique<RiskManager>();

    m_manualConfirmationFrontendAvailable = true;

    connect(&thread, &QThread::started, this, &MainAlgo::onThreadStarted);

    DEBUG << "Singleton instance created";
}

MainAlgo::~MainAlgo()
{
    DEBUG << "MainAlgo destructor - stopping thread";

    // Thread affinity assertion - destructor must be called from main thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());

    // Mark shutdown early so async callbacks can drop late results safely.
    m_isShuttingDown.store(true, std::memory_order_release);
    m_accountsRequestGeneration.fetch_add(1, std::memory_order_acq_rel);

    // CRITICAL: Destroy all thread-owned objects that may own timers/notifiers/processes on the
    // MainAlgo thread BEFORE calling thread.quit(). If we destroy them from the main thread after
    // the thread has stopped, Qt warns about cross-thread teardown and can leave strategy runtime
    // cleanup happening on the wrong thread. The MainAlgo event loop is still running here, so
    // BlockingQueuedConnection is safe.
    QMetaObject::invokeMethod(
        this,
        [this]()
        {
            // Stop and destroy balance polling timer on its own thread
            m_balancePollingTimer.reset();
            m_controlSymbolLeaseCleanupTimer.reset();
            m_managedBracketMonitorTimer.reset();
            m_controlSymbolLeaseExpirations.clear();
            m_managedBrackets.clear();

            cancelAllManualOrderConfirmations(QStringLiteral("MainAlgo shutdown"), false);
            cancelDeferredClosePositionsRequests(QStringLiteral("MainAlgo shutdown"));
            m_manualOrderConfirmationTimer.reset();

            // Destroy StrategyManager on its own thread so process supervision, socket
            // notifiers, and unload teardown all stay on MainAlgo.
            m_strategyManager.reset();
            m_riskManager.reset();
        },
        Qt::BlockingQueuedConnection);

    // CRITICAL: Stop thread BEFORE Qt's parent-child deletion destroys thread-owned objects
    thread.quit();

    // Wait for thread to finish (with timeout)
    if (!thread.wait(5000))
    {
        CRITICAL << "MainAlgo thread did not finish within timeout, terminating";
        thread.terminate();
        thread.wait();
    }

    DEBUG << "Destroyed singleton instance";
}

void MainAlgo::start()
{
    thread.start();
}

void MainAlgo::onThreadStarted()
{
    // Set kernel thread name for visibility in trace tools (ps, top, LTTng, TraceCompass)
    ThreadNames::setCurrentThreadName("MainAlgo");

    m_balancePollingTimer = std::make_unique<QTimer>(this);
    m_controlSymbolLeaseCleanupTimer = std::make_unique<QTimer>(this);
    m_manualOrderConfirmationTimer = std::make_unique<QTimer>(this);
    m_managedBracketMonitorTimer = std::make_unique<QTimer>(this);
    m_manualOrderConfirmationTimer->setSingleShot(true);
    m_managedBracketMonitorTimer->setSingleShot(false);

    connect(m_balancePollingTimer.get(), &QTimer::timeout, this, &MainAlgo::requestBalance, Qt::UniqueConnection);
    connect(m_controlSymbolLeaseCleanupTimer.get(),
            &QTimer::timeout,
            this,
            &MainAlgo::pruneExpiredControlSymbolLeases,
            Qt::UniqueConnection);
    connect(m_manualOrderConfirmationTimer.get(),
            &QTimer::timeout,
            this,
            &MainAlgo::onManualOrderConfirmationTimeout,
            Qt::UniqueConnection);
    connect(m_managedBracketMonitorTimer.get(),
            &QTimer::timeout,
            this,
            &MainAlgo::monitorManagedBrackets,
            Qt::UniqueConnection);
    m_controlSymbolLeaseCleanupTimer->start(PlatformControlConstants::SYMBOL_CONTEXT_LEASE_CLEANUP_INTERVAL_MS);
    m_managedBracketMonitorTimer->start(100);

    // Positions: broadcast to all strategies via adapter (strategy thread)
    connect(this,
            &MainAlgo::receivedNewPosition,
            m_strategyManager.get(),
            &StrategyManager::onMainAlgoPositionUpdated,
            Qt::QueuedConnection);

    // Balance: broadcast to all strategies via adapter (strategy thread)
    connect(this,
            &MainAlgo::balanceUpdated,
            m_strategyManager.get(),
            &StrategyManager::onMainAlgoBalanceUpdated,
            Qt::QueuedConnection);

    // StrategyManager now shares the MainAlgo thread, so symbol releases can
    // decrement SymbolContext ref counts synchronously during unload/shutdown.
    connect(m_strategyManager.get(),
            &StrategyManager::symbolReleased,
            this,
            &MainAlgo::releaseSymbolContextRef,
            Qt::DirectConnection);
    connect(this,
            &MainAlgo::replayPaused,
            m_strategyManager.get(),
            &StrategyManager::onReplayPaused,
            Qt::QueuedConnection);
    connect(this,
            &MainAlgo::replayResumed,
            m_strategyManager.get(),
            &StrategyManager::onReplayResumed,
            Qt::QueuedConnection);
    connect(this,
            &MainAlgo::replayResumed,
            this,
            &MainAlgo::onReplayResumedForDeferredClosePositions,
            Qt::UniqueConnection);

    // Direct cross-thread routing for replay market data from DBClient.
    connect(DBClient::getInstance(), &DBClient::newLevel2, this, &MainAlgo::routeLevel2, Qt::DirectConnection);
    connect(DBClient::getInstance(), &DBClient::newTrade, this, &MainAlgo::routeTrade, Qt::DirectConnection);
    // Direct cross-thread routing for live/sim market data from TradeStation.
    connect(TSClient::getInstance(), &TSClient::newBarReceived, this, &MainAlgo::routeBar, Qt::DirectConnection);
    connect(TSClient::getInstance(), &TSClient::newLevel2Received, this, &MainAlgo::routeLevel2, Qt::DirectConnection);
    connect(TSClient::getInstance(), &TSClient::newQuoteReceived, this, &MainAlgo::routeQuote, Qt::QueuedConnection);

    // Forward DBClient replay lifecycle signals to MainAlgo signals for UI.
    // Wired once here (both singletons are stable); enterReplayMode/Paused no longer re-wires these.
    auto* dbClient = DBClient::getInstance();
    bool connected = connect(dbClient, &DBClient::replayStarted, this, &MainAlgo::replayStarted);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayStopped, this, &MainAlgo::replayStopped);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayPaused, this, &MainAlgo::replayPaused);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayResumed, this, &MainAlgo::replayResumed);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayTimeUpdated, this, &MainAlgo::onReplayTimeReceived);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayEndReached, this, &MainAlgo::replayEndReached);
    ASSUME_TRUE(connected);
    connected = connect(dbClient, &DBClient::replayEndReached, this, &MainAlgo::onReplayEndReached);
    ASSUME_TRUE(connected);
}

// ── Centralized routing (called directly from DBClient thread) ─────────────

void MainAlgo::subscribeExistingLiveSymbols()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (MainApp::isInReplayMode() || MainApp::isInReviewMode())
    {
        return;
    }

    QStringList symbolsToSubscribe;
    {
        QReadLocker lock(&m_symbolContextsLock);
        symbolsToSubscribe.reserve(m_symbolContexts.size());
        for (auto it = m_symbolContexts.cbegin(); it != m_symbolContexts.cend(); ++it)
        {
            if (!it.value().isNull())
            {
                symbolsToSubscribe.append(it.key());
            }
        }
    }

    if (symbolsToSubscribe.isEmpty())
    {
        return;
    }

    INFO << "TradeStation live market-data path ready; subscribing existing symbols:" << symbolsToSubscribe;

    for (const QString& symbol: symbolsToSubscribe)
    {
        QPointer<SymbolContext> symbolContext;
        {
            QReadLocker lock(&m_symbolContextsLock);
            symbolContext = m_symbolContexts.value(symbol);
        }
        if (!symbolContext.isNull())
        {
            subscribeLiveSymbol(symbolContext);
        }
    }
}

void MainAlgo::scheduleLiveStreamRetry(SymbolContext* p_symbolContext,
                                       LiveStreamRetryState& p_state,
                                       const QString& p_feed,
                                       Stream::StreamError p_reason,
                                       const QString& p_message)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_DIFF(p_symbolContext, nullptr);
    const std::optional<int> retryDelay = p_state.schedule(p_reason);
    if (retryDelay.has_value() || p_state.terminal)
    {
        const QString reason =
            QString::fromLatin1(QMetaEnum::fromType<Stream::StreamError>().valueToKey(static_cast<int>(p_reason)));
        p_state.lastError = MarketDataSubscriptionError{p_symbolContext->symbol,
                                                        p_feed,
                                                        reason,
                                                        p_message,
                                                        p_state.terminal,
                                                        retryDelay.value_or(0)};
        emit p_symbolContext->marketDataSubscriptionFailed(p_state.lastError.value());
    }
    if (p_state.terminal)
    {
        WARNING << "Live subscription rejected; automatic retry disabled for" << p_symbolContext->symbol
                << "message=" << p_message;
        return;
    }
    if (!retryDelay.has_value())
    {
        return;
    }
    const int delayMs = retryDelay.value();
    WARNING << "Scheduling live stream retry for" << p_symbolContext->symbol << "delayMs=" << delayMs;
    QPointer<SymbolContext> context = p_symbolContext;
    QTimer::singleShot(delayMs,
                       this,
                       [this, context, state = &p_state]()
                       {
                           if (context.isNull())
                           {
                               return;
                           }
                           state->pending = false;
                           subscribeLiveSymbol(context);
                       });
}

void MainAlgo::subscribeLiveSymbol(SymbolContext* p_symbolContext)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_DIFF(p_symbolContext, nullptr);

    if (MainApp::isInReplayMode() || MainApp::isInReviewMode())
    {
        return;
    }

    TSClient* const tsClient = TSClient::getInstance();
    if (!tsClient->isAuthenticated())
    {
        DEBUG << "Skipping live symbol subscription while TradeStation is unauthenticated:" << p_symbolContext->symbol;
        return;
    }

    const QDateTime now = MainApp::getCurrentAppTime();
    const QDate currentDate = now.date();
    if (currentDate.dayOfWeek() >= Qt::Monday && currentDate.dayOfWeek() <= Qt::Friday &&
        now.time() >= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        p_symbolContext->barCache.warmCurrentDayCacheForLive(now);

        if (!p_symbolContext->m_liveCurrentDayHistoryPrefetchIssued)
        {
            const QTime cappedNowTime = std::min(now.time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);
            const int lastIndex = BarUtils::barIndex(TimeFrame::ONE_MINUTE, cappedNowTime);
            const QTime prefetchLastTime = BarUtils::indexToBarTime(TimeFrame::ONE_MINUTE, lastIndex);

            DEBUG << "Priming current-day historical bars for" << p_symbolContext->symbol << "from"
                  << TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION << "to" << prefetchLastTime;

            auto barsResult =
                p_symbolContext->barCache.getBars(TimeFrame::ONE_MINUTE,
                                                  currentDate,
                                                  TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                                  prefetchLastTime);

            if (std::holds_alternative<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(
                    barsResult))
            {
                std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(barsResult)
                    .then(this,
                          [symbol = p_symbolContext->symbol](
                              std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error> p_result)
                          {
                              if (!p_result.has_value())
                              {
                                  qCWarning(LOGGING_CATEGORY) << "Current-day historical prefetch failed for" << symbol
                                                              << "error=" << tsClientErrorToString(p_result.error());
                                  return;
                              }

                              qCDebug(LOGGING_CATEGORY) << "Current-day historical prefetch complete for" << symbol
                                                        << "bars=" << p_result.value()->size();
                          });
            }

            p_symbolContext->m_liveCurrentDayHistoryPrefetchIssued = true;
        }
    }

    const QString symbol = p_symbolContext->symbol;

    const auto ensureBarStream = [this, tsClient, p_symbolContext, symbol]()
    {
        if (!p_symbolContext->m_streamBars.isNull() || p_symbolContext->m_barStreamRetry.pending ||
            p_symbolContext->m_barStreamRetry.terminal)
        {
            return;
        }

        p_symbolContext->m_streamBars =
            tsClient->openStreamBars(symbol, 1, TSClient::BarUnit::Minute, 1, TSClient::BarSessionTemplate::USEQ24Hour);
        if (p_symbolContext->m_streamBars.isNull())
        {
            WARNING << "Failed to open bars stream for" << symbol;
            scheduleLiveStreamRetry(p_symbolContext,
                                    p_symbolContext->m_barStreamRetry,
                                    QStringLiteral("bars"),
                                    Stream::StreamError::Failed,
                                    QStringLiteral("Bars stream creation failed"));
            return;
        }

        connect(p_symbolContext->m_streamBars,
                &StreamBars::newBarReceived,
                this,
                [context = QPointer<SymbolContext>(p_symbolContext)](const Bar&)
                {
                    if (!context.isNull())
                    {
                        context->m_barStreamRetry.delayMs = StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS;
                        context->m_barStreamRetry.lastError.reset();
                    }
                });
        connect(p_symbolContext->m_streamBars,
                &Stream::streamClosed,
                this,
                [this, symbol, context = QPointer<SymbolContext>(p_symbolContext)](Stream::StreamError p_reason,
                                                                                   const QString& p_message)
                {
                    if (p_reason == Stream::StreamError::Closed || context.isNull())
                    {
                        return;
                    }

                    WARNING << "Bars stream closed for" << symbol << "reason=" << static_cast<int>(p_reason)
                            << "message=" << p_message;

                    context->m_streamBars = nullptr;
                    scheduleLiveStreamRetry(context,
                                            context->m_barStreamRetry,
                                            QStringLiteral("bars"),
                                            p_reason,
                                            p_message);
                });
    };

    const auto attachDepthStream = [this, symbol, context = QPointer<SymbolContext>(p_symbolContext)](
                                       QPointer<StreamMarketDepthAggregate> p_stream)
    {
        if (context.isNull())
        {
            if (!p_stream.isNull())
            {
                TSClient::getInstance()->closeStream(p_stream);
            }
            return;
        }
        context->m_depthStreamRetry.pending = false;
        if (p_stream.isNull())
        {
            WARNING << "Failed to open queued market-depth stream for" << symbol;
            scheduleLiveStreamRetry(context,
                                    context->m_depthStreamRetry,
                                    QStringLiteral("depth"),
                                    Stream::StreamError::Failed,
                                    QStringLiteral("Queued market-depth stream creation failed"));
            return;
        }

        context->m_streamMarketDepthAggregate = p_stream;
        connect(p_stream,
                &StreamMarketDepthAggregate::newLevel2Received,
                this,
                [context](const Level2&)
                {
                    if (!context.isNull())
                    {
                        context->m_depthStreamRetry.delayMs = StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS;
                        context->m_depthStreamRetry.lastError.reset();
                    }
                });
        connect(p_stream,
                &Stream::streamClosed,
                this,
                [this, symbol, context](Stream::StreamError p_reason, const QString& p_message)
                {
                    if (p_reason == Stream::StreamError::Closed || context.isNull())
                    {
                        return;
                    }

                    WARNING << "Level2 stream closed for" << symbol << "reason=" << static_cast<int>(p_reason)
                            << "message=" << p_message;

                    context->m_streamMarketDepthAggregate = nullptr;
                    scheduleLiveStreamRetry(context,
                                            context->m_depthStreamRetry,
                                            QStringLiteral("depth"),
                                            p_reason,
                                            p_message);
                });
    };

    const auto ensureDepthStream = [this, tsClient, p_symbolContext, attachDepthStream, symbol]()
    {
        if (!p_symbolContext->m_streamMarketDepthAggregate.isNull() || p_symbolContext->m_depthStreamRetry.pending ||
            p_symbolContext->m_depthStreamRetry.terminal)
        {
            return;
        }

        p_symbolContext->m_depthStreamRetry.pending = true;
        auto streamResult = tsClient->openStreamMarketDepthAggregate(
            symbol,
            static_cast<unsigned int>(MarketDepthConstants::DEFAULT_MARKET_DEPTH_LEVELS));
        if (streamResult.has_value())
        {
            attachDepthStream(streamResult.value());
            return;
        }

        QFuture<QPointer<StreamMarketDepthAggregate>> queuedFuture = streamResult.error();
        queuedFuture.then(this,
                          [attachDepthStream](QPointer<StreamMarketDepthAggregate> p_stream)
                          { attachDepthStream(p_stream); });
    };

    const auto ensureQuoteStream = [this, tsClient, p_symbolContext, symbol]()
    {
        if (!p_symbolContext->m_streamQuote.isNull() || p_symbolContext->m_quoteStreamRetry.pending ||
            p_symbolContext->m_quoteStreamRetry.terminal)
        {
            return;
        }

        p_symbolContext->m_streamQuote = tsClient->openStreamQuote(QStringList{symbol});
        if (p_symbolContext->m_streamQuote.isNull())
        {
            WARNING << "Failed to open quote stream for" << symbol;
            scheduleLiveStreamRetry(p_symbolContext,
                                    p_symbolContext->m_quoteStreamRetry,
                                    QStringLiteral("quotes"),
                                    Stream::StreamError::Failed,
                                    QStringLiteral("Quote stream creation failed"));
            return;
        }

        connect(p_symbolContext->m_streamQuote,
                &StreamQuote::newQuoteReceived,
                this,
                [context = QPointer<SymbolContext>(p_symbolContext)](const Quote&)
                {
                    if (!context.isNull())
                    {
                        context->m_quoteStreamRetry.delayMs = StreamConstants::LIVE_RETRY_INITIAL_DELAY_MS;
                        context->m_quoteStreamRetry.lastError.reset();
                    }
                });
        connect(p_symbolContext->m_streamQuote,
                &Stream::streamClosed,
                this,
                [this, symbol, context = QPointer<SymbolContext>(p_symbolContext)](Stream::StreamError p_reason,
                                                                                   const QString& p_message)
                {
                    if (p_reason == Stream::StreamError::Closed || context.isNull())
                    {
                        return;
                    }

                    WARNING << "Quote stream closed for" << symbol << "reason=" << static_cast<int>(p_reason)
                            << "message=" << p_message;

                    context->m_streamQuote = nullptr;
                    scheduleLiveStreamRetry(context,
                                            context->m_quoteStreamRetry,
                                            QStringLiteral("quotes"),
                                            p_reason,
                                            p_message);
                });
    };

    ensureBarStream();
    ensureDepthStream();
    ensureQuoteStream();
}

void MainAlgo::routeBar(const QString& p_symbol, const Bar& p_bar)
{
    QReadLocker lock(&m_symbolContextsLock);
    QPointer<SymbolContext> sc = m_symbolContexts.value(p_symbol);
    if (!sc.isNull())
    {
        sc->enqueueBar(p_bar);
    }
}

void MainAlgo::routeLevel2(const QString& p_symbol, const Level2& p_level2)
{
    QReadLocker lock(&m_symbolContextsLock);
    auto sc = m_symbolContexts.value(p_symbol);
    if (!sc.isNull())
        sc->enqueueLevel2(p_level2);
}

void MainAlgo::routeQuote(const QString& p_symbol, const Quote& p_quote)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    QPointer<SymbolContext> sc;
    {
        QReadLocker lock(&m_symbolContextsLock);
        sc = m_symbolContexts.value(p_symbol);
    }

    if (sc.isNull())
    {
        return;
    }

    const double lastPrice = p_quote.getLast();
    const unsigned int lastSize = p_quote.getLastSize();
    if (!(std::isfinite(lastPrice) && lastPrice > 0.0) || lastSize == 0u)
    {
        return;
    }

    QDateTime tradeTime = p_quote.getTradeTime();
    if (!tradeTime.isValid())
    {
        tradeTime = MainApp::getCurrentAppTime();
    }
    else
    {
        tradeTime = tradeTime.toTimeZone(TradingHours::MARKET_TIMEZONE);
    }

    QuoteTradeFingerprint& lastFingerprint = m_lastQuoteTradeBySymbol[p_symbol];
    const bool hasTradeChanged = !lastFingerprint.timestamp.isValid() || lastFingerprint.timestamp != tradeTime ||
                                 lastFingerprint.size != lastSize ||
                                 !qFuzzyCompare(lastFingerprint.price + 1.0, lastPrice + 1.0);
    if (!hasTradeChanged)
    {
        return;
    }

    lastFingerprint.timestamp = tradeTime;
    lastFingerprint.price = lastPrice;
    lastFingerprint.size = lastSize;

    Trade syntheticTrade;
    syntheticTrade.m_symbol = p_symbol;
    syntheticTrade.m_timestamp = tradeTime;
    syntheticTrade.m_price = lastPrice;
    syntheticTrade.m_size =
        static_cast<int>(qMin(lastSize, static_cast<unsigned int>(std::numeric_limits<int>::max())));
    syntheticTrade.m_side = TradeSide::None;

    const std::optional<double> bestBid = p_quote.getBestBid();
    const std::optional<double> bestAsk = p_quote.getBestAsk();
    if (bestBid.has_value() && bestAsk.has_value())
    {
        if (lastPrice >= bestAsk.value())
        {
            syntheticTrade.m_side = TradeSide::Ask;
        }
        else if (lastPrice <= bestBid.value())
        {
            syntheticTrade.m_side = TradeSide::Bid;
        }
    }

    sc->enqueueTrade(syntheticTrade);
}

void MainAlgo::routeTrade(const QString& p_symbol, const Trade& p_trade)
{
    QReadLocker lock(&m_symbolContextsLock);
    auto sc = m_symbolContexts.value(p_symbol);
    if (!sc.isNull())
        sc->enqueueTrade(p_trade);
}

/**
 * @brief Handles the selection of a new stock for display.
 *
 * This function is called when the user selects a different stock to display in the UI.
 * It manages the lifecycle of SymbolContext, disconnecting signals from the previous stock,
 * cleaning up resources (such as closing data streams), and setting up the new stock's
 * bar cache and market depth quote receivers with appropriate signal connections.
 *
 * If a stock was previously selected, it ensures proper cleanup by:
 * - Disconnecting signals from the old stock's BarCache and Level2Receiver
 * - Closing any active streams for the old stock
 * - Removing the old SymbolContext from the map and scheduling its deletion
 *
 * For the new stock, it either reuses an existing SymbolContext if the symbol is already
 * in the map, or creates a new one. It then connects the new stock's signals to emit
 * MainAlgo's signals for bar and market depth updates.
 *
 * @param symbol The stock symbol to select for display. Must be a valid stock symbol.
 *
 * @note This method must be called from the MainAlgo thread (QThread::currentThread() == &thread).
 * @note Assumes that if a stock is currently displayed, the new symbol is different.
 * @note Uses Qt's parent-child ownership for memory management of SymbolContext.
 */
void MainAlgo::onSelectDisplayedStock(const QString& symbol)
{
    // Make sure that this method gets Qt::InvokeMethod'ed if called from another thread
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);


    // If there is a current selected stock for display, release it
    if (m_currentDisplayedSymbolContext != nullptr)
    {
        // Selecting the same stock as currently selected. No action taken.
        OBJ_ASSUME_DIFF(m_currentDisplayedSymbolContext->symbol, symbol);

        // Clean up or detach the previous SymbolContext
        QString oldSymbol = m_currentDisplayedSymbolContext->symbol;
        m_currentDisplayedSymbolContext = nullptr;

        // Release the display ref — context is destroyed only if refCount reaches 0
        releaseSymbolContextRef(oldSymbol);
    }

    // Change the stock selected pointer to the new selected stock
    if (m_symbolContexts.contains(symbol))
    {
        m_currentDisplayedSymbolContext = m_symbolContexts[symbol];
        DEBUG << "onSelectDisplayedStock: reusing existing SymbolContext for" << symbol;
    }
    else
    {
        m_currentDisplayedSymbolContext = new SymbolContext(symbol, this); // Pass 'this' as parent
        Q_CHECK_PTR(m_currentDisplayedSymbolContext);

        {
            QWriteLocker lock(&m_symbolContextsLock);
            m_symbolContexts.insert(symbol, m_currentDisplayedSymbolContext);
        }

        // Subscribe to data for the new symbol
        if (MainApp::isInReplayMode())
        {
            if (!DBClient::getInstance()->addReplaySymbol(symbol))
                WARNING << "No replay data for" << symbol << "- live bars will not flow";
        }
        else if (!MainApp::isInReviewMode())
        {
            m_currentDisplayedSymbolContext->barCache.warmCurrentDayCacheForLive(MainApp::getCurrentAppTime());
            subscribeLiveSymbol(m_currentDisplayedSymbolContext);
        }

        DEBUG << "onSelectDisplayedStock: created new SymbolContext for" << symbol;
    }

    // Claim display reference
    ++m_currentDisplayedSymbolContext->m_refCount;
    DEBUG << "onSelectDisplayedStock: set displayed symbol to" << symbol
          << "| refCount:" << m_currentDisplayedSymbolContext->m_refCount
          << "| active SymbolContexts:" << m_symbolContexts.keys();

    // No snapshot-writing connections needed here — SymbolContext always populates
    // its own DisplaySnapshot. The GUI reads from it at 30 Hz.

    bool emittedBracket = false;
    for (auto it = m_managedBrackets.cbegin(); it != m_managedBrackets.cend(); ++it)
    {
        if (it->symbol == symbol)
        {
            emitManagedBracketOverlay(*it, false);
            emittedBracket = true;
            break;
        }
    }
    if (!emittedBracket)
    {
        ManagedBracket clearState;
        clearState.symbol = symbol;
        emitManagedBracketOverlay(clearState, true);
    }

    const QString normalizedSymbol = symbol.trimmed().toUpper();
    auto statusIt = m_strategyStatusesBySymbol.constFind(normalizedSymbol);
    if (statusIt != m_strategyStatusesBySymbol.cend())
    {
        emit strategyStatusEmitted(statusIt.value());
    }
    else
    {
        StrategyStatusEntry clearEntry;
        clearEntry.symbol = normalizedSymbol;
        clearEntry.timestamp = MainApp::getCurrentAppTime();
        clearEntry.action = StrategyStatusEntry::Action::Clear;
        emit strategyStatusEmitted(clearEntry);
    }
}

BarCache::GetBarsResult_t MainAlgo::requestMissingBarsDisplayedStock(QDate date, QTime first, QTime last, TimeFrame tf)
{
    if (m_currentDisplayedSymbolContext == nullptr)
    {
        // Instrument not yet initialized (e.g., setSymbol fired before onSelectDisplayedStock arrived).
        // Return empty result — checkForMissingBars will retry on next scroll/zoom.
        return std::make_shared<QVector<Bar>>();
    }

    DEBUG << "Requested bars from current displayed stock cache: " << first << " to " << last;

    OBJ_ASSUME_LTE(first, last); // The Equal in less than equal is for when the program is launched at 4:02 AM

    return m_currentDisplayedSymbolContext->barCache.getBars(tf, date, first, last);
}

BarCache::GetBarsResult_t MainAlgo::requestHistoricalBarsForSymbol(const QString& p_symbol,
                                                                   QDate p_date,
                                                                   QTime p_first,
                                                                   QTime p_last,
                                                                   TimeFrame p_tf)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_FALSE(p_symbol.isEmpty());

    QPointer<SymbolContext> symbolContext = acquireSymbolContext(p_symbol);
    OBJ_ASSUME_DIFF(symbolContext, nullptr);

    BarCache::GetBarsResult_t result = symbolContext->barCache.getBars(p_tf, p_date, p_first, p_last);
    if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(result))
    {
        releaseSymbolContextRef(p_symbol);
        return std::get<std::shared_ptr<QVector<Bar>>>(result);
    }

    QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> promise;
    QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> forwardedFuture = promise.future();
    promise.start();

    auto sharedPromise =
        std::make_shared<QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(std::move(promise));

    std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result).then(
        [mainAlgo = QPointer<MainAlgo>(this), p_symbol, sharedPromise](
            std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>&& p_bars) mutable
        {
            if (mainAlgo.isNull())
            {
                sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                sharedPromise->finish();
                return;
            }

            QMetaObject::invokeMethod(
                mainAlgo,
                [mainAlgo, p_symbol, sharedPromise, p_bars = std::move(p_bars)]() mutable
                {
                    if (mainAlgo.isNull())
                    {
                        sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                        sharedPromise->finish();
                        return;
                    }

                    mainAlgo->releaseSymbolContextRef(p_symbol);
                    sharedPromise->addResult(std::move(p_bars));
                    sharedPromise->finish();
                },
                Qt::QueuedConnection);
        });

    return forwardedFuture;
}

/*
 * This is the entry point that activates the chain of events after authentication state changes
 */
void MainAlgo::onTradeStationAuthStateChanged(bool isAuthenticated,
                                              TSClient::AuthStateReason reason,
                                              const QString& message)
{
    if (m_isShuttingDown.load(std::memory_order_acquire))
    {
        DEBUG << "Ignoring TradeStation auth state change during MainAlgo shutdown";
        return;
    }

    if (!isAuthenticated)
    {
        // Ignore transient "Connecting" state during token refresh
        if (reason == TSClient::AuthStateReason::Connecting)
        {
            DEBUG << "Token refresh in progress - ignoring transient auth state";
            return;
        }

        if (!m_havePastSuccessfulExchanges)
        {
            CRITICAL << "Tradestation failed to authenticate. Reason : " << message;
            CRITICAL << "Cannot proceed without authentication. Retrying";
        }
        else
        {
            CRITICAL << "Tradestation lost authentication. Reason : " << message;
        }
        return;
    }

    DEBUG << "Tradestation authenticated successfully : " << reason;

    // Now that the TSClient notified us that we are authenticated,
    // the first thing is to request the accounts.
    const uint64_t requestGeneration =
        m_accountsRequestGeneration.fetch_add(1, std::memory_order_acq_rel) + static_cast<uint64_t>(1);
    QFuture<std::expected<QVector<Account>, TSClient::Error>> future = TSClient::getInstance()->getAccounts();

    future.then(
        [mainAlgo = QPointer<MainAlgo>(this),
         requestGeneration](std::expected<QVector<Account>, TSClient::Error> results) mutable
        {
            if (mainAlgo.isNull())
            {
                return;
            }

            QMetaObject::invokeMethod(
                mainAlgo,
                [mainAlgo, requestGeneration, results = std::move(results)]() mutable
                {
                    if (mainAlgo.isNull())
                    {
                        return;
                    }

                    if (mainAlgo->m_isShuttingDown.load(std::memory_order_acquire))
                    {
                        qCDebug(LOGGING_CATEGORY)
                            << mainAlgo->objectName() << "Dropping getAccounts() result during MainAlgo shutdown";
                        return;
                    }

                    if (requestGeneration != mainAlgo->m_accountsRequestGeneration.load(std::memory_order_acquire))
                    {
                        qCDebug(LOGGING_CATEGORY)
                            << mainAlgo->objectName()
                            << "Dropping stale getAccounts() result. requestGeneration=" << requestGeneration;
                        return;
                    }

                    if (results.has_value())
                    {
                        qCDebug(LOGGING_CATEGORY) << mainAlgo->objectName() << "getAccounts() succeeded with"
                                                  << results.value().size() << "accounts";
                        mainAlgo->onReceivedAsyncGetAccounts(results.value());
                        return;
                    }

                    TSClient::Error error = results.error();
                    if (!TSClient::getInstance()->isAuthenticated())
                    {
                        qCWarning(LOGGING_CATEGORY)
                            << mainAlgo->objectName()
                            << "getAccounts() failed while TSClient is unauthenticated; waiting for re-authentication";
                        return;
                    }

                    switch (error)
                    {
                    case TSClient::Error::Timeout:
                        qCCritical(LOGGING_CATEGORY)
                            << mainAlgo->objectName() << "getAccounts() failed with Timeout error";
                        break;
                    case TSClient::Error::JSONError:
                        qCCritical(LOGGING_CATEGORY)
                            << mainAlgo->objectName() << "getAccounts() failed with JSON error";
                        break;
                    case TSClient::Error::Other:
                        qCCritical(LOGGING_CATEGORY)
                            << mainAlgo->objectName() << "getAccounts() failed with Other error";
                        break;
                    default:
                        qCCritical(LOGGING_CATEGORY)
                            << mainAlgo->objectName() << "getAccounts() failed with Unknown error";
                        break;
                    }

                    QTimer::singleShot(
                        1000,
                        mainAlgo,
                        [mainAlgo]()
                        {
                            if (mainAlgo.isNull() || mainAlgo->m_isShuttingDown.load(std::memory_order_acquire))
                            {
                                return;
                            }

                            qCDebug(LOGGING_CATEGORY)
                                << mainAlgo->objectName() << "Retrying getAccounts() after failure";
                            mainAlgo->onTradeStationAuthStateChanged(true,
                                                                     TSClient::AuthStateReason::ValidToken,
                                                                     "Re-auth after getAccounts() failure");
                        });
                },
                Qt::QueuedConnection);
        });
}


void MainAlgo::onReceivedAsyncGetAccounts(const QVector<Account>& results)
{
    if (m_isShuttingDown.load(std::memory_order_acquire))
    {
        DEBUG << "Ignoring TradeStation accounts result during MainAlgo shutdown";
        return;
    }

    m_havePastSuccessfulExchanges = true;

    if (MainApp::isInReplayMode())
    {
        WARNING << "Ignoring async TradeStation accounts result while replay mode is active";
        return;
    }

    // Select account based on trading mode:
    // - LIVE: first account (index 0)
    // - SIM: last account in the list
    if (MainApp::getTradingMode() == TradingMode::Live)
    {
        m_activeAccount = results.first();
    }
    else
    {
        m_activeAccount = results.last();
    }

    INFO << "Selected account:" << m_activeAccount.getAccountId()
         << "for mode:" << (MainApp::getTradingMode() == TradingMode::Sim ? "SIM" : "LIVE");

    // Start balance polling if not already started
    if (!m_balancePollingStarted)
    {
        startBalancePolling();
        m_balancePollingStarted = true;
    }

    // Only initialize position stream once
    if (positionStreamStarted)
    {
        DEBUG << "Position stream already started, skipping initialization";
    }
    else
    {
        m_positionReceiver = new PositionsReceiver(m_activeAccount.getAccountId(), this);
        Q_CHECK_PTR(m_positionReceiver);
        positionStreamStarted = true;

        auto c1 = connect(m_positionReceiver,
                          &PositionsReceiver::receivedNewPosition,
                          this,
                          &MainAlgo::onReceivedNewPosition,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c1);

        auto c2 = connect(m_positionReceiver,
                          &PositionsReceiver::positionDeleted,
                          this,
                          &MainAlgo::onPositionDeleted,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c2);

        auto c3 = connect(m_positionReceiver,
                          &PositionsReceiver::loadedPositionsFromDatabase,
                          this,
                          &MainAlgo::onLoadedPositionsFromDatabase,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c3);

        m_positionReceiver->emitLoadedPositionsFromDatabase();
    }

    // Only initialize order stream once
    if (orderStreamStarted)
    {
        DEBUG << "Order stream already started, skipping initialization";
    }
    else
    {
        m_orderReceiver = new OrdersReceiver(m_activeAccount.getAccountId(), this);
        Q_CHECK_PTR(m_orderReceiver);
        orderStreamStarted = true;

        auto c4 = connect(m_orderReceiver,
                          &OrdersReceiver::receivedNewOrder,
                          this,
                          &MainAlgo::onReceivedNewOrder,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c4);

        auto c5 = connect(m_orderReceiver,
                          &OrdersReceiver::loadedOrdersFromDatabase,
                          this,
                          &MainAlgo::onLoadedOrdersFromDatabase,
                          Qt::UniqueConnection);
        OBJ_ASSUME_TRUE(c5);

        m_orderReceiver->emitLoadedOrdersFromDatabase();
    }

    subscribeExistingLiveSymbols();

    emit tradeStationAccountsReceived(results);
}

void MainAlgo::onReceivedNewPosition(const QString& account, Position position)
{
    //DEBUG << "Received new position:" << position.toJsonString();
    const QString positionID = position.getPositionID().trimmed();
    std::optional<Position> previousPositionSnapshot;
    if (!positionID.isEmpty())
    {
        const auto previousPositionIt = m_currentPositions.constFind(positionID);
        if (previousPositionIt != m_currentPositions.constEnd())
        {
            previousPositionSnapshot = previousPositionIt.value();
        }
    }
    const bool hadPreviousPosition = previousPositionSnapshot.has_value();
    const bool isNewPosition = !positionID.isEmpty() && !hadPreviousPosition;
    const QString positionMapKey = positionID.isEmpty() ? position.getPositionID() : positionID;

    m_currentPositions[positionMapKey] = position;

    const auto currentQuantity = parseAbsoluteShareCount(position.getQuantity());
    const bool hasOpenShares = currentQuantity.has_value() && currentQuantity.value() > 0;

    bool hasIncreasedExposure = false;
    if (hadPreviousPosition)
    {
        const Position& previousPosition = previousPositionSnapshot.value();
        const auto previousQuantity = parseAbsoluteShareCount(previousPosition.getQuantity());
        const QString previousSide = previousPosition.getLongShort().trimmed().toUpper();
        const QString currentSide = position.getLongShort().trimmed().toUpper();
        hasIncreasedExposure = previousQuantity.has_value() && currentQuantity.has_value() && !previousSide.isEmpty() &&
                               previousSide == currentSide && currentQuantity.value() > previousQuantity.value();
    }

    const bool shouldAttemptEntryReconciliation =
        position.isPositionUpdate() && hasOpenShares && (isNewPosition || hasIncreasedExposure);
    if (shouldAttemptEntryReconciliation)
    {
        reconcilePendingEntryOrderFromPosition(account, position);
    }

    if (m_riskManager)
    {
        if (!hasOpenShares && position.getRealizedProfitLoss().has_value() &&
            (!previousPositionSnapshot.has_value() || !previousPositionSnapshot->getRealizedProfitLoss().has_value() ||
             !qFuzzyCompare(1.0 + previousPositionSnapshot->getRealizedProfitLoss().value(),
                            1.0 + position.getRealizedProfitLoss().value())))
        {
            m_riskManager->onPositionClosed(position, MainApp::getCurrentAppTime());
        }
        m_riskManager->updateOpenPositionsCount(account,
                                                getOpenPositionCountForAccount(account),
                                                MainApp::getCurrentAppTime());
        refreshRiskStatusForAccount(account);
    }
    monitorManagedBrackets();
    emit receivedNewPosition(account, position);
}

void MainAlgo::reconcilePendingEntryOrderFromPosition(const QString& p_account, const Position& p_position)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const QString symbol = p_position.getSymbol().trimmed().toUpper();
    if (symbol.isEmpty())
    {
        return;
    }

    const QString positionSide = p_position.getLongShort().trimmed();
    if (positionSide.isEmpty())
    {
        return;
    }

    const QString accountFromPosition = p_position.getAccountID().trimmed().toUpper();
    const QString fallbackAccount = p_account.trimmed().toUpper();
    const QString accountID = !accountFromPosition.isEmpty() ? accountFromPosition : fallbackAccount;
    if (accountID.isEmpty())
    {
        return;
    }

    const auto positionQuantity = parseAbsoluteShareCount(p_position.getQuantity());
    if (!positionQuantity.has_value())
    {
        return;
    }

    if (m_latestOrdersById.isEmpty())
    {
        return;
    }

    const QDateTime positionTimestamp =
        p_position.getTimestamp().isValid() ? p_position.getTimestamp() : MainApp::getCurrentAppTime();
    constexpr qint64 kEntryReconciliationMaxSkewMs = 10 * 60 * 1000;
    std::optional<Order> bestCandidate;
    bool bestQuantityExact = false;
    qint64 bestSkewMs = std::numeric_limits<qint64>::max();

    for (auto it = m_latestOrdersById.constBegin(); it != m_latestOrdersById.constEnd(); ++it)
    {
        const Order& order = it.value();
        if (isTerminalOrderStatus(order.getOrderStatus()))
        {
            continue;
        }

        if (order.getSymbol().trimmed().toUpper() != symbol)
        {
            continue;
        }

        const QString orderAccountID = order.getAccountID().trimmed().toUpper();
        if (!orderAccountID.isEmpty() && orderAccountID != accountID)
        {
            continue;
        }

        if (!isOpenActionForPosition(order.getTradeAction(), positionSide))
        {
            continue;
        }

        const auto orderQuantity = parsePositiveShareCount(order.getQuantity());
        if (!orderQuantity.has_value())
        {
            continue;
        }

        const bool quantityExactMatch = orderQuantity.value() == positionQuantity.value();
        const bool quantityCompatible = orderQuantity.value() <= positionQuantity.value();
        if (!quantityCompatible)
        {
            continue;
        }

        const QDateTime openedDateTime = order.getOpenedDateTime();
        qint64 skewMs = std::numeric_limits<qint64>::max();
        if (openedDateTime.isValid() && positionTimestamp.isValid())
        {
            const qint64 deltaMs = openedDateTime.msecsTo(positionTimestamp);
            skewMs = deltaMs >= 0 ? deltaMs : -deltaMs;
            if (skewMs > kEntryReconciliationMaxSkewMs)
            {
                continue;
            }
        }

        if (!bestCandidate.has_value())
        {
            bestCandidate = order;
            bestQuantityExact = quantityExactMatch;
            bestSkewMs = skewMs;
            continue;
        }

        const bool shouldReplace = (quantityExactMatch && !bestQuantityExact) ||
                                   (quantityExactMatch == bestQuantityExact && skewMs < bestSkewMs) ||
                                   (quantityExactMatch == bestQuantityExact && skewMs == bestSkewMs &&
                                    openedDateTime.isValid() && openedDateTime > bestCandidate->getOpenedDateTime());
        if (shouldReplace)
        {
            bestCandidate = order;
            bestQuantityExact = quantityExactMatch;
            bestSkewMs = skewMs;
        }
    }

    if (!bestCandidate.has_value())
    {
        return;
    }

    Order syntheticFilledOrder = *bestCandidate;
    syntheticFilledOrder.m_fillIsSynthetic = true;
    syntheticFilledOrder.m_orderStatus = Order::Status::FLL;
    syntheticFilledOrder.m_statusDescription = Order::getStatusDescriptionForStatus(Order::Status::FLL);
    syntheticFilledOrder.m_filledPrice = bestEffortSyntheticEntryFillPrice(syntheticFilledOrder, p_position);

    const QDateTime reconciliationNow = MainApp::getCurrentAppTime();
    QDateTime syntheticFillTimestamp = positionTimestamp.isValid() ? positionTimestamp : reconciliationNow;
    const QDateTime openedDateTime = syntheticFilledOrder.getOpenedDateTime();
    if (!syntheticFillTimestamp.isValid())
    {
        syntheticFillTimestamp = openedDateTime;
    }
    if (openedDateTime.isValid() && syntheticFillTimestamp.isValid() && syntheticFillTimestamp < openedDateTime)
    {
        // TradeStation position timestamps can remain fixed at first-open time while quantity increases.
        // Ensure synthetic fills never predate the order open time.
        syntheticFillTimestamp =
            (reconciliationNow.isValid() && reconciliationNow >= openedDateTime) ? reconciliationNow : openedDateTime;
    }

    syntheticFilledOrder.m_closedDateTime = syntheticFillTimestamp;

    std::optional<qint64> latencyMs;
    if (openedDateTime.isValid() && syntheticFillTimestamp.isValid())
    {
        latencyMs = openedDateTime.msecsTo(syntheticFillTimestamp);
    }

    OrdersDatabase* const ordersDb = OrdersDatabase::getInstance();
    if (ordersDb == nullptr || !ordersDb->isOpen())
    {
        WARNING << "Failed to persist synthetic entry fill reconciliation for order"
                << syntheticFilledOrder.getOrderID() << "because OrdersDatabase is unavailable";
    }
    else if (!ordersDb->updateOrder(syntheticFilledOrder, latencyMs))
    {
        WARNING << "Failed to persist synthetic entry fill reconciliation for order"
                << syntheticFilledOrder.getOrderID() << "positionID=" << p_position.getPositionID();
    }

    INFO << "Reconciled entry-order to filled from position update: orderID=" << syntheticFilledOrder.getOrderID()
         << "positionID=" << p_position.getPositionID() << "symbol=" << symbol
         << "fillPrice=" << syntheticFilledOrder.getFilledPrice()
         << "fillTs=" << syntheticFilledOrder.getClosedDateTime().toString(Qt::ISODate);

    const QString routedAccount =
        !syntheticFilledOrder.getAccountID().trimmed().isEmpty() ? syntheticFilledOrder.getAccountID() : accountID;
    onReceivedNewOrder(routedAccount, syntheticFilledOrder);
}

void MainAlgo::onPositionDeleted(const QString& account, const QString& positionID)
{
    DEBUG << "Position deleted:" << positionID;
    std::optional<Position> closedPosition;
    auto positionIt = m_currentPositions.find(positionID);
    if (positionIt != m_currentPositions.end())
    {
        closedPosition = *positionIt;
    }
    m_currentPositions.remove(positionID);
    const QDateTime closedTs = MainApp::getCurrentAppTime();

    if (m_riskManager)
    {
        const QString riskAccountId = !account.trimmed().isEmpty()
                                          ? account
                                          : (closedPosition.has_value() ? closedPosition->getAccountID() : QString());
        m_riskManager->updateOpenPositionsCount(riskAccountId, getOpenPositionCountForAccount(riskAccountId), closedTs);
        refreshRiskStatusForAccount(riskAccountId);
    }

    if (closedPosition.has_value())
    {
        prunePendingClosedPositionReconciliation();

        const QString closedPositionId = !closedPosition->getPositionID().trimmed().isEmpty()
                                             ? closedPosition->getPositionID().trimmed()
                                             : positionID.trimmed();
        const QString normalizedAccount =
            !account.trimmed().isEmpty() ? account.trimmed() : closedPosition->getAccountID().trimmed();

        bool reconciledCloseOrder = false;
        auto trackedIt = m_pendingCloseOrderIdsByPositionId.find(closedPositionId);
        if (trackedIt != m_pendingCloseOrderIdsByPositionId.end())
        {
            const QSet<QString> trackedOrderIds = *trackedIt;
            m_pendingCloseOrderIdsByPositionId.erase(trackedIt);

            if (!trackedOrderIds.isEmpty())
            {
                auto trackedCandidate = findBestCloseOrderReconciliationCandidate(m_latestOrdersById,
                                                                                  closedPosition.value(),
                                                                                  normalizedAccount,
                                                                                  closedTs,
                                                                                  &trackedOrderIds);
                if (trackedCandidate.has_value())
                {
                    reconciledCloseOrder = reconcileCloseOrderToFilled(normalizedAccount,
                                                                       closedPositionId,
                                                                       closedPosition.value(),
                                                                       closedTs,
                                                                       trackedCandidate.value(),
                                                                       "tracked-close-order");
                }
                else
                {
                    WARNING << "Tracked close-order reconciliation did not find a matching in-memory order for"
                            << closedPositionId << "trackedIDs=" << trackedOrderIds.values();
                }
            }
        }

        if (!reconciledCloseOrder)
        {
            auto fallbackCandidate = findBestCloseOrderReconciliationCandidate(m_latestOrdersById,
                                                                               closedPosition.value(),
                                                                               normalizedAccount,
                                                                               closedTs,
                                                                               nullptr);
            if (fallbackCandidate.has_value())
            {
                reconciledCloseOrder = reconcileCloseOrderToFilled(normalizedAccount,
                                                                   closedPositionId,
                                                                   closedPosition.value(),
                                                                   closedTs,
                                                                   fallbackCandidate.value(),
                                                                   "fallback-scan");
            }
        }

        if (closedPositionId.isEmpty())
        {
            WARNING << "Unable to defer close-order reconciliation because closed position ID is empty";
        }
        else if (!reconciledCloseOrder)
        {
            m_pendingClosedPositionsByPositionId.insert(
                closedPositionId,
                PendingClosedPositionReconciliation{closedPosition.value(), normalizedAccount, closedTs});
            DEBUG << "Deferred close-order reconciliation awaiting late order update: positionID=" << closedPositionId
                  << "symbol=" << closedPosition->getSymbol() << "account=" << normalizedAccount;
        }
        else
        {
            m_pendingClosedPositionsByPositionId.remove(closedPositionId);
        }
    }

    monitorManagedBrackets();
    emit positionDeleted(account, positionID);
}

void MainAlgo::prunePendingClosedPositionReconciliation()
{
    if (m_pendingClosedPositionsByPositionId.isEmpty())
    {
        return;
    }

    constexpr qint64 kPendingCloseRetentionMs = 2 * 60 * 1000;
    const QDateTime now = MainApp::getCurrentAppTime();

    for (auto it = m_pendingClosedPositionsByPositionId.begin(); it != m_pendingClosedPositionsByPositionId.end();)
    {
        const QDateTime closedTimestamp = it->closedTimestamp;
        const bool invalidTimestamp = !closedTimestamp.isValid();
        const qint64 ageMs = invalidTimestamp ? (kPendingCloseRetentionMs + 1) : closedTimestamp.msecsTo(now);
        const bool expired = ageMs < 0 || ageMs > kPendingCloseRetentionMs;
        if (expired)
        {
            DEBUG << "Dropping stale pending close-order reconciliation snapshot: positionID=" << it.key()
                  << "ageMs=" << ageMs;
            it = m_pendingClosedPositionsByPositionId.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool MainAlgo::reconcileCloseOrderToFilled(const QString& p_defaultAccount,
                                           const QString& p_positionId,
                                           const Position& p_closedPosition,
                                           const QDateTime& p_closedTimestamp,
                                           Order p_order,
                                           const QString& p_reconciliationContext)
{
    if (isTerminalOrderStatus(p_order.getOrderStatus()))
    {
        return false;
    }

    if (!isCloseActionForPosition(p_order.getTradeAction(), p_closedPosition.getLongShort()))
    {
        WARNING << "Close-order reconciliation skipped order" << p_order.getOrderID() << "for position" << p_positionId
                << "because trade action" << p_order.getTradeAction() << "does not close a"
                << p_closedPosition.getLongShort() << "position";
        return false;
    }

    const auto closedQuantity = parseAbsoluteShareCount(p_closedPosition.getQuantity());
    const auto orderQuantity = parsePositiveShareCount(p_order.getQuantity());
    if (closedQuantity.has_value() && orderQuantity.has_value() && orderQuantity.value() != closedQuantity.value())
    {
        WARNING << "Close-order reconciliation quantity mismatch for order" << p_order.getOrderID()
                << "positionID=" << p_positionId << "expected=" << closedQuantity.value()
                << "got=" << orderQuantity.value();
        return false;
    }

    const QDateTime closeTimestamp = p_closedTimestamp.isValid() ? p_closedTimestamp : MainApp::getCurrentAppTime();
    p_order.m_fillIsSynthetic = true;
    p_order.m_orderStatus = Order::Status::FLL;
    p_order.m_statusDescription = Order::getStatusDescriptionForStatus(Order::Status::FLL);

    QString syntheticFillPriceSource;
    p_order.m_filledPrice = bestEffortSyntheticFillPrice(p_order, p_closedPosition, &syntheticFillPriceSource);
    p_order.m_closedDateTime = closeTimestamp;

    std::optional<qint64> latencyMs;
    if (p_order.getOpenedDateTime().isValid())
    {
        latencyMs = p_order.getOpenedDateTime().msecsTo(closeTimestamp);
    }

    OrdersDatabase* const ordersDb = OrdersDatabase::getInstance();
    if (ordersDb == nullptr || !ordersDb->isOpen())
    {
        WARNING << "Failed to persist synthetic fill reconciliation for order" << p_order.getOrderID()
                << "because OrdersDatabase is unavailable";
    }
    else if (!ordersDb->updateOrder(p_order, latencyMs))
    {
        WARNING << "Failed to persist synthetic fill reconciliation for order" << p_order.getOrderID()
                << "positionID=" << p_positionId;
    }

    DEBUG << "Reconciled close-order to filled (" << p_reconciliationContext << "): orderID=" << p_order.getOrderID()
          << "positionID=" << p_positionId << "symbol=" << p_order.getSymbol()
          << "fillPrice=" << p_order.getFilledPrice() << "source=" << syntheticFillPriceSource;

    const QString routedAccount =
        !p_order.getAccountID().trimmed().isEmpty() ? p_order.getAccountID() : p_defaultAccount.trimmed();
    onReceivedNewOrder(routedAccount, p_order);
    return true;
}

void MainAlgo::onLoadedPositionsFromDatabase(const QString& account, QMap<QString, Position> positions)
{
    INFO << "Loading" << positions.size() << "positions from database for account" << account;

    // Emit each loaded position to the frontend
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it)
    {
        const Position& position = it.value();
        m_currentPositions[position.getPositionID()] = position;
        DEBUG << "Emitting loaded position:" << position.getPositionID();
        emit receivedNewPosition(account, position);
    }

    if (m_riskManager)
    {
        m_riskManager->updateOpenPositionsCount(account,
                                                getOpenPositionCountForAccount(account),
                                                MainApp::getCurrentAppTime());
        refreshRiskStatusForAccount(account);
    }

    monitorManagedBrackets();
}

void MainAlgo::onLoadedOrdersFromDatabase(const QString& account,
                                          QMap<QString, std::tuple<Order, std::optional<qint64>>> p_ordersById)
{
    INFO << "Loading" << p_ordersById.size() << "orders from database for account" << account;

    m_latestOrdersById.clear();
    m_pendingCloseOrderIdsByPositionId.clear();
    m_pendingClosedPositionsByPositionId.clear();

    QVector<Order> orderedByOpenedTime;
    orderedByOpenedTime.reserve(p_ordersById.size());
    for (auto it = p_ordersById.constBegin(); it != p_ordersById.constEnd(); ++it)
    {
        orderedByOpenedTime.append(std::get<0>(it.value()));
    }

    std::sort(orderedByOpenedTime.begin(),
              orderedByOpenedTime.end(),
              [](const Order& p_left, const Order& p_right)
              { return p_left.getOpenedDateTime() < p_right.getOpenedDateTime(); });

    for (const Order& order: orderedByOpenedTime)
    {
        m_latestOrdersById.insert(order.getOrderID(), order);
        const QString routedAccount = order.getAccountID().trimmed().isEmpty() ? account : order.getAccountID();
        emit receivedNewOrder(routedAccount, order);
    }
}

void MainAlgo::onReceivedNewOrder(const QString& account, Order order)
{
    // Reconciliation reads persisted snapshots and excludes synthetic fill estimates.
    if (order.getFilledPrice() > 0.0)
    {
        OBJ_ASSUME_DIFF(m_positionReceiver, nullptr);
        m_positionReceiver->reconcileClosedPositions(order.getSymbol());
    }
    const QString orderID = order.getOrderID();
    if (!orderID.trimmed().isEmpty())
    {
        m_latestOrdersById.insert(orderID, order);
    }

    DEBUG << "onReceivedNewOrder: orderID=" << orderID << "status=" << static_cast<int>(order.getOrderStatus())
          << "mappings_size=" << m_orderMappings.size();

    handleManagedBracketOrderUpdate(order);

    const QString riskAccountId = !order.getAccountID().trimmed().isEmpty() ? order.getAccountID() : account;
    const bool fillLikeStatus = isFillLikeStatus(order.getOrderStatus());
    if (m_riskManager && fillLikeStatus)
    {
        auto entryOrderIt = m_orderIdToRiskEntryCandidate.find(orderID);
        if (entryOrderIt != m_orderIdToRiskEntryCandidate.end() && *entryOrderIt &&
            !m_countedRiskEntryFillOrderIds.contains(orderID))
        {
            m_countedRiskEntryFillOrderIds.insert(orderID);
            m_riskManager->onEntryOrderFirstFill(riskAccountId, orderID, MainApp::getCurrentAppTime());
            refreshRiskStatusForAccount(riskAccountId);
        }
    }

    // Attach strategy log for the full lifetime of the order.
    // Persist to DB on first arrival; keep in memory until the order is terminal
    // so that every subsequent update (e.g. Filled) also carries the log.
    auto logIt = m_orderIdToLog.find(orderID);
    if (logIt != m_orderIdToLog.end())
    {
        order.setStrategyLog(*logIt);
        m_deferredOrderUpdates.remove(orderID);

        // Persist only on the first update (ACK/OPN) — idempotent but saves extra queries
        const Order::Status status = order.getOrderStatus();
        if (status == Order::Status::ACK || status == Order::Status::OPN)
        {
            OrdersDatabase::getInstance()->updateOrderStrategyLog(orderID, *logIt);
        }

        // Remove from map only when the order is in a terminal state
        const bool isTerminal =
            (status == Order::Status::FLL || status == Order::Status::FLP || status == Order::Status::FPR ||
             status == Order::Status::CAN || status == Order::Status::UCN || status == Order::Status::TSC ||
             status == Order::Status::REJ || status == Order::Status::EXP || status == Order::Status::OUT ||
             status == Order::Status::DON);
        if (isTerminal)
        {
            m_orderIdToLog.erase(logIt);
            m_orderIdToRiskEntryCandidate.remove(orderID);
            m_countedRiskEntryFillOrderIds.remove(orderID);
        }
    }
    else if (!m_pendingOrderLogs.isEmpty())
    {
        // The stream can beat the async place-order ACK callback to the MainAlgo thread.
        // Keep the first unannotated update so we can replay it once the requestId→orderId
        // log binding is known.
        m_deferredOrderUpdates.insert(orderID, DeferredOrderUpdate{account, order});
    }

    const Order::Status orderStatus = order.getOrderStatus();
    if (isTerminalOrderStatus(orderStatus))
    {
        m_orderIdToRiskEntryCandidate.remove(orderID);
        m_countedRiskEntryFillOrderIds.remove(orderID);

        for (auto trackedIt = m_pendingCloseOrderIdsByPositionId.begin();
             trackedIt != m_pendingCloseOrderIdsByPositionId.end();)
        {
            trackedIt->remove(orderID);
            if (trackedIt->isEmpty())
            {
                trackedIt = m_pendingCloseOrderIdsByPositionId.erase(trackedIt);
            }
            else
            {
                ++trackedIt;
            }
        }
    }

    // Emit enriched order to FrontEnd (with strategy log attached if available)
    emit receivedNewOrder(account, order);

    if (!isTerminalOrderStatus(orderStatus) && !orderID.trimmed().isEmpty() &&
        !m_pendingClosedPositionsByPositionId.isEmpty())
    {
        prunePendingClosedPositionReconciliation();
        if (!m_pendingClosedPositionsByPositionId.isEmpty())
        {
            const QSet<QString> allowedOrderIds = {orderID.trimmed()};
            for (auto pendingIt = m_pendingClosedPositionsByPositionId.begin();
                 pendingIt != m_pendingClosedPositionsByPositionId.end();)
            {
                const QString pendingPositionId = pendingIt.key();
                const PendingClosedPositionReconciliation pendingSnapshot = pendingIt.value();
                const QString pendingAccount =
                    !pendingSnapshot.accountID.trimmed().isEmpty() ? pendingSnapshot.accountID : account.trimmed();
                auto candidate = findBestCloseOrderReconciliationCandidate(m_latestOrdersById,
                                                                           pendingSnapshot.position,
                                                                           pendingAccount,
                                                                           pendingSnapshot.closedTimestamp,
                                                                           &allowedOrderIds);
                if (!candidate.has_value())
                {
                    ++pendingIt;
                    continue;
                }

                pendingIt = m_pendingClosedPositionsByPositionId.erase(pendingIt);
                reconcileCloseOrderToFilled(pendingAccount,
                                            pendingPositionId,
                                            pendingSnapshot.position,
                                            pendingSnapshot.closedTimestamp,
                                            candidate.value(),
                                            "late-order-update");
                break;
            }
        }
    }

    // Route to the strategy that placed this order
    auto strategyIt = m_orderMappings.find(orderID);
    if (strategyIt == m_orderMappings.end())
    {
        DEBUG << "Received non-strategy order update for order ID:" << orderID;
        return;
    }

    QString strategyID = *strategyIt;
    DEBUG << "Routing order update for orderID=" << orderID << "to strategyID=" << strategyID;
    QMetaObject::invokeMethod(
        m_strategyManager.get(),
        [this, strategyID, order]() { m_strategyManager->onOrderUpdatedForStrategy(strategyID, order); },
        Qt::QueuedConnection);
}

void MainAlgo::startBalancePolling()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    m_balancePollingTimer->start(PollingConstants::BALANCE_POLLING_INTERVAL_MS);
    requestBalance(); // initial request
    DEBUG << "Started balance polling";
}

void MainAlgo::stopBalancePolling()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    m_balancePollingTimer->stop();
    DEBUG << "Stopped balance polling";
}

[[nodiscard]] Balance MainAlgo::getCurrentBalance() const
{
    return m_currentBalance;
}

[[nodiscard]] QString MainAlgo::getActiveAccountId() const
{
    return m_activeAccount.getAccountId();
}

[[nodiscard]] QVector<Position> MainAlgo::getCurrentPositionsSnapshot() const
{
    return m_currentPositions.values().toVector();
}

int MainAlgo::getSignedNetPositionSharesForAccountSymbol(const QString& p_accountID, const QString& p_symbol) const
{
    const QString accountId = p_accountID.trimmed().toUpper();
    const QString symbol = p_symbol.trimmed().toUpper();
    if (accountId.isEmpty() || symbol.isEmpty())
    {
        return 0;
    }

    int netShares = 0;
    for (auto it = m_currentPositions.constBegin(); it != m_currentPositions.constEnd(); ++it)
    {
        const Position& position = it.value();
        if (position.getAccountID().trimmed().toUpper() != accountId)
        {
            continue;
        }
        if (position.getSymbol().trimmed().toUpper() != symbol)
        {
            continue;
        }
        if (position.isDeleted())
        {
            continue;
        }
        netShares += signedPositionShares(position);
    }

    return netShares;
}

int MainAlgo::getOpenPositionCountForAccount(const QString& p_accountID) const
{
    const QString accountId = p_accountID.trimmed().toUpper();
    if (accountId.isEmpty())
    {
        return 0;
    }

    QSet<QString> openSymbols;
    for (auto it = m_currentPositions.constBegin(); it != m_currentPositions.constEnd(); ++it)
    {
        const Position& position = it.value();
        if (position.getAccountID().trimmed().toUpper() != accountId)
        {
            continue;
        }
        if (position.isDeleted())
        {
            continue;
        }
        if (signedPositionShares(position) == 0)
        {
            continue;
        }
        openSymbols.insert(position.getSymbol().trimmed().toUpper());
    }

    return openSymbols.size();
}

std::optional<double> MainAlgo::resolveOrderRiskReferencePrice(const PlaceOrderRequest& p_orderRequest) const
{
    if (p_orderRequest.getLimitPrice().has_value())
    {
        const double limit = p_orderRequest.getLimitPrice().value();
        if (limit > 0.0)
        {
            return limit;
        }
    }

    if (p_orderRequest.getOrderType().type == OrderType::Type::StopMarket && p_orderRequest.getStopPrice().has_value())
    {
        const double stop = p_orderRequest.getStopPrice().value();
        if (stop > 0.0)
        {
            return stop;
        }
    }

    const std::expected<MarketDataSnapshot, QString> snapshot =
        const_cast<MainAlgo*>(this)->getMarketDataSnapshot(p_orderRequest.getSymbol(), 1);
    if (!snapshot.has_value() || !snapshot->latestLevel2.has_value())
    {
        return std::nullopt;
    }

    const Level2 level2 = snapshot->latestLevel2.value();
    const bool isBuySide = isBuySideTradeAction(p_orderRequest.getTradeAction());
    if (isBuySide)
    {
        for (const Level2Row& ask: level2.m_asks)
        {
            if (ask.m_price > 0.0)
            {
                return ask.m_price;
            }
        }
    }
    else
    {
        for (const Level2Row& bid: level2.m_bids)
        {
            if (bid.m_price > 0.0)
            {
                return bid.m_price;
            }
        }
    }

    return std::nullopt;
}

std::optional<double> MainAlgo::resolveOrderRiskStopPrice(const PlaceOrderRequest& p_orderRequest) const
{
    if (p_orderRequest.getStopPrice().has_value() && p_orderRequest.getStopPrice().value() > 0.0)
    {
        return p_orderRequest.getStopPrice().value();
    }

    const QString accountId = p_orderRequest.getAccountID().trimmed().toUpper();
    const QString symbol = p_orderRequest.getSymbol().trimmed().toUpper();
    if (accountId.isEmpty() || symbol.isEmpty())
    {
        return std::nullopt;
    }

    const QString key = managedBracketKey(accountId, symbol);
    auto bracketIt = m_managedBrackets.constFind(key);
    if (bracketIt == m_managedBrackets.constEnd())
    {
        return std::nullopt;
    }

    if (bracketIt->stopPrice <= 0.0)
    {
        return std::nullopt;
    }

    return bracketIt->stopPrice;
}

void MainAlgo::refreshRiskStatusForAccount(const QString& p_accountID)
{
    const QString accountId = p_accountID.trimmed().toUpper();
    if (!accountId.isEmpty())
    {
        emit riskStatusChanged(accountId);
    }
}

[[nodiscard]] bool MainAlgo::hasManagedBracketForAccountSymbol(const QString& p_accountID,
                                                               const QString& p_symbol) const
{
    const QString key = managedBracketKey(p_accountID, p_symbol);
    return m_managedBrackets.contains(key);
}

[[nodiscard]] QString MainAlgo::getDisplayedSymbol() const
{
    if (m_currentDisplayedSymbolContext)
    {
        return m_currentDisplayedSymbolContext->symbol;
    }
    return QString();
}

[[nodiscard]] QPointer<SymbolContext> MainAlgo::getDisplayedSymbolContext() const
{
    return m_currentDisplayedSymbolContext;
}

void MainAlgo::requestBalance()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    // Don't request balance if not authenticated — TSClient will assert on empty API key.
    // Exception: in replay mode, the mock network manager handles requests without real credentials.
    if (!TSClient::getInstance()->isAuthenticated() && TSClient::getInstance()->getMode() != TSClient::Mode::Replay)
        return;

    OBJ_ASSUME_FALSE(m_activeAccount.getAccountId().isEmpty());


    QFuture<std::expected<QVector<Balance>, TSClient::Error>> balanceFuture =
        TSClient::getInstance()->getBalances(QStringList(m_activeAccount.getAccountId()));

    QPointer<MainAlgo> self(this);
    balanceFuture.then(
        [self](std::expected<QVector<Balance>, TSClient::Error> results) mutable
        {
            if (!self)
            {
                return;
            }

            const bool invoked = QMetaObject::invokeMethod(
                self,
                [self, results = std::move(results)]() mutable
                {
                    if (!self)
                    {
                        return;
                    }

                    if (results.has_value())
                    {
                        // DEBUG << "getBalances() succeeded with" << results.value().size() << "balances";
                        self->onBalanceReceived(results.value());
                        return;
                    }

                    // TODO do something smarter with errors
                    qCCritical(MainAlgoLog)
                        << self->objectName() << "getBalances() failed with" << QtEnum::toString(results.error());
                },
                Qt::QueuedConnection);
            if (!invoked)
            {
                qCWarning(MainAlgoLog)
                    << "MainAlgo" << "Dropping balance response: failed to dispatch continuation to MainAlgo thread";
            }
        });
}

void MainAlgo::onBalanceReceived(const QVector<Balance>& results)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_EQUAL(results.size(), 1);

    m_currentBalance = results.at(0);
    if (m_riskManager)
    {
        m_riskManager->onBalanceUpdate(m_currentBalance, MainApp::getCurrentAppTime());
        refreshRiskStatusForAccount(m_currentBalance.getAccountID());
    }

    // Emit signal for the UI or other components interested
    emit balanceUpdated(m_currentBalance);
}

SymbolContext::SymbolContext(const QString& p_symbol, QObject* p_parent)
    : QObject(p_parent)
    , symbol(p_symbol)
    , barCache(p_symbol, this)
    , barReceiver(p_symbol, this)
    , m_level2Receiver(p_symbol, this)
    , m_liveBarAccumulator(this, 60)
    , m_barAggregator(this)
{
    this->setObjectName("SymbolContext::" + p_symbol);

    // All internal connections use Qt::DirectConnection so they execute on the
    // pool thread during drain(). This is safe because drain guarantees only one
    // pool thread accesses a symbol's internals at a time.

    // Connect BarReceiver to BarCache for 1m bar storage
    bool connected = connect(
        &barReceiver,
        &BarReceiver::receivedNewBar,
        &barCache,
        [this](const QString&, const Bar& bar) { barCache.storeBar(TimeFrame::ONE_MINUTE, bar); },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire LiveBarAccumulator::barClosed → BarReceiver::receivedNewBar
    connected = connect(&m_liveBarAccumulator,
                        &LiveBarAccumulator::barClosed,
                        &barReceiver,
                        &BarReceiver::receivedNewBar,
                        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire LiveBarAccumulator::barUpdated → BarReceiver::receivedNewBar (in-progress candle)
    connected = connect(&m_liveBarAccumulator,
                        &LiveBarAccumulator::barUpdated,
                        &barReceiver,
                        &BarReceiver::receivedNewBar,
                        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire closed 1m bars → BarAggregator for higher-TF accumulation (OHLCV + period-close detection)
    connected = connect(&m_liveBarAccumulator,
                        &LiveBarAccumulator::barClosed,
                        &m_barAggregator,
                        &BarAggregator::onNewBar,
                        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire in-progress 1m bar updates → BarAggregator for real-time live candle animation
    connected = connect(&m_liveBarAccumulator,
                        &LiveBarAccumulator::barUpdated,
                        &m_barAggregator,
                        &BarAggregator::onBarUpdated,
                        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Wire BarAggregator::barClosed → BarCache for higher-TF storage
    connected =
        connect(&m_barAggregator, &BarAggregator::barClosed, &barCache, &BarCache::storeBar, Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // ── Always-populate DisplaySnapshot ──────────────────────────────────
    // Every SymbolContext writes to its own DisplaySnapshot regardless of whether
    // it is the "currently displayed" symbol. This allows any chart window to read
    // from any SymbolContext's snapshot at 30 Hz with zero extra wiring.

    // 1m bar → snapshot
    connected = connect(
        &barReceiver,
        &BarReceiver::receivedNewBar,
        this,
        [this](const QString&, const Bar& bar)
        {
            LTTnG_TP(opentraderplatform, snapshot_write, symbol.toUtf8().constData(), "bar");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.latestBar = bar;
            m_displaySnapshot.barDirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // L2 → snapshot
    connected = connect(
        &m_level2Receiver,
        &Level2Receiver::receivedNewLevel2,
        this,
        [this](const QString&, const Level2& level2)
        {
            LTTnG_TP(opentraderplatform, snapshot_write, symbol.toUtf8().constData(), "l2");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.latestLevel2 = level2;
            m_displaySnapshot.l2Dirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // Higher-TF aggregator → snapshot
    connected = connect(
        &m_barAggregator,
        &BarAggregator::barUpdated,
        this,
        [this](TimeFrame tf, const Bar& bar)
        {
            LTTnG_TP(opentraderplatform, snapshot_write, symbol.toUtf8().constData(), "aggregator");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.aggregatorBars[tf] = bar;
            m_displaySnapshot.aggregatorDirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    connected = connect(
        &m_barAggregator,
        &BarAggregator::barClosed,
        this,
        [this](TimeFrame tf, const Bar& bar)
        {
            LTTnG_TP(opentraderplatform, snapshot_write, symbol.toUtf8().constData(), "aggregator");
            QWriteLocker lock(&m_displaySnapshot.lock);
            m_displaySnapshot.aggregatorBars[tf] = bar;
            m_displaySnapshot.aggregatorDirty = true;
        },
        Qt::DirectConnection);
    OBJ_ASSUME_TRUE(connected);

    // DBClient wiring is handled centrally by MainAlgo routing (onNewLevel2Received / onNewTradeReceived).
    // Live subscription is also managed by MainAlgo when creating the SymbolContext.

    DEBUG << "New instance";
}

SymbolContext::~SymbolContext()
{
    // Signal that we're destroying — drain() will exit early on pending items
    m_destroying.store(true, std::memory_order_release);

    // Wait for any running drain to complete
    LTTnG_TP(opentraderplatform, symbolctx_shutdown_wait, symbol.toUtf8().constData());
    QMutexLocker lock(&m_queueMutex);
    while (m_draining.load(std::memory_order_acquire))
    {
        m_drainDone.wait(&m_queueMutex);
    }

    TSClient* tsClient = TSClient::getInstance();
    if (!m_streamBars.isNull())
    {
        tsClient->closeStream(m_streamBars);
        m_streamBars = nullptr;
    }
    if (!m_streamMarketDepthAggregate.isNull())
    {
        tsClient->closeStream(m_streamMarketDepthAggregate);
        m_streamMarketDepthAggregate = nullptr;
    }
    if (!m_streamQuote.isNull())
    {
        tsClient->closeStream(m_streamQuote);
        m_streamQuote = nullptr;
    }

    DEBUG << "Deleted instance";
}

// ── Actor model: enqueue / drain ───────────────────────────────────────────

void SymbolContext::enqueueLevel2(const Level2& p_level2)
{
    [[maybe_unused]] int depth = 0;
    {
        QMutexLocker lock(&m_queueMutex);
        m_queue.enqueue(WorkItem{p_level2});
        depth = m_queue.size();
    }
    m_pendingWorkItems.fetch_add(1, std::memory_order_release);
    LTTnG_TP(opentraderplatform, symbolctx_enqueue, symbol.toUtf8().constData(), "L2", depth);
    if (!m_draining.exchange(true, std::memory_order_acq_rel))
    {
        LTTnG_TP(opentraderplatform, symbolctx_pool_submit, symbol.toUtf8().constData());
        QThreadPool::globalInstance()->start(QRunnable::create([this] { drain(); }));
    }
}

void SymbolContext::enqueueTrade(const Trade& p_trade)
{
    enqueueWorkItem(WorkItem{p_trade}, "Trade");
}

void SymbolContext::enqueueBar(const Bar& p_bar)
{
    enqueueWorkItem(WorkItem{p_bar}, "Bar");
}

void SymbolContext::enqueueWorkItem(WorkItem&& p_item, [[maybe_unused]] const char* p_kind)
{
    [[maybe_unused]] int depth = 0;
    {
        QMutexLocker lock(&m_queueMutex);
        m_queue.enqueue(std::move(p_item));
        depth = m_queue.size();
    }
    m_pendingWorkItems.fetch_add(1, std::memory_order_release);
    LTTnG_TP(opentraderplatform, symbolctx_enqueue, symbol.toUtf8().constData(), p_kind, depth);
    if (!m_draining.exchange(true, std::memory_order_acq_rel))
    {
        LTTnG_TP(opentraderplatform, symbolctx_pool_submit, symbol.toUtf8().constData());
        QThreadPool::globalInstance()->start(QRunnable::create([this] { drain(); }));
    }
}

void SymbolContext::drain()
{
    // Thread assertion: drain runs on a pool thread, never on the GUI or MainAlgo thread
    OBJ_ASSUME_DIFF(QThread::currentThread(), QCoreApplication::instance()->thread());

    LTTnG_TP(opentraderplatform, symbolctx_drain_start, symbol.toUtf8().constData());
    int itemsProcessed = 0;

    for (;;)
    {
        WorkItem item;
        {
            QMutexLocker lock(&m_queueMutex);
            if (m_queue.isEmpty())
            {
                m_draining.store(false, std::memory_order_release);
                m_drainDone.wakeAll();
                // ABA re-check: an enqueue may have happened between isEmpty() and store(false)
                if (m_queue.isEmpty())
                {
                    LTTnG_TP(opentraderplatform, symbolctx_drain_end, symbol.toUtf8().constData(), itemsProcessed);
                    return;
                }
                if (!m_draining.exchange(true, std::memory_order_acq_rel))
                {
                    LTTnG_TP(opentraderplatform, symbolctx_drain_end, symbol.toUtf8().constData(), itemsProcessed);
                    return;
                }
                continue;
            }
            item = m_queue.dequeue();
        }
        m_pendingWorkItems.fetch_sub(1, std::memory_order_release);

        if (m_destroying.load(std::memory_order_acquire))
        {
            LTTnG_TP(opentraderplatform, symbolctx_drain_end, symbol.toUtf8().constData(), itemsProcessed);
            return;
        }

        std::visit(
            [this](auto&& event)
            {
                using T = std::decay_t<decltype(event)>;
                if constexpr (std::is_same_v<T, Level2>)
                {
                    LTTnG_TP(opentraderplatform, symbolctx_process_level2, symbol.toUtf8().constData());
                    processLevel2(event);
                }
                else if constexpr (std::is_same_v<T, Trade>)
                {
                    LTTnG_TP(opentraderplatform, symbolctx_process_trade, symbol.toUtf8().constData());
                    processTrade(event);
                }
                else if constexpr (std::is_same_v<T, Bar>)
                {
                    processBar(event);
                }
            },
            item);
        ++itemsProcessed;
    }
}

void SymbolContext::processLevel2(const Level2& p_level2)
{
    m_activity.recordL2(QDateTime::currentMSecsSinceEpoch());
    m_level2Receiver.onReceivedNewLevel2(p_level2);

    QWriteLocker lock(&m_displaySnapshot.lock);
    if (!m_displaySnapshot.processedReplayTime.has_value() ||
        p_level2.m_timeStamp > *m_displaySnapshot.processedReplayTime)
    {
        m_displaySnapshot.processedReplayTime = p_level2.m_timeStamp;
    }
}

void SymbolContext::processBar(const Bar& p_bar)
{
    barReceiver.onReceivedNewBar(p_bar);

    if (p_bar.getBarStatus() == Bar::BarStatus::Closed)
    {
        m_barAggregator.onNewBar(symbol, p_bar);
    }
    else if (p_bar.getBarStatus() == Bar::BarStatus::Open)
    {
        m_barAggregator.onBarUpdated(symbol, p_bar);
    }
}

void SymbolContext::processTrade(const Trade& p_trade)
{
    m_activity.recordTrade(QDateTime::currentMSecsSinceEpoch());
    if (isWithinSupportedIntradayBarSession(p_trade.m_timestamp))
    {
        // In live/sim the TradeStation bar stream owns the 1m candle (see processBar). Live trades here
        // are synthesized from quote snapshots, which only carry the latest print, so building a second
        // 1m bar from them would fight the real one (lower volume, different close) and make it jitter.
        if (MainApp::getDataSourceMode() != DataSourceMode::Live)
        {
            m_liveBarAccumulator.onNewTrade(symbol, p_trade);
        }
    }
    else
    {
        DEBUG << "Ignoring trade outside supported intraday bar session for" << symbol << "at"
              << p_trade.m_timestamp.toString(Qt::ISODate);
    }
    emit receivedNewTrade(symbol, p_trade);

    // Write trade to DisplaySnapshot so any chart showing this symbol gets it
    LTTnG_TP(opentraderplatform, snapshot_write, symbol.toUtf8().constData(), "trade");
    QWriteLocker lock(&m_displaySnapshot.lock);
    if (!m_displaySnapshot.processedReplayTime.has_value() ||
        p_trade.m_timestamp > *m_displaySnapshot.processedReplayTime)
    {
        m_displaySnapshot.processedReplayTime = p_trade.m_timestamp;
    }
    m_displaySnapshot.pendingTrades.append(p_trade);
    m_displaySnapshot.recentTrades.append(p_trade);
    const int excessTrades =
        m_displaySnapshot.recentTrades.size() - PlatformControlConstants::RECENT_TRADES_BUFFER_LIMIT;
    if (excessTrades > 0)
    {
        m_displaySnapshot.recentTrades.remove(0, excessTrades);
    }
    m_displaySnapshot.tradeDirty = true;
}

uint64_t MainAlgo::getNextRequestId()
{
    // Thread-safe atomic increment returns the old value, so we need pre-increment semantics
    // Actually ++operator does pre-increment by default for atomic
    return ++m_requestIdCounter;
}

QFuture<std::expected<ClosePositionsResult, QString>> MainAlgo::closePositions(const ClosePositionsRequest& p_request,
                                                                               const QString& p_strategyID)
{
    auto promise = std::make_shared<QPromise<std::expected<ClosePositionsResult, QString>>>();
    promise->start();
    QFuture<std::expected<ClosePositionsResult, QString>> future = promise->future();

    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, p_request, p_strategyID, promise]() { processClosePositions(p_strategyID, p_request, promise); },
        Qt::QueuedConnection);
    if (!invoked)
    {
        promise->addResult(std::unexpected(QStringLiteral("Failed to dispatch close positions request to MainAlgo")));
        promise->finish();
    }

    return future;
}

QFuture<std::expected<PlaceOrderResult, TSClient::Error>> MainAlgo::placeOrder(const PlaceOrderRequest& p_orderRequest,
                                                                               const QString& p_strategyID)
{
    auto promise = std::make_shared<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>>();
    promise->start();
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> future = promise->future();

    const uint64_t requestId = getNextRequestId();
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, requestId, p_strategyID, p_orderRequest, promise]()
        { processPlaceOrder(requestId, p_strategyID, p_orderRequest, promise); },
        Qt::QueuedConnection);
    if (!invoked)
    {
        promise->addResult(std::unexpected(TSClient::Error::Other));
        promise->finish();
    }

    return future;
}

RiskConfig MainAlgo::getRiskConfigForAccount(const QString& p_accountID) const
{
    if (!m_riskManager)
    {
        return {};
    }

    if (QThread::currentThread() == &thread)
    {
        return m_riskManager->getConfig(p_accountID);
    }

    if (!thread.isRunning())
    {
        return {};
    }

    RiskConfig result;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_accountID]() { result = m_riskManager->getConfig(p_accountID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

RiskStatusSnapshot MainAlgo::getRiskStatusSnapshotForAccount(const QString& p_accountID) const
{
    if (!m_riskManager)
    {
        return {};
    }

    if (QThread::currentThread() == &thread)
    {
        RiskStatusSnapshot snapshot = m_riskManager->getStatusSnapshot(p_accountID, MainApp::getCurrentAppTime());
        snapshot.runtime.openPositionsCount = getOpenPositionCountForAccount(p_accountID);
        return snapshot;
    }

    if (!thread.isRunning())
    {
        return {};
    }

    RiskStatusSnapshot result;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_accountID]()
        {
            result = m_riskManager->getStatusSnapshot(p_accountID, MainApp::getCurrentAppTime());
            result.runtime.openPositionsCount = getOpenPositionCountForAccount(p_accountID);
        },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

void MainAlgo::setRiskConfigForAccount(const QString& p_accountID, const RiskConfig& p_config)
{
    if (!m_riskManager)
    {
        return;
    }

    if (QThread::currentThread() == &thread)
    {
        m_riskManager->setConfig(p_accountID, p_config, MainApp::getCurrentAppTime());
        refreshRiskStatusForAccount(p_accountID);
        return;
    }

    if (!thread.isRunning())
    {
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, p_accountID, p_config]()
        {
            m_riskManager->setConfig(p_accountID, p_config, MainApp::getCurrentAppTime());
            refreshRiskStatusForAccount(p_accountID);
        },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
}

void MainAlgo::resetRiskDayForAccount(const QString& p_accountID)
{
    if (!m_riskManager)
    {
        return;
    }

    if (QThread::currentThread() == &thread)
    {
        m_riskManager->resetRiskDay(p_accountID, MainApp::getCurrentAppTime(), QStringLiteral("manual reset"));
        refreshRiskStatusForAccount(p_accountID);
        return;
    }

    if (!thread.isRunning())
    {
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, p_accountID]()
        {
            m_riskManager->resetRiskDay(p_accountID, MainApp::getCurrentAppTime(), QStringLiteral("manual reset"));
            refreshRiskStatusForAccount(p_accountID);
        },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
}

void MainAlgo::unlockRiskForAccount(const QString& p_accountID)
{
    if (!m_riskManager)
    {
        return;
    }

    if (QThread::currentThread() == &thread)
    {
        m_riskManager->unlockTrading(p_accountID, MainApp::getCurrentAppTime(), QStringLiteral("manual unlock"));
        refreshRiskStatusForAccount(p_accountID);
        return;
    }

    if (!thread.isRunning())
    {
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, p_accountID]()
        {
            m_riskManager->unlockTrading(p_accountID, MainApp::getCurrentAppTime(), QStringLiteral("manual unlock"));
            refreshRiskStatusForAccount(p_accountID);
        },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
}

std::expected<QString, QString> MainAlgo::loadStrategy(const StrategyConfig& p_config)
{
    if (!m_strategyManager)
    {
        return std::unexpected("Strategy manager unavailable");
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->loadStrategy(p_config);
    }

    if (!thread.isRunning())
    {
        return std::unexpected("MainAlgo thread is not running");
    }

    std::expected<QString, QString> result = std::unexpected(QString{});
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_config]() { result = m_strategyManager->loadStrategy(p_config); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::startStrategy(const QString& p_strategyID)
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->startStrategy(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_strategyID]() { result = m_strategyManager->startStrategy(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::startQueuedReplayStrategies()
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->startQueuedReplayStrategies();
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result]() { result = m_strategyManager->startQueuedReplayStrategies(); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::stopStrategy(const QString& p_strategyID)
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->stopStrategy(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_strategyID]() { result = m_strategyManager->stopStrategy(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::updateStrategyConfig(const QString& p_strategyID, const StrategyConfig& p_config)
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->updateStrategyConfig(p_strategyID, p_config);
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_strategyID, p_config]()
        { result = m_strategyManager->updateStrategyConfig(p_strategyID, p_config); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::unclaimStrategySymbol(const QString& p_strategyID, const QString& p_symbol)
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->unclaimSymbol(p_strategyID, p_symbol, false);
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_strategyID, p_symbol]()
        { result = m_strategyManager->unclaimSymbol(p_strategyID, p_symbol, false); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::unclaimAndBlockStrategySymbol(const QString& p_strategyID, const QString& p_symbol)
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->unclaimSymbol(p_strategyID, p_symbol, true);
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_strategyID, p_symbol]()
        { result = m_strategyManager->unclaimSymbol(p_strategyID, p_symbol, true); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QString MainAlgo::unloadStrategy(const QString& p_strategyID)
{
    if (!m_strategyManager)
    {
        return "Strategy manager unavailable";
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->unloadStrategy(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return "MainAlgo thread is not running";
    }

    QString result;
    const bool invoked = QMetaObject::invokeMethod(
        this,
        [this, &result, p_strategyID]() { result = m_strategyManager->unloadStrategy(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

bool MainAlgo::isStrategyRunning(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return false;
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->isStrategyRunning(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return false;
    }

    bool result = false;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_strategyID]() { result = m_strategyManager->isStrategyRunning(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

StrategyManager::StrategyExecutionState MainAlgo::getStrategyExecutionState(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return StrategyManager::StrategyExecutionState::Stopped;
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->getStrategyExecutionState(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return StrategyManager::StrategyExecutionState::Stopped;
    }

    StrategyManager::StrategyExecutionState result = StrategyManager::StrategyExecutionState::Stopped;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_strategyID]() { result = m_strategyManager->getStrategyExecutionState(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

StrategyConfig MainAlgo::getStrategyConfig(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return {};
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->getStrategyConfig(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return {};
    }

    StrategyConfig result;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_strategyID]() { result = m_strategyManager->getStrategyConfig(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QVector<Position> MainAlgo::getStrategyOpenPositions(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return {};
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->getStrategyOpenPositions(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return {};
    }

    QVector<Position> result;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_strategyID]() { result = m_strategyManager->getStrategyOpenPositions(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

int MainAlgo::getStrategyPositionCount(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return 0;
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->getStrategyPositionCount(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return 0;
    }

    int result = 0;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, &result, p_strategyID]() { result = m_strategyManager->getStrategyPositionCount(p_strategyID); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

QVector<StrategySymbolViewState> MainAlgo::getStrategySymbolViewStates(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return {};
    }

    if (QThread::currentThread() == &thread)
    {
        return m_strategyManager->getStrategySymbolViewStates(p_strategyID);
    }

    if (!thread.isRunning())
    {
        return {};
    }

    struct StrategyViewStatesQuery
    {
        QSemaphore completion{0};
        QVector<StrategySymbolViewState> result;
    };

    const std::shared_ptr<StrategyViewStatesQuery> query = std::make_shared<StrategyViewStatesQuery>();
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [this, query, p_strategyID]()
        {
            if (m_strategyManager)
            {
                query->result = m_strategyManager->getStrategySymbolViewStates(p_strategyID);
            }
            query->completion.release();
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(invoked);

    if (!query->completion.tryAcquire(1, kStrategyViewStatesQueryWaitMs))
    {
        DEBUG << "Timed out waiting for strategy symbol view states for" << p_strategyID;
        return {};
    }

    return query->result;
}

std::optional<QVector<StrategyLogMessage>> MainAlgo::getStrategyLogMessages(const QString& p_strategyID) const
{
    if (!m_strategyManager)
    {
        return std::nullopt;
    }

    auto readMessages = [this, &p_strategyID]() -> std::optional<QVector<StrategyLogMessage>>
    {
        const StrategyLogger* logger = m_strategyManager->getStrategyLogger(p_strategyID);
        if (logger == nullptr)
        {
            return std::nullopt;
        }

        return logger->getMessages();
    };

    if (QThread::currentThread() == &thread)
    {
        return readMessages();
    }

    if (!thread.isRunning())
    {
        return std::nullopt;
    }

    std::optional<QVector<StrategyLogMessage>> result;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [&result, readMessages]() { result = readMessages(); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

int MainAlgo::getPendingManualOrderConfirmationsForStrategy(const QString& p_strategyID) const
{
    const QString strategyID = p_strategyID.trimmed();
    if (strategyID.isEmpty())
    {
        return 0;
    }

    auto countForStrategy = [this, strategyID]() -> int
    {
        int count = 0;
        if (m_activeManualOrderConfirmation.has_value() && m_activeManualOrderConfirmation->strategyID == strategyID)
        {
            ++count;
        }

        for (const ManualOrderConfirmationRequest& request: m_pendingManualOrderConfirmations)
        {
            if (request.strategyID == strategyID)
            {
                ++count;
            }
        }

        return count;
    };

    if (QThread::currentThread() == &thread)
    {
        return countForStrategy();
    }

    if (!thread.isRunning())
    {
        return 0;
    }

    int result = 0;
    MainAlgo* const self = const_cast<MainAlgo*>(this);
    const bool invoked = QMetaObject::invokeMethod(
        self,
        [&result, countForStrategy]() { result = countForStrategy(); },
        Qt::BlockingQueuedConnection);
    ASSUME_TRUE(invoked);
    return result;
}

void MainAlgo::processClosePositions(const QString& p_strategyID,
                                     const ClosePositionsRequest& p_request,
                                     std::shared_ptr<QPromise<std::expected<ClosePositionsResult, QString>>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(p_promise.get(), nullptr);

    const QString resolvedAccountId = p_request.accountId.trimmed();
    if (resolvedAccountId.isEmpty())
    {
        p_promise->addResult(std::unexpected(QStringLiteral("Close positions request requires a non-empty accountId")));
        p_promise->finish();
        return;
    }

    if (p_request.aggressivityOffsetCents < 0.0)
    {
        p_promise->addResult(
            std::unexpected(QStringLiteral("Close positions aggressivity offset must be non-negative")));
        p_promise->finish();
        return;
    }

    if (MainApp::isInReplayMode() && getReplayState() == Playback::State::Paused)
    {
        DeferredClosePositionsRequest deferred;
        deferred.strategyID = p_strategyID;
        deferred.request = p_request;
        deferred.promise = p_promise;
        m_deferredClosePositionsRequests.enqueue(std::move(deferred));

        INFO << "Queued close positions request until replay resumes:" << "account=" << resolvedAccountId
             << "symbols=" << p_request.symbols << "queued=" << m_deferredClosePositionsRequests.size();
        return;
    }

    ClosePositionsResult initialResult;
    initialResult.accountId = resolvedAccountId;
    initialResult.requestedSymbols = normalizeClosePositionSymbols(p_request.symbols);
    initialResult.aggressivityOffsetCents = p_request.aggressivityOffsetCents;
    initialResult.executionMode = p_request.executionMode;

    const TradingSession session = MainApp::getCurrentSession();
    initialResult.session = tradingSessionToString(session);
    initialResult.usesLimitOrders = isExtendedHoursSession(session);
    initialResult.usesAggressiveLimitOrders =
        initialResult.usesLimitOrders && p_request.executionMode == ClosePositionsExecutionMode::AggressiveMarketable;
    initialResult.forcedDayPlus = initialResult.usesLimitOrders;

    if (!isTradableCloseSession(session))
    {
        p_promise->addResult(std::unexpected(
            QString("Close positions kill switch is unavailable during the %1 session").arg(initialResult.session)));
        p_promise->finish();
        return;
    }

    QVector<Position> matchingPositions;
    matchingPositions.reserve(m_currentPositions.size());
    for (const Position& position: m_currentPositions.values())
    {
        const QString trimmedQuantity = position.getQuantity().trimmed();
        if (position.isDeleted() || trimmedQuantity == "0" || trimmedQuantity == "0.0" || trimmedQuantity == "0.00")
        {
            continue;
        }
        if (position.getAccountID() != resolvedAccountId)
        {
            continue;
        }
        if (!initialResult.requestedSymbols.isEmpty() && !initialResult.requestedSymbols.contains(position.getSymbol()))
        {
            continue;
        }
        matchingPositions.append(position);
    }

    std::sort(matchingPositions.begin(),
              matchingPositions.end(),
              [](const Position& p_left, const Position& p_right)
              {
                  if (p_left.getSymbol() == p_right.getSymbol())
                  {
                      return p_left.getPositionID() < p_right.getPositionID();
                  }
                  return p_left.getSymbol() < p_right.getSymbol();
              });

    struct ClosePositionsAggregationState
    {
        ClosePositionsResult result;
        int pendingOrders = 0;
        bool finished = false;
    };

    auto state = std::make_shared<ClosePositionsAggregationState>();
    state->result = initialResult;
    state->result.matchedPositionCount = matchingPositions.size();
    state->result.items.reserve(matchingPositions.size());

    auto finishIfComplete = [state, p_promise]()
    {
        if (!state->finished && state->pendingOrders == 0)
        {
            state->finished = true;
            p_promise->addResult(std::expected<ClosePositionsResult, QString>(state->result));
            p_promise->finish();
        }
    };

    if (matchingPositions.isEmpty())
    {
        INFO << "Close positions request found no open positions for account" << resolvedAccountId << "symbols"
             << initialResult.requestedSymbols;
        finishIfComplete();
        return;
    }

    INFO << "Close positions request:" << "account=" << resolvedAccountId << "positions=" << matchingPositions.size()
         << "session=" << initialResult.session
         << "mode=" << closePositionsExecutionModeToString(initialResult.executionMode)
         << "symbols=" << initialResult.requestedSymbols;

    const auto calculateCloseLimitPrice =
        [&initialResult](const Level2& p_level2, const TradeAction p_tradeAction) -> std::expected<double, QString>
    {
        switch (initialResult.executionMode)
        {
        case ClosePositionsExecutionMode::AggressiveMarketable:
            return calculateAggressiveMarketableLimitPrice(p_level2,
                                                           p_tradeAction,
                                                           initialResult.aggressivityOffsetCents);
        case ClosePositionsExecutionMode::PassiveResting:
            return calculatePassiveRestingLimitPrice(p_level2, p_tradeAction, initialResult.aggressivityOffsetCents);
        }

        return std::unexpected(QStringLiteral("Unsupported close positions execution mode"));
    };

    for (const Position& position: matchingPositions)
    {
        ClosePositionItemResult item;
        item.positionId = position.getPositionID();
        item.accountId = position.getAccountID();
        item.symbol = position.getSymbol();
        item.longShort = position.getLongShort();
        item.tradeAction = closeTradeActionForPosition(position);
        item.orderType = initialResult.usesLimitOrders ? OrderType::Type::Limit : OrderType::Type::Market;
        item.duration = initialResult.usesLimitOrders ? OrderDuration::DayPlus : OrderDuration::Day;

        if (position.getAssetType() != "STOCK")
        {
            item.failureCode = "unsupported_asset_type";
            item.failureMessage =
                QString("Close positions kill switch currently supports only STOCK positions (got %1)")
                    .arg(position.getAssetType());
            state->result.items.append(item);
            continue;
        }

        const auto quantityResult = parsePositionQuantityShares(position);
        if (!quantityResult.has_value())
        {
            item.failureCode = "invalid_quantity";
            item.failureMessage = quantityResult.error();
            state->result.items.append(item);
            continue;
        }
        item.quantity = quantityResult.value();

        if (initialResult.usesLimitOrders)
        {
            const std::expected<MarketDataSnapshot, QString> snapshot = getMarketDataSnapshot(position.getSymbol(), 1);
            if (!snapshot.has_value())
            {
                item.failureCode = "market_data_unavailable";
                item.failureMessage =
                    QString("Missing best bid/ask data for %1: %2").arg(position.getSymbol(), snapshot.error());
                state->result.items.append(item);
                continue;
            }

            std::optional<Level2> pricingLevel2 = snapshot->latestLevel2;
            const std::optional<Level2> positionBbo = level2FromPositionBbo(position);
            const bool usingPositionBbo = !pricingLevel2.has_value() && positionBbo.has_value();
            if (usingPositionBbo)
            {
                pricingLevel2 = positionBbo;
                DEBUG << "Using position BBO fallback to price close order for" << position.getSymbol();
            }

            if (!pricingLevel2.has_value())
            {
                item.failureCode = "market_data_unavailable";
                item.failureMessage = QString("Missing best bid/ask data for %1").arg(position.getSymbol());
                state->result.items.append(item);
                continue;
            }

            auto limitPriceResult = calculateCloseLimitPrice(pricingLevel2.value(), item.tradeAction);
            if (!limitPriceResult.has_value() && positionBbo.has_value() && !usingPositionBbo)
            {
                limitPriceResult = calculateCloseLimitPrice(positionBbo.value(), item.tradeAction);
                if (limitPriceResult.has_value())
                {
                    DEBUG << "Recovered close order pricing from position BBO for" << position.getSymbol();
                }
            }
            if (!limitPriceResult.has_value())
            {
                item.failureCode = "price_unavailable";
                item.failureMessage =
                    QString("Unable to price %1 close order: %2").arg(position.getSymbol(), limitPriceResult.error());
                state->result.items.append(item);
                continue;
            }
            item.limitPrice = limitPriceResult.value();
        }

        PlaceOrderRequest orderRequest;
        orderRequest.setAccountID(resolvedAccountId);
        orderRequest.setSymbol(position.getSymbol());
        orderRequest.setTradeAction(item.tradeAction);
        orderRequest.setOrderType(item.orderType);
        orderRequest.setQuantity(item.quantity);
        orderRequest.setTimeInForce(TimeInForce(item.duration));
        if (item.limitPrice.has_value())
        {
            orderRequest.setLimitPrice(item.limitPrice.value());
        }

        item.submitted = true;
        const int itemIndex = state->result.items.size();
        state->result.items.append(item);
        state->result.submittedOrderCount++;
        state->pendingOrders++;

        auto orderPromise = std::make_shared<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>>();
        orderPromise->start();
        QFuture<std::expected<PlaceOrderResult, TSClient::Error>> orderFuture = orderPromise->future();

        processPlaceOrder(getNextRequestId(), p_strategyID, orderRequest, orderPromise);

        orderFuture.then(
            this,
            [this, state, itemIndex, finishIfComplete](std::expected<PlaceOrderResult, TSClient::Error> p_result)
            {
                ASSUME_TRUE(itemIndex >= 0);
                ASSUME_TRUE(itemIndex < state->result.items.size());
                ClosePositionItemResult& itemResult = state->result.items[itemIndex];

                if (!p_result.has_value())
                {
                    itemResult.failureCode = tsClientErrorToString(p_result.error());
                    itemResult.failureMessage =
                        QString("Order placement failed: %1").arg(itemResult.failureCode.value());
                }
                else
                {
                    const PlaceOrderResult& placeOrderResult = p_result.value();
                    appendPlaceOrderItems(placeOrderResult.getOrders(),
                                          &itemResult.orderIds,
                                          &itemResult.brokerMessages,
                                          &itemResult.brokerErrors);
                    appendPlaceOrderItems(placeOrderResult.getErrors(),
                                          &itemResult.orderIds,
                                          &itemResult.brokerMessages,
                                          &itemResult.brokerErrors);

                    itemResult.placementSucceeded = !placeOrderResult.hasErrors();
                    if (!itemResult.placementSucceeded)
                    {
                        itemResult.failureCode = "place_order_rejected";
                        itemResult.failureMessage = summarizeClosePositionFailure(itemResult);
                    }
                    else
                    {
                        QSet<QString>& trackedOrderIds = m_pendingCloseOrderIdsByPositionId[itemResult.positionId];
                        for (const QString& rawOrderId: itemResult.orderIds)
                        {
                            const QString trackedOrderId = rawOrderId.trimmed();
                            if (!trackedOrderId.isEmpty())
                            {
                                trackedOrderIds.insert(trackedOrderId);
                            }
                        }

                        if (trackedOrderIds.isEmpty())
                        {
                            m_pendingCloseOrderIdsByPositionId.remove(itemResult.positionId);
                        }
                    }
                }

                state->pendingOrders--;
                finishIfComplete();
            });
    }

    finishIfComplete();
}

void MainAlgo::onReplayResumedForDeferredClosePositions()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (m_deferredClosePositionsRequests.isEmpty())
    {
        return;
    }

    QQueue<DeferredClosePositionsRequest> pending;
    pending.swap(m_deferredClosePositionsRequests);

    INFO << "Processing deferred close positions requests after replay resumed. count=" << pending.size();

    while (!pending.isEmpty())
    {
        DeferredClosePositionsRequest request = pending.dequeue();
        ASSUME_DIFF(request.promise.get(), nullptr);
        processClosePositions(request.strategyID, request.request, request.promise);
    }
}

void MainAlgo::processPlaceOrder(uint64_t p_requestId,
                                 const QString& p_strategyID,
                                 const PlaceOrderRequest& p_orderRequest,
                                 std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    PlaceOrderRequest orderRequest = p_orderRequest;
    if (MainApp::isInReplayMode() || TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
    {
        orderRequest.setRoute(QStringLiteral("replay"));
    }

    const QString accountId = orderRequest.getAccountID().trimmed().toUpper();
    if (m_riskManager)
    {
        const RiskManager::OrderEvalContext evalContext = {
            getSignedNetPositionSharesForAccountSymbol(accountId, orderRequest.getSymbol()),
            getOpenPositionCountForAccount(accountId),
            resolveOrderRiskReferencePrice(orderRequest),
            resolveOrderRiskStopPrice(orderRequest),
        };

        const RiskDecision riskDecision =
            m_riskManager->evaluateOrder(orderRequest, evalContext, MainApp::getCurrentAppTime());
        m_requestIdToRiskEntryCandidate[p_requestId] = riskDecision.isEntry;
        if (!riskDecision.allow)
        {
            WARNING << "Risk rejected order before placement. requestId=" << p_requestId
                    << "strategyID=" << p_strategyID << "account=" << accountId << "symbol=" << orderRequest.getSymbol()
                    << "reasonCode=" << riskDecision.reasonCode << "message=" << riskDecision.message;

            refreshRiskStatusForAccount(accountId);
            m_requestIdToRiskEntryCandidate.remove(p_requestId);

            PlaceOrderResult rejected = makeRejectedPlaceOrderResult(riskDecision.reasonCode, riskDecision.message);
            p_promise->addResult(std::expected<PlaceOrderResult, TSClient::Error>(rejected));
            p_promise->finish();
            return;
        }
    }
    else
    {
        m_requestIdToRiskEntryCandidate[p_requestId] = false;
    }

    // Store temporary mapping: requestId -> strategyID (empty string for non-strategy callers;
    // replaced with OrderID -> strategyID when ACK received for strategy-owned orders)
    m_requestIdToStrategyId[p_requestId] = p_strategyID;
    m_requestIdToPlacedOrderRequest[p_requestId] = orderRequest;

    // If the request carries a strategy log, hold it until we have the OrderID from ACK
    if (orderRequest.getStrategyLog().has_value())
    {
        m_pendingOrderLogs[p_requestId] = orderRequest.getStrategyLog().value();
    }

    // Store the promise for resolution when order is acknowledged
    m_pendingOrderPromises[p_requestId] = p_promise;

    // Call TSClient to place the order
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> future =
        TSClient::getInstance()->placeOrder(orderRequest);

    // Attach continuation to detect resolution
    // Pass 'this' as context so continuation runs on MainAlgo thread
    future.then(this,
                [this, p_requestId](std::expected<PlaceOrderResult, TSClient::Error> result)
                { onOrderResolved(p_requestId, result); });

    DEBUG << "Processing placeOrder: requestId=" << p_requestId << "strategyID=" << p_strategyID;
}

QString MainAlgo::defaultManualOrderPrompt(const PlaceOrderRequest& p_orderRequest)
{
    return QString("%1 %2 %3 (%4)")
        .arg(tradeActionToText(p_orderRequest.getTradeAction()),
             QString::number(p_orderRequest.getQuantity()),
             p_orderRequest.getSymbol(),
             OrderType::toString(p_orderRequest.getOrderType().type));
}

void MainAlgo::beginManualConfirmationReplaySpeedOverrideIfNeeded()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (m_manualConfirmationReplaySpeedOverrideActive || !MainApp::isInReplayMode() ||
        getReplayState() != Playback::State::Playing)
    {
        return;
    }

    if (m_replaySpeed <= Playback::Speed::Normal)
    {
        return;
    }

    m_manualConfirmationReplaySpeedOverrideActive = true;
    m_manualConfirmationReplaySpeedRestoreTarget = m_replaySpeed;
    applyReplaySpeedInternal(Playback::Speed::Normal);
    INFO << "Manual confirmation active: temporarily forcing replay speed to 1x";
}

void MainAlgo::endManualConfirmationReplaySpeedOverrideIfIdle()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (!m_manualConfirmationReplaySpeedOverrideActive)
    {
        return;
    }

    if (m_activeManualOrderConfirmation.has_value() || !m_pendingManualOrderConfirmations.isEmpty())
    {
        return;
    }

    const Playback::Speed restoreSpeed = m_manualConfirmationReplaySpeedRestoreTarget.value_or(Playback::Speed::Normal);
    m_manualConfirmationReplaySpeedOverrideActive = false;
    m_manualConfirmationReplaySpeedRestoreTarget.reset();
    applyReplaySpeedInternal(restoreSpeed);
    INFO << "Manual confirmation queue drained: restored replay speed to" << static_cast<int>(restoreSpeed);
}

void MainAlgo::applyReplaySpeedInternal(const Playback::Speed p_speed)
{
    m_replaySpeed = p_speed;
    DBClient::getInstance()->setReplaySpeed(p_speed);

    // Sync speed to OrderEmulator so latency is scaled correctly
    if (OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator())
    {
        emulator->setReplaySpeed(static_cast<int>(p_speed));
    }
}

void MainAlgo::processPlaceOrderWithUserConfirmation(
    const uint64_t p_requestId,
    const QString& p_strategyID,
    const QString& p_strategyRequestID,
    const PlaceOrderRequest& p_orderRequest,
    const QString& p_promptText,
    const StrategyManualOrderExecutionMode p_executionMode,
    std::shared_ptr<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(p_promise.get(), nullptr);
    ASSUME_FALSE(p_strategyID.isEmpty());

    const QString strategyRequestID = p_strategyRequestID.trimmed();
    if (strategyRequestID.isEmpty())
    {
        WARNING << "Rejected strategy manual order request with empty strategy request ID for strategy" << p_strategyID;
        if (m_strategyManager != nullptr && !strategyRequestID.isEmpty())
        {
            m_strategyManager->publishManualOrderDecision(
                p_strategyID,
                strategyRequestID,
                p_orderRequest.getSymbol(),
                StrategyManualOrderDecision::Cancelled,
                QStringLiteral("Manual confirmation request_id must not be empty"));
        }
        p_promise->addResult(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
        p_promise->finish();
        return;
    }

    if (p_executionMode == StrategyManualOrderExecutionMode::MarketableOnAccept)
    {
        const OrderType::Type orderType = p_orderRequest.getOrderType().type;
        if (orderType == OrderType::Type::StopMarket || orderType == OrderType::Type::StopLimit)
        {
            const QString reason = QStringLiteral("Marketable-on-accept confirmations do not support stop orders");
            if (m_strategyManager != nullptr)
            {
                m_strategyManager->publishManualOrderDecision(p_strategyID,
                                                              strategyRequestID,
                                                              p_orderRequest.getSymbol(),
                                                              StrategyManualOrderDecision::Rejected,
                                                              reason);
            }
            p_promise->addResult(std::expected<PlaceOrderResult, TSClient::Error>(
                std::unexpected(TSClient::Error::RejectedByValidator)));
            p_promise->finish();
            return;
        }
    }

    if (m_manualOrderConfirmationsMuted)
    {
        const QString reason = QStringLiteral("Manual confirmations muted by user (press M to unmute)");
        WARNING << "Rejected strategy manual order request while global manual-confirm mute is active."
                << "strategyID=" << p_strategyID << "requestID=" << strategyRequestID
                << "symbol=" << p_orderRequest.getSymbol() << "reason=" << reason;
        if (m_strategyManager != nullptr)
        {
            m_strategyManager->publishManualOrderDecision(p_strategyID,
                                                          strategyRequestID,
                                                          p_orderRequest.getSymbol(),
                                                          StrategyManualOrderDecision::Rejected,
                                                          reason);
        }
        p_promise->addResult(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
        p_promise->finish();
        return;
    }

    const QString requestedSymbol = p_orderRequest.getSymbol().trimmed().toUpper();
    if (!requestedSymbol.isEmpty() && m_blockedManualConfirmationSymbols.contains(requestedSymbol))
    {
        const QString reason =
            QStringLiteral("User blocked manual confirmations for this symbol with the Shift+N shortcut");
        WARNING << "Rejected strategy manual order request for user-blocked symbol." << "strategyID=" << p_strategyID
                << "requestID=" << strategyRequestID << "symbol=" << requestedSymbol << "reason=" << reason;
        if (m_strategyManager != nullptr)
        {
            m_strategyManager->publishManualOrderDecision(p_strategyID,
                                                          strategyRequestID,
                                                          p_orderRequest.getSymbol(),
                                                          StrategyManualOrderDecision::Rejected,
                                                          reason);
        }
        p_promise->addResult(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
        p_promise->finish();
        return;
    }

    if (!requestedSymbol.isEmpty())
    {
        const auto fuseDuplicateRequest = [&](ManualOrderConfirmationRequest& p_targetRequest)
        {
            p_targetRequest.orderRequest = p_orderRequest;
            p_targetRequest.executionMode = p_executionMode;
            const QString incomingPromptText = p_promptText.trimmed();
            if (!incomingPromptText.isEmpty())
            {
                p_targetRequest.promptText = incomingPromptText;
            }
        };

        QString fusedConfirmationID;
        if (m_activeManualOrderConfirmation.has_value())
        {
            const QString activeSymbol = m_activeManualOrderConfirmation->orderRequest.getSymbol().trimmed().toUpper();
            if (activeSymbol == requestedSymbol)
            {
                fuseDuplicateRequest(m_activeManualOrderConfirmation.value());
                fusedConfirmationID = m_activeManualOrderConfirmation->confirmationID;
            }
        }
        if (fusedConfirmationID.isEmpty())
        {
            for (ManualOrderConfirmationRequest& pendingRequest: m_pendingManualOrderConfirmations)
            {
                const QString pendingSymbol = pendingRequest.orderRequest.getSymbol().trimmed().toUpper();
                if (pendingSymbol == requestedSymbol)
                {
                    fuseDuplicateRequest(pendingRequest);
                    fusedConfirmationID = pendingRequest.confirmationID;
                    break;
                }
            }
        }

        if (!fusedConfirmationID.isEmpty())
        {
            const QString reason =
                QString("Manual confirmation already pending for %1; duplicate request fused into %2")
                    .arg(requestedSymbol, fusedConfirmationID);
            WARNING << "Rejected duplicate strategy manual order request for symbol with pending confirmation."
                    << "strategyID=" << p_strategyID << "requestID=" << strategyRequestID
                    << "symbol=" << requestedSymbol << "fusedConfirmationID=" << fusedConfirmationID
                    << "reason=" << reason;

            if (m_strategyManager != nullptr)
            {
                m_strategyManager->publishManualOrderDecision(p_strategyID,
                                                              strategyRequestID,
                                                              p_orderRequest.getSymbol(),
                                                              StrategyManualOrderDecision::Cancelled,
                                                              reason);
            }
            p_promise->addResult(std::expected<PlaceOrderResult, TSClient::Error>(
                std::unexpected(TSClient::Error::RejectedByValidator)));
            p_promise->finish();
            return;
        }
    }

    const int totalPending =
        m_pendingManualOrderConfirmations.size() + (m_activeManualOrderConfirmation.has_value() ? 1 : 0);
    if (totalPending >= StrategyManualConfirmationConstants::MAX_PENDING_REQUESTS)
    {
        WARNING << "Rejected strategy manual order request due to queue overflow. strategyID=" << p_strategyID
                << "requestID=" << strategyRequestID << "pending=" << totalPending;
        if (m_strategyManager != nullptr)
        {
            m_strategyManager->publishManualOrderDecision(p_strategyID,
                                                          strategyRequestID,
                                                          p_orderRequest.getSymbol(),
                                                          StrategyManualOrderDecision::Cancelled,
                                                          QStringLiteral("Manual confirmation queue is full"));
        }
        p_promise->addResult(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
        p_promise->finish();
        return;
    }

    int timeoutSeconds = StrategyManualConfirmationConstants::DEFAULT_TIMEOUT_SECONDS;
    if (appStateSettings != nullptr)
    {
        timeoutSeconds = appStateSettings
                             ->value(StrategyManualConfirmationConstants::SETTINGS_KEY_TIMEOUT_SECONDS,
                                     StrategyManualConfirmationConstants::DEFAULT_TIMEOUT_SECONDS)
                             .toInt();
    }
    timeoutSeconds = std::clamp(timeoutSeconds,
                                StrategyManualConfirmationConstants::MIN_TIMEOUT_SECONDS,
                                StrategyManualConfirmationConstants::MAX_TIMEOUT_SECONDS);

    ManualOrderConfirmationRequest request;
    request.requestId = p_requestId;
    request.confirmationID = QString("strategy-confirmation-%1").arg(p_requestId);
    request.strategyID = p_strategyID;
    request.strategyRequestID = strategyRequestID;
    request.orderRequest = p_orderRequest;
    request.promptText = p_promptText.trimmed();
    request.executionMode = p_executionMode;
    request.promise = p_promise;
    request.timeoutSec = timeoutSeconds;

    m_pendingManualOrderConfirmations.enqueue(request);
    activateNextManualOrderConfirmationIfIdle();
}

void MainAlgo::activateNextManualOrderConfirmationIfIdle()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(m_manualOrderConfirmationTimer.get(), nullptr);

    if (m_activeManualOrderConfirmation.has_value() || m_pendingManualOrderConfirmations.isEmpty())
    {
        return;
    }

    m_activeManualOrderConfirmation = m_pendingManualOrderConfirmations.dequeue();
    ASSUME_TRUE(m_activeManualOrderConfirmation.has_value());
    ManualOrderConfirmationRequest& request = m_activeManualOrderConfirmation.value();

    if (!m_manualConfirmationFrontendAvailable)
    {
        finalizeActiveManualOrderConfirmation(StrategyManualOrderDecision::Rejected,
                                              QStringLiteral("Manual confirmation requires GUI frontend"),
                                              false);
        return;
    }

    beginManualConfirmationReplaySpeedOverrideIfNeeded();

    const QString promptText =
        request.promptText.isEmpty() ? defaultManualOrderPrompt(request.orderRequest) : request.promptText;
    const QString accountID = request.orderRequest.getAccountID().trimmed().toUpper();
    const int quantity = std::max(0, request.orderRequest.getQuantity());
    const bool isLongSide = isBuySideTradeAction(request.orderRequest.getTradeAction());
    double referencePrice = 0.0;
    if (request.orderRequest.getLimitPrice().has_value() && request.orderRequest.getLimitPrice().value() > 0.0)
    {
        referencePrice = request.orderRequest.getLimitPrice().value();
    }
    else
    {
        const std::optional<double> resolvedReferencePrice = resolveOrderRiskReferencePrice(request.orderRequest);
        if (resolvedReferencePrice.has_value() && resolvedReferencePrice.value() > 0.0)
        {
            referencePrice = resolvedReferencePrice.value();
        }
    }
    double stopPrice = 0.0;
    if (request.orderRequest.getStopPrice().has_value() && request.orderRequest.getStopPrice().value() > 0.0)
    {
        stopPrice = request.orderRequest.getStopPrice().value();
    }
    emit strategyOrderConfirmationRequested(request.confirmationID,
                                            request.orderRequest.getSymbol(),
                                            promptText,
                                            request.timeoutSec,
                                            accountID,
                                            quantity,
                                            isLongSide,
                                            referencePrice,
                                            stopPrice);

    const qint64 timeoutMs = static_cast<qint64>(request.timeoutSec) * 1000LL;
    m_manualOrderConfirmationRemainingMs =
        static_cast<int>(std::clamp<qint64>(timeoutMs, 0, std::numeric_limits<int>::max()));
    m_manualOrderConfirmationCountdownPaused = false;

    if (m_manualOrderConfirmationRemainingMs <= 0)
    {
        onManualOrderConfirmationTimeout();
        return;
    }

    if (MainApp::isInReplayMode() && getReplayState() == Playback::State::Paused)
    {
        m_manualOrderConfirmationCountdownPaused = true;
        return;
    }

    m_manualOrderConfirmationTimer->start(m_manualOrderConfirmationRemainingMs);
}

void MainAlgo::onStrategyOrderConfirmationDecision(const QString& p_confirmationID,
                                                   const bool p_accepted,
                                                   const double p_overrideStopPrice)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (!m_activeManualOrderConfirmation.has_value())
    {
        DEBUG << "Ignoring strategy order confirmation decision without active request. confirmationID="
              << p_confirmationID;
        return;
    }

    ManualOrderConfirmationRequest& active = m_activeManualOrderConfirmation.value();
    if (active.confirmationID != p_confirmationID)
    {
        DEBUG << "Ignoring stale strategy order confirmation decision. active=" << active.confirmationID
              << "received=" << p_confirmationID;
        return;
    }

    if (p_accepted && p_overrideStopPrice > 0.0)
    {
        active.userOverrideStopPrice = p_overrideStopPrice;
    }

    finalizeActiveManualOrderConfirmation(
        p_accepted ? StrategyManualOrderDecision::Accepted : StrategyManualOrderDecision::Rejected,
        p_accepted ? QStringLiteral("Accepted by user") : QStringLiteral("Rejected by user"),
        true);
}

void MainAlgo::onStrategyOrderConfirmationRejectAndBlockSymbol(const QString& p_confirmationID)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (!m_activeManualOrderConfirmation.has_value())
    {
        DEBUG << "Ignoring strategy confirmation reject+block without active request. confirmationID="
              << p_confirmationID;
        return;
    }

    const ManualOrderConfirmationRequest& active = m_activeManualOrderConfirmation.value();
    if (active.confirmationID != p_confirmationID)
    {
        DEBUG << "Ignoring stale strategy confirmation reject+block. active=" << active.confirmationID
              << "received=" << p_confirmationID;
        return;
    }

    const QString blockedSymbol = active.orderRequest.getSymbol().trimmed().toUpper();
    if (!blockedSymbol.isEmpty())
    {
        m_blockedManualConfirmationSymbols.insert(blockedSymbol);
        INFO << "Blocked future manual confirmations for symbol" << blockedSymbol << "(requested by user shortcut R)";
    }

    const QString reason = blockedSymbol.isEmpty()
                               ? QStringLiteral("Rejected by user")
                               : QStringLiteral("Rejected by user and blocked future confirmations for this symbol");

    finalizeActiveManualOrderConfirmation(StrategyManualOrderDecision::Rejected, reason, true);
}

void MainAlgo::onManualOrderConfirmationTimeout()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (m_manualOrderConfirmationCountdownPaused)
    {
        return;
    }

    finalizeActiveManualOrderConfirmation(StrategyManualOrderDecision::TimedOut,
                                          QStringLiteral("User confirmation timed out"),
                                          true);
}

std::expected<PlaceOrderRequest, QString>
MainAlgo::resolveManualOrderRequestOnAccept(const ManualOrderConfirmationRequest& p_request)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (p_request.executionMode == StrategyManualOrderExecutionMode::Fixed)
    {
        return p_request.orderRequest;
    }

    ASSUME_TRUE(p_request.executionMode == StrategyManualOrderExecutionMode::MarketableOnAccept);

    const TradingSession session = MainApp::getCurrentSession();
    if (!isTradableCloseSession(session))
    {
        return std::unexpected(QString("Manual marketable confirmation is unavailable during the %1 session")
                                   .arg(tradingSessionToString(session)));
    }

    double marketableOffsetCents = StrategyManualConfirmationConstants::DEFAULT_MARKETABLE_OFFSET_CENTS;
    double maxChasePercent = StrategyManualConfirmationConstants::DEFAULT_MAX_CHASE_PERCENT;
    if (appStateSettings != nullptr)
    {
        marketableOffsetCents = appStateSettings
                                    ->value(StrategyManualConfirmationConstants::SETTINGS_KEY_MARKETABLE_OFFSET_CENTS,
                                            StrategyManualConfirmationConstants::DEFAULT_MARKETABLE_OFFSET_CENTS)
                                    .toDouble();
        maxChasePercent = appStateSettings
                              ->value(StrategyManualConfirmationConstants::SETTINGS_KEY_MAX_CHASE_PERCENT,
                                      StrategyManualConfirmationConstants::DEFAULT_MAX_CHASE_PERCENT)
                              .toDouble();
    }
    marketableOffsetCents = std::clamp(marketableOffsetCents,
                                       StrategyManualConfirmationConstants::MIN_MARKETABLE_OFFSET_CENTS,
                                       StrategyManualConfirmationConstants::MAX_MARKETABLE_OFFSET_CENTS);
    maxChasePercent = std::clamp(maxChasePercent,
                                 StrategyManualConfirmationConstants::MIN_MAX_CHASE_PERCENT,
                                 StrategyManualConfirmationConstants::MAX_MAX_CHASE_PERCENT);

    const QString symbol = p_request.orderRequest.getSymbol();
    const std::expected<MarketDataSnapshot, QString> snapshot = getMarketDataSnapshot(symbol, 1);
    if (!snapshot.has_value())
    {
        return std::unexpected(QString("Cannot resolve marketable price for %1: %2").arg(symbol, snapshot.error()));
    }

    if (!snapshot->latestLevel2.has_value())
    {
        return std::unexpected(QString("Cannot resolve marketable price for %1: best bid/ask unavailable").arg(symbol));
    }

    const Level2& level2 = snapshot->latestLevel2.value();
    const TradeAction tradeAction = p_request.orderRequest.getTradeAction();

    auto bestPriceResult = bestExecutableBookPrice(level2, tradeAction);
    if (!bestPriceResult.has_value())
    {
        return std::unexpected(
            QString("Cannot resolve marketable price for %1: %2").arg(symbol, bestPriceResult.error()));
    }
    const double bestExecutablePrice = bestPriceResult.value();

    PlaceOrderRequest resolvedOrder;
    resolvedOrder.setAccountID(p_request.orderRequest.getAccountID());
    resolvedOrder.setSymbol(symbol);
    resolvedOrder.setTradeAction(tradeAction);
    resolvedOrder.setQuantity(p_request.orderRequest.getQuantity());

    if (p_request.orderRequest.getAdvancedOptions().has_value())
    {
        resolvedOrder.setAdvancedOptions(p_request.orderRequest.getAdvancedOptions().value());
    }
    if (p_request.orderRequest.getOrderConfirmID().has_value())
    {
        resolvedOrder.setOrderConfirmID(p_request.orderRequest.getOrderConfirmID().value());
    }
    if (p_request.orderRequest.getRoute().has_value())
    {
        resolvedOrder.setRoute(p_request.orderRequest.getRoute().value());
    }
    if (p_request.orderRequest.getOcaGroupName().has_value())
    {
        resolvedOrder.setOcaGroupName(p_request.orderRequest.getOcaGroupName().value());
    }
    if (p_request.orderRequest.getOcaGroupType().has_value())
    {
        resolvedOrder.setOcaGroupType(p_request.orderRequest.getOcaGroupType().value());
    }
    if (p_request.orderRequest.getStrategyLog().has_value())
    {
        resolvedOrder.setStrategyLog(p_request.orderRequest.getStrategyLog().value());
    }

    double executionReferencePrice = bestExecutablePrice;
    if (isExtendedHoursSession(session))
    {
        auto limitPriceResult = calculateAggressiveMarketableLimitPrice(level2, tradeAction, marketableOffsetCents);
        if (!limitPriceResult.has_value())
        {
            return std::unexpected(
                QString("Cannot resolve marketable price for %1: %2").arg(symbol, limitPriceResult.error()));
        }

        executionReferencePrice = limitPriceResult.value();
        resolvedOrder.setOrderType(OrderType::Type::Limit);
        resolvedOrder.setTimeInForce(TimeInForce(OrderDuration::DayPlus));
        resolvedOrder.setLimitPrice(executionReferencePrice);
    }
    else
    {
        resolvedOrder.setOrderType(OrderType::Type::Market);
        resolvedOrder.setTimeInForce(p_request.orderRequest.getTimeInForce());
    }

    const std::optional<double> anchorLimitPrice = p_request.orderRequest.getLimitPrice();
    if (anchorLimitPrice.has_value() && anchorLimitPrice.value() > 0.0)
    {
        const double anchorPrice = anchorLimitPrice.value();
        if (isBuySideTradeAction(tradeAction))
        {
            const double maxAllowedPrice = anchorPrice * (1.0 + maxChasePercent / 100.0);
            if (executionReferencePrice > maxAllowedPrice)
            {
                return std::unexpected(
                    QString("Rejected marketable confirmation for %1: price moved from $%2 to $%3 (> %4%% chase)")
                        .arg(symbol,
                             QString::number(anchorPrice, 'f', 4),
                             QString::number(executionReferencePrice, 'f', 4),
                             QString::number(maxChasePercent, 'f', 2)));
            }
        }
        else if (isSellSideTradeAction(tradeAction))
        {
            const double minAllowedPrice = std::max(0.01, anchorPrice * (1.0 - maxChasePercent / 100.0));
            if (executionReferencePrice < minAllowedPrice)
            {
                return std::unexpected(
                    QString("Rejected marketable confirmation for %1: price moved from $%2 to $%3 (> %4%% chase)")
                        .arg(symbol,
                             QString::number(anchorPrice, 'f', 4),
                             QString::number(executionReferencePrice, 'f', 4),
                             QString::number(maxChasePercent, 'f', 2)));
            }
        }
    }

    return resolvedOrder;
}

void MainAlgo::finishManualOrderConfirmationRequest(const ManualOrderConfirmationRequest& p_request,
                                                    const StrategyManualOrderDecision p_decision,
                                                    const QString& p_reason,
                                                    const bool p_resumeReplayIfNeeded)
{
    Q_UNUSED(p_resumeReplayIfNeeded);
    StrategyManualOrderDecision finalDecision = p_decision;
    QString finalReason = p_reason;
    std::optional<PlaceOrderRequest> resolvedOrderRequest;
    if (p_decision == StrategyManualOrderDecision::Accepted)
    {
        const std::expected<PlaceOrderRequest, QString> resolvedOrder = resolveManualOrderRequestOnAccept(p_request);
        if (!resolvedOrder.has_value())
        {
            finalDecision = StrategyManualOrderDecision::Rejected;
            finalReason = resolvedOrder.error();
            WARNING << "Rejected accepted strategy confirmation due to execution resolution failure."
                    << "strategyID=" << p_request.strategyID << "requestID=" << p_request.strategyRequestID
                    << "symbol=" << p_request.orderRequest.getSymbol() << "reason=" << finalReason;
        }
        else
        {
            resolvedOrderRequest = resolvedOrder.value();
        }
    }

    notifyStrategyManualOrderDecision(p_request, finalDecision, finalReason);

    if (finalDecision == StrategyManualOrderDecision::Accepted)
    {
        ASSUME_TRUE(resolvedOrderRequest.has_value());

        PlaceOrderRequest orderToPlace = resolvedOrderRequest.value();
        if (p_request.userOverrideStopPrice > 0.0)
        {
            orderToPlace.setStopPrice(p_request.userOverrideStopPrice);
        }
        else if (p_request.orderRequest.getStopPrice().has_value() &&
                 p_request.orderRequest.getStopPrice().value() > 0.0)
        {
            orderToPlace.setStopPrice(p_request.orderRequest.getStopPrice().value());
        }

        processPlaceOrder(p_request.requestId, p_request.strategyID, orderToPlace, p_request.promise);
    }
    else
    {
        p_request.promise->addResult(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
        p_request.promise->finish();
    }
}

void MainAlgo::cancelDeferredClosePositionsRequests(const QString& p_reason)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    while (!m_deferredClosePositionsRequests.isEmpty())
    {
        DeferredClosePositionsRequest request = m_deferredClosePositionsRequests.dequeue();
        ASSUME_DIFF(request.promise.get(), nullptr);
        request.promise->addResult(std::unexpected(p_reason));
        request.promise->finish();
    }
}

void MainAlgo::finalizeActiveManualOrderConfirmation(const StrategyManualOrderDecision p_decision,
                                                     const QString& p_reason,
                                                     const bool p_resumeReplayIfNeeded)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    if (!m_activeManualOrderConfirmation.has_value())
    {
        return;
    }

    const ManualOrderConfirmationRequest request = m_activeManualOrderConfirmation.value();
    m_activeManualOrderConfirmation.reset();
    resetActiveManualOrderConfirmationCountdown();

    emit strategyOrderConfirmationResolved(request.confirmationID);
    finishManualOrderConfirmationRequest(request, p_decision, p_reason, p_resumeReplayIfNeeded);

    activateNextManualOrderConfirmationIfIdle();
    endManualConfirmationReplaySpeedOverrideIfIdle();
}

void MainAlgo::pauseActiveManualOrderConfirmationCountdown()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(m_manualOrderConfirmationTimer.get(), nullptr);

    if (!m_activeManualOrderConfirmation.has_value() || m_manualOrderConfirmationCountdownPaused)
    {
        return;
    }

    if (!m_manualOrderConfirmationTimer->isActive())
    {
        if (m_manualOrderConfirmationRemainingMs > 0)
        {
            m_manualOrderConfirmationCountdownPaused = true;
        }
        return;
    }

    const int remainingMs = m_manualOrderConfirmationTimer->remainingTime();
    m_manualOrderConfirmationTimer->stop();
    m_manualOrderConfirmationRemainingMs = std::max(0, remainingMs);
    m_manualOrderConfirmationCountdownPaused = true;
}

void MainAlgo::resumeActiveManualOrderConfirmationCountdown()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(m_manualOrderConfirmationTimer.get(), nullptr);

    if (!m_activeManualOrderConfirmation.has_value() || !m_manualOrderConfirmationCountdownPaused)
    {
        return;
    }

    m_manualOrderConfirmationCountdownPaused = false;
    if (m_manualOrderConfirmationRemainingMs <= 0)
    {
        onManualOrderConfirmationTimeout();
        return;
    }

    m_manualOrderConfirmationTimer->start(m_manualOrderConfirmationRemainingMs);
}

void MainAlgo::resetActiveManualOrderConfirmationCountdown()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(m_manualOrderConfirmationTimer.get(), nullptr);

    m_manualOrderConfirmationTimer->stop();
    m_manualOrderConfirmationRemainingMs = 0;
    m_manualOrderConfirmationCountdownPaused = false;
}

void MainAlgo::notifyStrategyManualOrderDecision(const ManualOrderConfirmationRequest& p_request,
                                                 const StrategyManualOrderDecision p_decision,
                                                 const QString& p_reason)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    if (!m_strategyManager)
    {
        return;
    }

    m_strategyManager->publishManualOrderDecision(p_request.strategyID,
                                                  p_request.strategyRequestID,
                                                  p_request.orderRequest.getSymbol(),
                                                  p_decision,
                                                  p_reason);
}

void MainAlgo::cancelManualOrderConfirmationsForStrategy(const QString& p_strategyID,
                                                         const QString& p_reason,
                                                         const bool p_resumeReplayIfNeeded)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (p_strategyID.isEmpty())
    {
        return;
    }

    QQueue<ManualOrderConfirmationRequest> kept;
    while (!m_pendingManualOrderConfirmations.isEmpty())
    {
        ManualOrderConfirmationRequest request = m_pendingManualOrderConfirmations.dequeue();
        if (request.strategyID == p_strategyID)
        {
            finishManualOrderConfirmationRequest(request,
                                                 StrategyManualOrderDecision::Cancelled,
                                                 p_reason,
                                                 p_resumeReplayIfNeeded);
            continue;
        }
        kept.enqueue(std::move(request));
    }
    m_pendingManualOrderConfirmations = std::move(kept);

    if (m_activeManualOrderConfirmation.has_value() && m_activeManualOrderConfirmation->strategyID == p_strategyID)
    {
        finalizeActiveManualOrderConfirmation(StrategyManualOrderDecision::Cancelled, p_reason, p_resumeReplayIfNeeded);
    }

    endManualConfirmationReplaySpeedOverrideIfIdle();
}

void MainAlgo::cancelAllManualOrderConfirmations(const QString& p_reason, const bool p_resumeReplayIfNeeded)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    while (!m_pendingManualOrderConfirmations.isEmpty())
    {
        ManualOrderConfirmationRequest request = m_pendingManualOrderConfirmations.dequeue();
        finishManualOrderConfirmationRequest(request,
                                             StrategyManualOrderDecision::Cancelled,
                                             p_reason,
                                             p_resumeReplayIfNeeded);
    }

    if (m_activeManualOrderConfirmation.has_value())
    {
        finalizeActiveManualOrderConfirmation(StrategyManualOrderDecision::Cancelled, p_reason, p_resumeReplayIfNeeded);
    }

    endManualConfirmationReplaySpeedOverrideIfIdle();
}

void MainAlgo::setManualOrderConfirmationsMuted(const bool p_muted)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (m_manualOrderConfirmationsMuted == p_muted)
    {
        return;
    }

    m_manualOrderConfirmationsMuted = p_muted;
    INFO << "Global manual-confirm mute mode set to" << m_manualOrderConfirmationsMuted;

    if (!m_manualOrderConfirmationsMuted)
    {
        return;
    }

    const QString reason = QStringLiteral("Rejected by user: manual confirmations muted");
    while (!m_pendingManualOrderConfirmations.isEmpty())
    {
        ManualOrderConfirmationRequest request = m_pendingManualOrderConfirmations.dequeue();
        finishManualOrderConfirmationRequest(request, StrategyManualOrderDecision::Rejected, reason, true);
    }

    if (m_activeManualOrderConfirmation.has_value())
    {
        finalizeActiveManualOrderConfirmation(StrategyManualOrderDecision::Rejected, reason, true);
    }

    endManualConfirmationReplaySpeedOverrideIfIdle();
}

void MainAlgo::onOrderResolved(uint64_t p_requestId, const std::expected<PlaceOrderResult, TSClient::Error>& p_result)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const bool isRiskEntryCandidate = m_requestIdToRiskEntryCandidate.take(p_requestId);

    // Look up and remove the promise
    auto promiseIt = m_pendingOrderPromises.find(p_requestId);
    OBJ_ASSUME_FALSE(promiseIt == m_pendingOrderPromises.end());
    auto promise = *promiseIt;
    m_pendingOrderPromises.erase(promiseIt);

    // Look up which strategy placed this order
    auto strategyIt = m_requestIdToStrategyId.find(p_requestId);

    // There is something catastrophically wrong if the requestID is not in the
    // map when the QFuture associated to it gets resolved here
    OBJ_ASSUME_FALSE(strategyIt == m_requestIdToStrategyId.end());

    QString strategyID = *strategyIt;
    m_requestIdToStrategyId.remove(p_requestId);

    auto requestIt = m_requestIdToPlacedOrderRequest.find(p_requestId);
    OBJ_ASSUME_FALSE(requestIt == m_requestIdToPlacedOrderRequest.end());
    const PlaceOrderRequest placedOrderRequest = *requestIt;
    m_requestIdToPlacedOrderRequest.erase(requestIt);

    // Resolve the promise with the result
    promise->addResult(p_result);
    promise->finish();

    if (p_result.has_value())
    {
        // Order was successfully placed
        PlaceOrderResult result = p_result.value();

        DEBUG << "Order placed successfully: strategyID="
              << (strategyID.isEmpty() ? QStringLiteral("<external>") : strategyID)
              << "successful=" << result.isAllSuccessful();

        // Extract OrderIDs from result and create permanent mappings
        const auto& orders = result.getOrders();
        for (const auto& orderResultItem: orders)
        {
            if (!orderResultItem.isError())
            {
                // Successful strategy order - create permanent mapping for future updates
                QString orderID = orderResultItem.getOrderID();
                if (!strategyID.isEmpty())
                {
                    m_orderMappings[orderID] = strategyID;
                    DEBUG << "Created order mapping: OrderID=" << orderID << "→ strategyID=" << strategyID
                          << "(total mappings=" << m_orderMappings.size() << ")";
                }

                m_orderIdToRiskEntryCandidate[orderID] = isRiskEntryCandidate;
                emitSyntheticOrderAckIfMissing(placedOrderRequest, orderID);

                // Promote any pending strategy log from requestId → orderID scope
                if (m_pendingOrderLogs.contains(p_requestId))
                {
                    const QString strategyLog = m_pendingOrderLogs.take(p_requestId);
                    m_orderIdToLog[orderID] = strategyLog;

                    // Persist immediately when the DB row already exists. If the stream insert has not
                    // happened yet, the first annotated stream update will write the same value later.
                    OrdersDatabase::getInstance()->updateOrderStrategyLog(orderID, strategyLog);

                    auto deferredIt = m_deferredOrderUpdates.find(orderID);
                    if (deferredIt != m_deferredOrderUpdates.end())
                    {
                        Order deferredOrder = deferredIt->order;
                        const QString deferredAccount = deferredIt->account;
                        m_deferredOrderUpdates.erase(deferredIt);
                        deferredOrder.setStrategyLog(strategyLog);
                        onReceivedNewOrder(deferredAccount, deferredOrder);
                    }
                }
            }
        }

        if (m_pendingOrderLogs.isEmpty())
        {
            m_deferredOrderUpdates.clear();
        }

        // TODO: Emit GUI signal if this order is for the displayed stock
        // TODO: Route order result to strategy via SDK
    }
    else
    {
        // Order placement failed - discard any pending log for this request
        m_pendingOrderLogs.remove(p_requestId);
        m_requestIdToRiskEntryCandidate.remove(p_requestId);

        TSClient::Error error = p_result.error();
        WARNING << "Order placement failed: requestId=" << p_requestId << "strategyID=" << strategyID
                << "error=" << QtEnum::toString(error);

        // TODO: Route error to strategy via SDK
    }

    if (m_pendingOrderLogs.isEmpty())
    {
        m_deferredOrderUpdates.clear();
    }
}

void MainAlgo::emitSyntheticOrderAckIfMissing(const PlaceOrderRequest& p_orderRequest, const QString& p_orderID)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const QString normalizedOrderID = p_orderID.trimmed();
    if (normalizedOrderID.isEmpty())
    {
        return;
    }

    OrdersDatabase* const ordersDb = OrdersDatabase::getInstance();
    if (ordersDb != nullptr && ordersDb->isOpen() && ordersDb->orderExists(normalizedOrderID))
    {
        return;
    }

    Order syntheticOrder(buildSyntheticPlacedOrderJson(p_orderRequest, normalizedOrderID), false);
    OBJ_ASSUME_TRUE(syntheticOrder.isValid());

    if (ordersDb != nullptr && ordersDb->isOpen())
    {
        const bool inserted = ordersDb->insertOrder(syntheticOrder);
        if (!inserted)
        {
            WARNING << "Failed to persist synthetic order ACK for order ID:" << normalizedOrderID;
        }
    }

    DEBUG << "Emitting synthetic order ACK for order ID:" << normalizedOrderID;
    onReceivedNewOrder(p_orderRequest.getAccountID(), syntheticOrder);
}

void MainAlgo::processCancelOrder(
    const QString& p_orderID,
    std::shared_ptr<QPromise<std::expected<CancelOrderResult, TSClient::Error>>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(p_promise.get(), nullptr);

    QFuture<std::expected<CancelOrderResult, TSClient::Error>> future = TSClient::getInstance()->cancelOrder(p_orderID);

    future.then(this,
                [p_promise](std::expected<CancelOrderResult, TSClient::Error> result)
                {
                    p_promise->addResult(result);
                    p_promise->finish();
                });

    DEBUG << "Processing cancelOrder: orderID=" << p_orderID;
}

void MainAlgo::processSubscribeToSymbol(const QString& p_strategyID,
                                        const QString& p_symbol,
                                        std::shared_ptr<QPromise<bool>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    ASSUME_DIFF(p_promise.get(), nullptr);

    if (MainApp::isInReplayMode())
    {
        // Validate data exists for the replay date
        if (!DBClient::hasReplayData(m_replayDate, p_symbol))
        {
            WARNING << "No replay data for" << p_symbol << "on" << m_replayDate.toString(Qt::ISODate)
                    << "- subscription rejected";
            p_promise->addResult(false);
            p_promise->finish();
            return;
        }

        // If symbol is already loaded (displayed stock), reuse the existing SymbolContext.
        if (m_symbolContexts.contains(p_symbol))
        {
            ++m_symbolContexts[p_symbol]->m_refCount;
            // Pass the actual SymbolContext so the strategy gets a direct connection (no MainAlgo hop)
            m_strategyManager->connectSymbolToStrategy(p_strategyID, p_symbol, m_symbolContexts[p_symbol]);
            p_promise->addResult(true);
            p_promise->finish();
            return;
        }

        // New secondary symbol: create SymbolContext — data flows automatically via
        // DBClient::newLevel2/newTrade → MainAlgo routing → SymbolContext queue.
        // Also open its replay files in DBClient so records get emitted.
        auto* instrument = new SymbolContext(p_symbol, this);
        Q_CHECK_PTR(instrument);
        instrument->m_refCount = 1; // Strategy claim
        {
            QWriteLocker lock(&m_symbolContextsLock);
            m_symbolContexts.insert(p_symbol, instrument);
        }

        // Open replay data files for this symbol (non-blocking, same-thread call)
        if (!DBClient::getInstance()->addReplaySymbol(p_symbol))
        {
            WARNING << "addReplaySymbol failed for" << p_symbol << "- no data files found";
        }

        // Wire bar-close to OrderEmulator if replay is active
        if (OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator())
            connectBarCloseToOrderEmulator(instrument, emulator);

        m_strategyManager->connectSymbolToStrategy(p_strategyID, p_symbol, instrument);

        INFO << "Secondary symbol SymbolContext created for" << p_symbol << "(replay data routed via DBClient)";
    }
    else
    {
        // Live/sim mode: create SymbolContext if needed and subscribe
        if (!m_symbolContexts.contains(p_symbol) || m_symbolContexts[p_symbol].isNull())
        {
            auto* instrument = new SymbolContext(p_symbol, this);
            Q_CHECK_PTR(instrument);
            instrument->m_refCount = 1; // Strategy claim
            {
                QWriteLocker lock(&m_symbolContextsLock);
                m_symbolContexts.insert(p_symbol, instrument);
            }

            instrument->barCache.warmCurrentDayCacheForLive(MainApp::getCurrentAppTime());
            subscribeLiveSymbol(instrument);
        }
        else
        {
            ++m_symbolContexts[p_symbol]->m_refCount;
        }

        m_strategyManager->connectSymbolToStrategy(p_strategyID, p_symbol, m_symbolContexts[p_symbol]);
    }

    p_promise->addResult(true);
    p_promise->finish();
}

QPointer<SymbolContext> MainAlgo::acquireSymbolContext(const QString& p_symbol)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const QString normalizedSymbol = p_symbol.trimmed().toUpper();
    QPointer<SymbolContext> sc;

    if (m_symbolContexts.contains(normalizedSymbol))
    {
        sc = m_symbolContexts[normalizedSymbol];
        DEBUG << "acquireSymbolContext: reusing existing SymbolContext for" << normalizedSymbol;
    }
    else
    {
        sc = new SymbolContext(normalizedSymbol, this);
        Q_CHECK_PTR(sc);

        {
            QWriteLocker lock(&m_symbolContextsLock);
            m_symbolContexts.insert(normalizedSymbol, sc);
        }

        sc->barCache.warmCurrentDayCacheForLive(MainApp::getCurrentAppTime());

        // Subscribe to data for the new symbol
        if (MainApp::isInReplayMode())
        {
            if (!DBClient::getInstance()->addReplaySymbol(normalizedSymbol))
                WARNING << "No replay data for" << normalizedSymbol << "- live bars will not flow";
        }
        else
        {
            subscribeLiveSymbol(sc);
        }

        DEBUG << "acquireSymbolContext: created new SymbolContext for" << normalizedSymbol;
    }

    ++sc->m_refCount;
    DEBUG << "acquireSymbolContext:" << normalizedSymbol << "refCount now" << sc->m_refCount;

    bool emittedBracket = false;
    for (auto it = m_managedBrackets.cbegin(); it != m_managedBrackets.cend(); ++it)
    {
        if (it->symbol == normalizedSymbol)
        {
            emitManagedBracketOverlay(*it, false);
            emittedBracket = true;
            break;
        }
    }
    if (!emittedBracket)
    {
        ManagedBracket clearState;
        clearState.symbol = normalizedSymbol;
        emitManagedBracketOverlay(clearState, true);
    }

    return sc;
}

void MainAlgo::releaseSymbolContextRef(const QString& symbol)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (!m_symbolContexts.contains(symbol))
    {
        WARNING << "releaseSymbolContextRef: no SymbolContext for" << symbol;
        return;
    }

    QPointer<SymbolContext> sc = m_symbolContexts[symbol];
    OBJ_ASSUME_DIFF(sc, nullptr);

    --sc->m_refCount;
    DEBUG << "releaseSymbolContextRef:" << symbol << "refCount now" << sc->m_refCount;

    if (sc->m_refCount <= 0)
    {
        {
            QWriteLocker lock(&m_symbolContextsLock);
            int removed = m_symbolContexts.remove(symbol);
            OBJ_ASSUME_EQUAL(removed, 1);
        }
        m_lastQuoteTradeBySymbol.remove(symbol);
        sc->deleteLater();
        DEBUG << "SymbolContext destroyed for" << symbol;
    }
}

void MainAlgo::processClaimSymbols(const QString& p_strategyID,
                                   const QStringList& p_symbols,
                                   std::shared_ptr<QPromise<QStringList>> p_promise)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    m_strategyManager->processClaimSymbols(p_strategyID, p_symbols, p_promise);
}

// onAggregatorBarUpdated/onAggregatorBarClosed removed — snapshot writes are now
// handled directly inside SymbolContext via DirectConnection to m_barAggregator.

// ---------------------------------------------------------------------------
// Replay time snapshot writer — replay time is a global concept, not per-SymbolContext
// ---------------------------------------------------------------------------

void MainAlgo::onReplayTimeReceived(const QDateTime& time)
{
    if (!m_currentDisplayedSymbolContext)
        return;

    LTTnG_TP(opentraderplatform,
             snapshot_write,
             m_currentDisplayedSymbolContext->symbol.toUtf8().constData(),
             "replayTime");
    QWriteLocker lock(&m_currentDisplayedSymbolContext->m_displaySnapshot.lock);
    m_currentDisplayedSymbolContext->m_displaySnapshot.replayTime = time;
    m_currentDisplayedSymbolContext->m_displaySnapshot.replayTimeDirty = true;
}

void MainAlgo::onReplayEndReached()
{
    INFO << "Replay ended, pausing heartbeat timers to prevent stream timeout";

    // Pause heartbeat timers on ALL mock streams, not just the displayed one
    for (auto& instrument: m_symbolContexts)
    {
        if (instrument.isNull())
        {
            continue;
        }
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::enterReplayMode(const QString& p_symbol, QDate p_date, QTime p_startTime, Playback::Speed p_speed)
{
    INFO << "MainAlgo entering replay mode for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    // Store replay state for strategy subscription validation
    m_replayDate = p_date;
    m_replayStartTime = p_startTime;
    m_replaySpeed = p_speed;
    m_manualConfirmationReplaySpeedOverrideActive = false;
    m_manualConfirmationReplaySpeedRestoreTarget.reset();
    m_requestIdToRiskEntryCandidate.clear();
    m_orderIdToRiskEntryCandidate.clear();
    m_countedRiskEntryFillOrderIds.clear();

    auto* dbClient = DBClient::getInstance();

    // Wire OrderEmulator to DBClient market data (same signals as live)
    connectReplaySignals(p_symbol);

    startReplayOrderStreams();

    dbClient->startReplay(p_symbol, p_date, p_startTime, p_speed);
}

void MainAlgo::enterReplayModePaused(const QString& p_symbol, QDate p_date, QTime p_startTime, Playback::Speed p_speed)
{
    INFO << "MainAlgo entering replay mode (paused) for" << p_symbol << "on" << p_date.toString(Qt::ISODate) << "at"
         << p_startTime.toString("hh:mm:ss");

    m_replayDate = p_date;
    m_replayStartTime = p_startTime;
    m_replaySpeed = p_speed;
    m_manualConfirmationReplaySpeedOverrideActive = false;
    m_manualConfirmationReplaySpeedRestoreTarget.reset();
    m_requestIdToRiskEntryCandidate.clear();
    m_orderIdToRiskEntryCandidate.clear();
    m_countedRiskEntryFillOrderIds.clear();

    auto* dbClient = DBClient::getInstance();

    bool isReentry = dbClient->isReplayActive();

    // If replay is already active (e.g., changing replay day), stop it first
    if (isReentry)
    {
        DEBUG << "Stopping existing replay before starting new one";
        dbClient->stopReplay();
    }

    // Forward DBClient replay lifecycle signals are wired once in onThreadStarted().

    connectReplaySignals(p_symbol);

    if (!isReentry)
    {
        startReplayOrderStreams();
    }

    dbClient->startReplayPaused(p_symbol, p_date, p_startTime, p_speed);

    // Pause heartbeat timers since we're starting in paused state
    for (auto& instrument: m_symbolContexts)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::connectBarCloseToOrderEmulator(SymbolContext* p_sc, OrderEmulator* p_emulator)
{
    OBJ_ASSUME_DIFF(p_sc, nullptr);
    OBJ_ASSUME_DIFF(p_emulator, nullptr);

    // Qt::UniqueConnection silently rejects lambda connections — use plain connection.
    // connectReplaySignals guards against re-entry at the call site.
    auto feedBarClose = [p_emulator](const QString& sym, const Bar& bar)
    { p_emulator->updateBarClose(sym, bar.getClose()); };

    connect(&p_sc->m_liveBarAccumulator, &LiveBarAccumulator::barUpdated, p_emulator, feedBarClose);
    connect(&p_sc->m_liveBarAccumulator, &LiveBarAccumulator::barClosed, p_emulator, feedBarClose);
}

void MainAlgo::connectReplaySignals(const QString& p_symbol)
{
    auto* dbClient = DBClient::getInstance();
    OrderEmulator* emulator = TSClient::getInstance()->getOrderEmulator();

    if (emulator)
    {
        // Replay Level2 → OrderEmulator (market data for order fills). UniqueConnection
        // guards against duplicate wiring on re-entry (e.g. preloadChartForReplay).
        connect(dbClient, &DBClient::newLevel2, emulator, &OrderEmulator::updateMarketDepth, Qt::UniqueConnection);

        // Bar close price → OrderEmulator for all active symbols (needed by recalculatePositionPnL).
        // Disconnect first per-SymbolContext to prevent duplicates on re-entry (preloadChart then
        // startReplay both call connectReplaySignals). UniqueConnection can't be used with lambdas.
        for (auto& sc: m_symbolContexts)
        {
            if (sc.isNull())
                continue;
            disconnect(&sc->m_liveBarAccumulator, &LiveBarAccumulator::barUpdated, emulator, nullptr);
            disconnect(&sc->m_liveBarAccumulator, &LiveBarAccumulator::barClosed, emulator, nullptr);
            connectBarCloseToOrderEmulator(sc, emulator);
        }

        emulator->setReplaySpeed(static_cast<int>(m_replaySpeed));
    }

    // Trade forwarding to DisplaySnapshot is now handled internally by SymbolContext::processTrade.

    INFO << "Replay signals connected for" << p_symbol;
}

void MainAlgo::exitReplayMode()
{
    INFO << "MainAlgo exiting replay mode";
    m_manualConfirmationReplaySpeedOverrideActive = false;
    m_manualConfirmationReplaySpeedRestoreTarget.reset();
    m_requestIdToRiskEntryCandidate.clear();
    m_orderIdToRiskEntryCandidate.clear();
    m_countedRiskEntryFillOrderIds.clear();

    auto* dbClient = DBClient::getInstance();
    dbClient->stopReplay();

    // Disconnect replay-specific signals from DBClient
    disconnect(dbClient, &DBClient::replayStarted, this, nullptr);
    disconnect(dbClient, &DBClient::replayStopped, this, nullptr);
    disconnect(dbClient, &DBClient::replayPaused, this, nullptr);
    disconnect(dbClient, &DBClient::replayResumed, this, nullptr);
    disconnect(dbClient, &DBClient::replayTimeUpdated, this, nullptr);
    disconnect(dbClient, &DBClient::replayEndReached, this, nullptr);
}

void MainAlgo::pauseReplay()
{
    pauseActiveManualOrderConfirmationCountdown();
    DBClient::getInstance()->pauseReplay();

    // Pause heartbeat timers on ALL streams to prevent timeout while paused
    for (auto& instrument: m_symbolContexts)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.pauseHeartbeat();
        instrument->m_level2Receiver.pauseHeartbeat();
    }
}

void MainAlgo::resumeReplay()
{
    // Resume heartbeat timers on ALL streams before resuming replay
    for (auto& instrument: m_symbolContexts)
    {
        OBJ_ASSUME_FALSE(instrument.isNull());
        instrument->barReceiver.resumeHeartbeat();
        instrument->m_level2Receiver.resumeHeartbeat();
    }

    DBClient::getInstance()->resumeReplay();
    resumeActiveManualOrderConfirmationCountdown();
}

void MainAlgo::setReplaySpeed(Playback::Speed p_speed)
{
    if (m_manualConfirmationReplaySpeedOverrideActive)
    {
        m_manualConfirmationReplaySpeedRestoreTarget = p_speed;
        if (p_speed != Playback::Speed::Normal)
        {
            INFO << "Replay speed change requested during manual confirmation; deferring to post-confirmation speed"
                 << static_cast<int>(p_speed);
        }
        applyReplaySpeedInternal(Playback::Speed::Normal);
        return;
    }

    applyReplaySpeedInternal(p_speed);
}

void MainAlgo::pauseLiveStreams()
{
    INFO << "Pausing live streams for replay mode";

    if (m_balancePollingStarted)
    {
        stopBalancePolling();
        m_balancePollingStarted = false;
    }

    // Receivers may be null if the user enters replay before account setup completed
    if (m_positionReceiver)
    {
        m_positionReceiver->stopStream(m_activeAccount.getAccountId());
        positionStreamStarted = false;
        DEBUG << "Positions stream stopped";
    }
    else
    {
        DEBUG << "No position stream to stop (not yet started)";
    }

    if (m_orderReceiver)
    {
        m_orderReceiver->stopStream(m_activeAccount.getAccountId());
        orderStreamStarted = false;
        DEBUG << "Orders stream stopped";
    }
    else
    {
        DEBUG << "No order stream to stop (not yet started)";
    }
}

void MainAlgo::teardownAccountReceivers()
{
    if (m_positionReceiver)
    {
        m_positionReceiver->stopStream(m_activeAccount.getAccountId());
        delete m_positionReceiver;
        m_positionReceiver = nullptr;
        DEBUG << "Positions receiver destroyed";
    }

    if (m_orderReceiver)
    {
        m_orderReceiver->stopStream(m_activeAccount.getAccountId());
        delete m_orderReceiver;
        m_orderReceiver = nullptr;
        DEBUG << "Orders receiver destroyed";
    }

    positionStreamStarted = false;
    orderStreamStarted = false;
}

void MainAlgo::startReplayOrderStreams()
{
    INFO << "Starting replay order/position streams with simulated account";

    // Get the simulated account ID from TSClient
    QString simAccountID = OrderEmulator::getSimulatedAccountID();

    // Create simulated account and update m_activeAccount
    QJsonObject accountJson;
    accountJson["AccountID"] = simAccountID;
    accountJson["AccountType"] = "Margin";
    accountJson["Name"] = "Replay Simulation Account";
    accountJson["Status"] = "Active";
    Account simAccount(accountJson);

    // Update active account to the simulated account
    m_activeAccount = simAccount;
    INFO << "Set active account to simulated account:" << simAccountID;

    // Delete existing receivers and create new ones with the simulated account
    teardownAccountReceivers();
    m_positionReceiver = new PositionsReceiver(simAccountID, this);
    bool connected = connect(m_positionReceiver,
                             &PositionsReceiver::receivedNewPosition,
                             this,
                             &MainAlgo::onReceivedNewPosition,
                             Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    connected = connect(m_positionReceiver,
                        &PositionsReceiver::positionDeleted,
                        this,
                        &MainAlgo::onPositionDeleted,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    connected = connect(m_positionReceiver,
                        &PositionsReceiver::loadedPositionsFromDatabase,
                        this,
                        &MainAlgo::onLoadedPositionsFromDatabase,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    m_positionReceiver->emitLoadedPositionsFromDatabase();
    positionStreamStarted = true;
    DEBUG << "Replay positions receiver created for" << simAccountID;

    m_orderReceiver = new OrdersReceiver(simAccountID, this);
    connected = connect(m_orderReceiver,
                        &OrdersReceiver::receivedNewOrder,
                        this,
                        &MainAlgo::onReceivedNewOrder,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    connected = connect(m_orderReceiver,
                        &OrdersReceiver::loadedOrdersFromDatabase,
                        this,
                        &MainAlgo::onLoadedOrdersFromDatabase,
                        Qt::UniqueConnection);
    ASSUME_TRUE(connected);
    m_orderReceiver->emitLoadedOrdersFromDatabase();
    orderStreamStarted = true;
    DEBUG << "Replay orders receiver created for" << simAccountID;

    // Emit simulated account to update GUI account selector
    QVector<Account> simAccounts;
    simAccounts.append(simAccount);
    emit tradeStationAccountsReceived(simAccounts);
    DEBUG << "Emitted simulated account for replay mode";

    // Start balance polling so the balances widget updates during replay
    if (!m_balancePollingStarted)
    {
        startBalancePolling();
        m_balancePollingStarted = true;
    }
}

void MainAlgo::resumeLiveStreams()
{
    if (m_isShuttingDown.load(std::memory_order_acquire))
    {
        INFO << "Skipping live-stream resume because MainAlgo is shutting down";
        return;
    }

    if (MainApp::getDataSourceMode() != DataSourceMode::Live)
    {
        INFO << "Skipping live-stream resume because the app is not in live mode";
        return;
    }

    INFO << "Resuming live streams while returning to live mode";

    // Skip account fetch if not authenticated (e.g. app launched directly into Review without TS credentials)
    if (!TSClient::getInstance()->isAuthenticated())
    {
        INFO << "Not authenticated with TradeStation — stopping balance polling and skipping live account fetch";
        stopBalancePolling();
        m_balancePollingStarted = false;
        return;
    }

    // Fetch real accounts from API; replay mode uses fake "SIM123456", and Review may have no live receivers yet.
    INFO << "Fetching real accounts from API while returning to live mode";
    const uint64_t requestGeneration =
        m_accountsRequestGeneration.fetch_add(1, std::memory_order_acq_rel) + static_cast<uint64_t>(1);
    QFuture<std::expected<QVector<Account>, TSClient::Error>> future = TSClient::getInstance()->getAccounts();

    future.then(
        [mainAlgo = QPointer<MainAlgo>(this),
         requestGeneration](std::expected<QVector<Account>, TSClient::Error> results) mutable
        {
            if (mainAlgo.isNull())
            {
                return;
            }

            QMetaObject::invokeMethod(
                mainAlgo,
                [mainAlgo, requestGeneration, results = std::move(results)]() mutable
                {
                    if (mainAlgo.isNull())
                    {
                        return;
                    }

                    if (mainAlgo->m_isShuttingDown.load(std::memory_order_acquire))
                    {
                        qCInfo(LOGGING_CATEGORY) << mainAlgo->objectName()
                                                 << "Dropping live account refresh result because MainAlgo is shutting "
                                                    "down";
                        return;
                    }

                    if (requestGeneration != mainAlgo->m_accountsRequestGeneration.load(std::memory_order_acquire))
                    {
                        qCInfo(LOGGING_CATEGORY)
                            << mainAlgo->objectName()
                            << "Dropping stale live account refresh result. requestGeneration=" << requestGeneration;
                        return;
                    }

                    if (MainApp::getDataSourceMode() != DataSourceMode::Live)
                    {
                        qCInfo(LOGGING_CATEGORY) << mainAlgo->objectName()
                                                 << "Discarding live account refresh because the app left live mode";
                        return;
                    }

                    if (!results.has_value())
                    {
                        qCCritical(LOGGING_CATEGORY)
                            << mainAlgo->objectName() << "Failed to fetch accounts while returning to live mode";
                        return;
                    }

                    QVector<Account> accounts = results.value();
                    if (accounts.isEmpty())
                    {
                        qCCritical(LOGGING_CATEGORY)
                            << mainAlgo->objectName() << "No accounts returned while returning to live mode";
                        return;
                    }

                    // Emit accounts to GUI so user can select
                    emit mainAlgo->tradeStationAccountsReceived(accounts);

                    // Select account using the same mode-based policy as startup.
                    if (MainApp::getTradingMode() == TradingMode::Live)
                    {
                        mainAlgo->m_activeAccount = accounts.first();
                    }
                    else
                    {
                        mainAlgo->m_activeAccount = accounts.last();
                    }
                    qCInfo(LOGGING_CATEGORY)
                        << mainAlgo->objectName() << "Using account" << mainAlgo->m_activeAccount.getAccountId()
                        << "after leaving replay/review mode for"
                        << (MainApp::getTradingMode() == TradingMode::Sim ? "SIM" : "LIVE");

                    // Delete and recreate receivers with real account
                    mainAlgo->teardownAccountReceivers();
                    mainAlgo->m_positionReceiver =
                        new PositionsReceiver(mainAlgo->m_activeAccount.getAccountId(), mainAlgo);
                    bool connected = connect(mainAlgo->m_positionReceiver,
                                             &PositionsReceiver::receivedNewPosition,
                                             mainAlgo,
                                             &MainAlgo::onReceivedNewPosition,
                                             Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    connected = connect(mainAlgo->m_positionReceiver,
                                        &PositionsReceiver::positionDeleted,
                                        mainAlgo,
                                        &MainAlgo::onPositionDeleted,
                                        Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    connected = connect(mainAlgo->m_positionReceiver,
                                        &PositionsReceiver::loadedPositionsFromDatabase,
                                        mainAlgo,
                                        &MainAlgo::onLoadedPositionsFromDatabase,
                                        Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    mainAlgo->m_positionReceiver->emitLoadedPositionsFromDatabase();
                    mainAlgo->positionStreamStarted = true;
                    qCDebug(LOGGING_CATEGORY) << mainAlgo->objectName() << "Positions receiver recreated and started";

                    mainAlgo->m_orderReceiver = new OrdersReceiver(mainAlgo->m_activeAccount.getAccountId(), mainAlgo);
                    connected = connect(mainAlgo->m_orderReceiver,
                                        &OrdersReceiver::receivedNewOrder,
                                        mainAlgo,
                                        &MainAlgo::onReceivedNewOrder,
                                        Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    connected = connect(mainAlgo->m_orderReceiver,
                                        &OrdersReceiver::loadedOrdersFromDatabase,
                                        mainAlgo,
                                        &MainAlgo::onLoadedOrdersFromDatabase,
                                        Qt::UniqueConnection);
                    ASSUME_TRUE(connected);
                    mainAlgo->m_orderReceiver->emitLoadedOrdersFromDatabase();
                    mainAlgo->orderStreamStarted = true;
                    qCDebug(LOGGING_CATEGORY) << mainAlgo->objectName() << "Orders receiver recreated and started";
                },
                Qt::QueuedConnection);
        });
}

Playback::State MainAlgo::getReplayState() const
{
    return DBClient::getInstance()->getPlaybackState();
}

MainAlgo::ActivityMetrics MainAlgo::getActivityMetrics(const QString& p_symbol) const
{
    QReadLocker lock(&m_symbolContextsLock);
    const QPointer<SymbolContext> symbolContext = m_symbolContexts.value(p_symbol);
    const SymbolContext* context = symbolContext.data();
    if (context == nullptr)
        return {};
    const ActivityTracker& activity = context->m_activity;
    return {activity.tradeRateHz(), activity.l2RateHz(), activity.isActive()};
}

std::expected<QPointer<SymbolContext>, QString> MainAlgo::leaseControlSymbolContext(const QString& p_symbol)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const QString symbol = p_symbol.trimmed().toUpper();
    if (symbol.isEmpty())
    {
        return std::unexpected("Symbol must not be empty");
    }

    QPointer<SymbolContext> sc = m_symbolContexts.value(symbol);
    if (!m_controlSymbolLeaseExpirations.contains(symbol) || sc.isNull())
    {
        sc = acquireSymbolContext(symbol);
        if (sc.isNull())
        {
            return std::unexpected(QString("Failed to acquire SymbolContext for %1").arg(symbol));
        }
    }

    m_controlSymbolLeaseExpirations[symbol] =
        QDateTime::currentDateTimeUtc().addMSecs(PlatformControlConstants::SYMBOL_CONTEXT_LEASE_TIMEOUT_MS);
    return sc;
}

void MainAlgo::pruneExpiredControlSymbolLeases()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = m_controlSymbolLeaseExpirations.begin(); it != m_controlSymbolLeaseExpirations.end();)
    {
        if (it.value() > now)
        {
            ++it;
            continue;
        }

        const QString symbol = it.key();
        it = m_controlSymbolLeaseExpirations.erase(it);
        releaseSymbolContextRef(symbol);
    }
}

std::expected<MainAlgo::MarketDataSnapshot, QString> MainAlgo::getMarketDataSnapshot(const QString& p_symbol,
                                                                                     const int p_maxTrades)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    if (p_maxTrades <= 0)
    {
        return std::unexpected("maxCount must be greater than 0");
    }

    QString symbol = p_symbol.trimmed().toUpper();
    if (symbol.isEmpty())
    {
        symbol = getDisplayedSymbol();
    }
    if (symbol.isEmpty())
    {
        return std::unexpected("No symbol was provided and no symbol is currently displayed");
    }

    const auto leased = leaseControlSymbolContext(symbol);
    if (!leased.has_value())
    {
        return std::unexpected(leased.error());
    }

    QPointer<SymbolContext> sc = leased.value();
    OBJ_ASSUME_DIFF(sc, nullptr);

    MarketDataSnapshot snapshot;
    snapshot.symbol = symbol;
    const qint64 deadlineMs = QDateTime::currentMSecsSinceEpoch() + kMarketDataSnapshotWaitMs;
    do
    {
        snapshot.activity = {sc->m_activity.tradeRateHz(), sc->m_activity.l2RateHz(), sc->m_activity.isActive()};
        snapshot.recentTrades.clear();

        QReadLocker lock(&sc->m_displaySnapshot.lock);
        snapshot.latestLevel2 = sc->m_displaySnapshot.latestLevel2;
        snapshot.replayTime = sc->m_displaySnapshot.replayTime;

        const QVector<Trade>& recentTrades = sc->m_displaySnapshot.recentTrades;
        const int availableTrades = recentTrades.size();
        const int copyCount = std::min(p_maxTrades, availableTrades);
        snapshot.recentTrades.reserve(copyCount);
        for (int index = availableTrades - copyCount; index < availableTrades; ++index)
        {
            snapshot.recentTrades.append(recentTrades.at(index));
        }

        if (snapshot.latestLevel2.has_value() || QDateTime::currentMSecsSinceEpoch() >= deadlineMs)
        {
            break;
        }

        QThread::msleep(kMarketDataSnapshotPollMs);
    } while (true);

    return snapshot;
}

void MainAlgo::deleteAllSymbolContext()
{
    INFO << "Deleting all stock instruments for clean mode transition";

    // Clear the displayed pointer first
    m_currentDisplayedSymbolContext = nullptr;
    m_controlSymbolLeaseExpirations.clear();
    m_lastQuoteTradeBySymbol.clear();

    // Delete instruments directly (not deleteLater) so that each BarCache destructor
    // queues closeDatabase to DatabaseThread before the next createAndSetDisplayedSymbolContext
    // queues openDatabase. deleteLater would defer destruction past the next openDatabase call,
    // causing the DB close to arrive on DatabaseThread after the new open — breaking the connection.
    {
        QWriteLocker lock(&m_symbolContextsLock);
        for (auto it = m_symbolContexts.begin(); it != m_symbolContexts.end(); ++it)
        {
            if (SymbolContext* instrument = it.value(); instrument)
            {
                DEBUG << "Deleting stock instrument for" << instrument->symbol;
                delete instrument;
            }
        }
        m_symbolContexts.clear();
    }

    INFO << "All stock instruments deleted";
}

void MainAlgo::stopAllStrategies()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    INFO << "Stopping all strategies for mode transition";
    cancelAllManualOrderConfirmations(QStringLiteral("All strategies were stopped"), false);
    m_strategyManager->stopAllStrategies();

    for (auto it = m_strategyStatusesBySymbol.cbegin(); it != m_strategyStatusesBySymbol.cend(); ++it)
    {
        StrategyStatusEntry clearEntry;
        clearEntry.symbol = it.key();
        clearEntry.timestamp = MainApp::getCurrentAppTime();
        clearEntry.action = StrategyStatusEntry::Action::Clear;
        emit strategyStatusEmitted(clearEntry);
    }
    m_strategyStatusesBySymbol.clear();

    INFO << "All strategies stopped";
}

void MainAlgo::restoreStrategiesState()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    m_strategyManager->restoreStrategiesState();
}

void MainAlgo::createAndSetDisplayedSymbolContext(const QString& p_symbol)
{
    INFO << "Creating and setting displayed stock instrument for" << p_symbol;

    // Create new stock instrument
    auto* newInstrument = new SymbolContext(p_symbol, this);
    Q_CHECK_PTR(newInstrument);

    m_symbolContexts[p_symbol] = newInstrument;
    m_currentDisplayedSymbolContext = newInstrument;

    // Claim display reference (matches the release in onSelectDisplayedStock)
    ++newInstrument->m_refCount;

    // Subscribe to data for the new symbol
    if (MainApp::isInReplayMode())
    {
        if (!DBClient::getInstance()->addReplaySymbol(p_symbol))
            WARNING << "No replay data for" << p_symbol << "- live bars will not flow";
    }
    else if (!MainApp::isInReviewMode())
    {
        newInstrument->barCache.warmCurrentDayCacheForLive(MainApp::getCurrentAppTime());
        subscribeLiveSymbol(newInstrument);
    }

    // No snapshot-writing connections needed here — SymbolContext always populates
    // its own DisplaySnapshot. The GUI reads from it at 30 Hz.

    INFO << "Stock instrument created and set as displayed for" << p_symbol;
}

void MainAlgo::processStrategyLog(const StrategyLogEntry& p_entry)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_FALSE(p_entry.symbol.isEmpty());
    OBJ_ASSUME_FALSE(p_entry.message.isEmpty());

    OrdersDatabase::getInstance()->insertStrategyLog(p_entry);
    emit strategyLogEmitted(p_entry);
}

void MainAlgo::processStrategyStatus(const StrategyStatusEntry& p_entry)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_FALSE(p_entry.symbol.isEmpty());

    const QString normalizedSymbol = p_entry.symbol.trimmed().toUpper();
    OBJ_ASSUME_FALSE(normalizedSymbol.isEmpty());

    if (p_entry.action == StrategyStatusEntry::Action::Clear)
    {
        processStrategyStatusClear(p_entry.strategyID, normalizedSymbol);
        return;
    }

    OBJ_ASSUME_FALSE(p_entry.message.isEmpty());

    StrategyStatusEntry entry = p_entry;
    entry.symbol = normalizedSymbol;
    entry.timestamp = MainApp::getCurrentAppTime();
    entry.action = StrategyStatusEntry::Action::Upsert;

    m_strategyStatusesBySymbol.insert(normalizedSymbol, entry);
    emit strategyStatusEmitted(entry);
}

void MainAlgo::processStrategyStatusClear(const QString& p_strategyID, const QString& p_symbol)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const QString normalizedSymbol = p_symbol.trimmed().toUpper();
    if (normalizedSymbol.isEmpty())
    {
        return;
    }

    auto it = m_strategyStatusesBySymbol.find(normalizedSymbol);
    if (it != m_strategyStatusesBySymbol.end())
    {
        if (!p_strategyID.isEmpty() && !it->strategyID.isEmpty() && it->strategyID != p_strategyID)
        {
            WARNING << "Ignoring strategy status clear for" << normalizedSymbol << "from" << p_strategyID
                    << "because status is owned by" << it->strategyID;
            return;
        }

        m_strategyStatusesBySymbol.erase(it);
    }

    StrategyStatusEntry clearEntry;
    clearEntry.strategyID = p_strategyID;
    clearEntry.symbol = normalizedSymbol;
    clearEntry.timestamp = MainApp::getCurrentAppTime();
    clearEntry.action = StrategyStatusEntry::Action::Clear;
    emit strategyStatusEmitted(clearEntry);
}

void MainAlgo::processStrategyChartDisplaySwitchRequest(const QString& p_strategyID,
                                                        const QString& p_symbol,
                                                        const QString& p_reason)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const QString symbol = p_symbol.trimmed().toUpper();
    const QString reason = p_reason.trimmed();
    if (symbol.isEmpty())
    {
        WARNING << "Ignored strategy chart-display switch request with empty symbol. strategyID=" << p_strategyID
                << "reason=" << (reason.isEmpty() ? QStringLiteral("<none>") : reason);
        return;
    }

    if (m_manualOrderConfirmationsMuted)
    {
        INFO << "Ignored strategy chart-display switch request while global manual-confirm mute is active."
             << "strategyID=" << p_strategyID << "symbol=" << symbol
             << "reason=" << (reason.isEmpty() ? QStringLiteral("<none>") : reason);
        return;
    }

    const QString currentSymbol = getDisplayedSymbol().trimmed().toUpper();
    if (!currentSymbol.isEmpty() && currentSymbol == symbol)
    {
        DEBUG << "Ignored strategy chart-display switch request because symbol is already displayed."
              << "strategyID=" << p_strategyID << "symbol=" << symbol
              << "reason=" << (reason.isEmpty() ? QStringLiteral("<none>") : reason);
        return;
    }

    INFO << "Accepted strategy chart-display switch request." << "strategyID=" << p_strategyID
         << "fromSymbol=" << currentSymbol << "toSymbol=" << symbol
         << "reason=" << (reason.isEmpty() ? QStringLiteral("<none>") : reason);

    emit strategyDisplaySymbolRequested(p_strategyID, symbol, reason);
}

std::optional<StrategyStatusEntry> MainAlgo::getLatestStrategyStatusForSymbol(const QString& p_symbol) const
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);

    const QString normalizedSymbol = p_symbol.trimmed().toUpper();
    if (normalizedSymbol.isEmpty())
    {
        return std::nullopt;
    }

    auto it = m_strategyStatusesBySymbol.constFind(normalizedSymbol);
    if (it == m_strategyStatusesBySymbol.cend())
    {
        return std::nullopt;
    }

    return it.value();
}

QString MainAlgo::managedBracketKey(const QString& p_accountID, const QString& p_symbol)
{
    return p_accountID.trimmed().toUpper() + '\n' + p_symbol.trimmed().toUpper();
}

void MainAlgo::emitManagedBracketOverlay(const ManagedBracket& p_bracket, const bool p_clear)
{
    StrategyBracketOverlayEntry entry;
    entry.strategyID = p_bracket.sourceID;
    entry.symbol = p_bracket.symbol;
    entry.timestamp = MainApp::getCurrentAppTime();
    entry.action = p_clear ? StrategyBracketOverlayEntry::Action::Clear : StrategyBracketOverlayEntry::Action::Upsert;
    entry.side = p_bracket.side;
    entry.stopPrice = p_bracket.stopPrice;
    entry.takePrice = p_bracket.takePrice;
    entry.referenceEntryPrice = p_bracket.referenceEntryPrice;
    entry.triggered = p_bracket.triggered;
    entry.triggerReason = p_bracket.triggerReason;

    emit managedBracketOverlayEmitted(entry);
}

void MainAlgo::updateManagedBracketQuantity(ManagedBracket& p_bracket)
{
    int quantity = 0;
    StrategyBracketOverlayEntry::Side side = p_bracket.side;
    for (auto it = m_currentPositions.cbegin(); it != m_currentPositions.cend(); ++it)
    {
        const Position& position = it.value();
        if (position.getAccountID() != p_bracket.accountID || position.getSymbol() != p_bracket.symbol)
        {
            continue;
        }

        const auto parsedQty = parsePositionQuantityShares(position);
        if (!parsedQty.has_value())
        {
            continue;
        }

        quantity += parsedQty.value();
        if (position.getLongShort() == "Short")
        {
            side = StrategyBracketOverlayEntry::Side::Short;
        }
        else
        {
            side = StrategyBracketOverlayEntry::Side::Long;
        }
    }

    p_bracket.protectedQuantity = quantity;
    p_bracket.side = side;
}

std::optional<Level2> MainAlgo::resolveManagedBracketPricingLevel2(const ManagedBracket& p_bracket,
                                                                   QString* p_source) const
{
    QPointer<SymbolContext> sc;
    {
        QReadLocker lock(&m_symbolContextsLock);
        sc = m_symbolContexts.value(p_bracket.symbol);
    }

    if (!sc.isNull())
    {
        QReadLocker snapshotLock(&sc->m_displaySnapshot.lock);
        if (sc->m_displaySnapshot.latestLevel2.has_value())
        {
            if (p_source != nullptr)
            {
                *p_source = QStringLiteral("display-snapshot");
            }
            return sc->m_displaySnapshot.latestLevel2.value();
        }
    }

    for (auto it = m_currentPositions.cbegin(); it != m_currentPositions.cend(); ++it)
    {
        const Position& position = it.value();
        if (position.getAccountID() != p_bracket.accountID || position.getSymbol() != p_bracket.symbol)
        {
            continue;
        }

        const auto fallbackLevel2 = level2FromPositionBbo(position);
        if (fallbackLevel2.has_value())
        {
            if (p_source != nullptr)
            {
                *p_source = QStringLiteral("position-bbo");
            }
            return fallbackLevel2;
        }
    }

    if (p_source != nullptr)
    {
        *p_source = QStringLiteral("unavailable");
    }
    return std::nullopt;
}

std::optional<double> MainAlgo::managedBracketTriggerPrice(const ManagedBracket& p_bracket) const
{
    QPointer<SymbolContext> sc;
    {
        QReadLocker lock(&m_symbolContextsLock);
        sc = m_symbolContexts.value(p_bracket.symbol);
    }
    if (sc.isNull())
    {
        return std::nullopt;
    }

    QReadLocker snapshotLock(&sc->m_displaySnapshot.lock);
    if (!sc->m_displaySnapshot.recentTrades.isEmpty())
    {
        const double lastTrade = sc->m_displaySnapshot.recentTrades.last().m_price;
        if (lastTrade > 0.0)
        {
            return lastTrade;
        }
    }

    if (sc->m_displaySnapshot.latestLevel2.has_value())
    {
        const Level2& level2 = sc->m_displaySnapshot.latestLevel2.value();
        const double bestBid = level2.m_bids[0].m_price;
        const double bestAsk = level2.m_asks[0].m_price;
        if (bestBid > 0.0 && bestAsk > 0.0)
        {
            return (bestBid + bestAsk) / 2.0;
        }
        if (bestBid > 0.0)
        {
            return bestBid;
        }
        if (bestAsk > 0.0)
        {
            return bestAsk;
        }
    }

    return std::nullopt;
}

bool MainAlgo::submitManagedBracketNativeLeg(ManagedBracket& p_bracket, const bool p_takeLeg)
{
    if (p_bracket.protectedQuantity <= 0)
    {
        return false;
    }

    PlaceOrderRequest request;
    request.setAccountID(p_bracket.accountID);
    request.setSymbol(p_bracket.symbol);
    request.setQuantity(p_bracket.protectedQuantity);
    request.setTimeInForce(TimeInForce(OrderDuration::Day));
    request.setTradeAction(p_bracket.side == StrategyBracketOverlayEntry::Side::Long ? TradeAction::Sell
                                                                                     : TradeAction::BuyToCover);
    request.setOcaGroupName(p_bracket.nativeOcaGroupName);
    request.setOcaGroupType(QStringLiteral("OCO"));

    if (p_takeLeg)
    {
        request.setOrderType(OrderType::Type::Limit);
        request.setLimitPrice(p_bracket.takePrice);
    }
    else
    {
        request.setOrderType(OrderType::Type::StopMarket);
        request.setStopPrice(p_bracket.stopPrice);
    }

    if (MainApp::isInReplayMode() || TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
    {
        request.setRoute(QStringLiteral("replay"));
    }

    DEBUG << "Managed bracket native leg submit:" << p_bracket.symbol << "account=" << p_bracket.accountID
          << "leg=" << (p_takeLeg ? "take" : "stop") << "qty=" << p_bracket.protectedQuantity
          << "limit=" << (p_takeLeg ? p_bracket.takePrice : 0.0) << "stop=" << (p_takeLeg ? 0.0 : p_bracket.stopPrice)
          << "ocoGroup=" << p_bracket.nativeOcaGroupName;

    const QString key = managedBracketKey(p_bracket.accountID, p_bracket.symbol);
    TSClient::getInstance()->placeOrder(request).then(
        this,
        [this, key, p_takeLeg](std::expected<PlaceOrderResult, TSClient::Error> p_result)
        {
            auto it = m_managedBrackets.find(key);
            if (it == m_managedBrackets.end())
            {
                return;
            }

            if (!p_result.has_value() || p_result->hasErrors() || p_result->getOrders().isEmpty())
            {
                QString reason;
                if (!p_result.has_value())
                {
                    reason = QStringLiteral("TSClientError: %1").arg(tsClientErrorToString(p_result.error()));
                }
                else if (p_result->hasErrors())
                {
                    QStringList brokerErrors;
                    for (const OrderResultItem& errorItem: p_result->getErrors())
                    {
                        if (errorItem.getError().has_value() && !errorItem.getError()->trimmed().isEmpty())
                        {
                            brokerErrors.append(errorItem.getError()->trimmed());
                            continue;
                        }
                        if (!errorItem.getMessage().trimmed().isEmpty())
                        {
                            brokerErrors.append(errorItem.getMessage().trimmed());
                        }
                    }

                    reason = brokerErrors.isEmpty() ? QStringLiteral("broker returned placement errors")
                                                    : brokerErrors.join(QStringLiteral(" | "));
                }
                else
                {
                    reason = QStringLiteral("broker response had no placed orders");
                }

                const QString legName = p_takeLeg ? QStringLiteral("take") : QStringLiteral("stop");
                handleManagedBracketNativeFailure(
                    key,
                    *it,
                    QStringLiteral("native-%1 leg placement failed: %2").arg(legName, reason));
                return;
            }

            const QString orderID = p_result->getOrders().first().getOrderID();
            if (orderID.isEmpty())
            {
                const QString legName = p_takeLeg ? QStringLiteral("take") : QStringLiteral("stop");
                handleManagedBracketNativeFailure(
                    key,
                    *it,
                    QStringLiteral("native-%1 leg placement returned empty order ID").arg(legName));
                return;
            }

            if (p_takeLeg)
            {
                it->nativeTakeOrderID = orderID;
            }
            else
            {
                it->nativeStopOrderID = orderID;
            }

            DEBUG << "Managed bracket native leg tracked order:" << it->symbol
                  << "leg=" << (p_takeLeg ? "take" : "stop") << "orderID=" << orderID;

            if (it->nativeFailureDropLatched)
            {
                WARNING << "Late native managed-bracket order accepted after drop latch; cancelling order" << orderID
                        << "for" << it->symbol;
                cancelManagedBracketNativeOrders(*it);
            }
        });
    return true;
}

bool MainAlgo::submitManagedBracketNativeOrders(ManagedBracket& p_bracket)
{
    if (p_bracket.protectedQuantity <= 0)
    {
        return false;
    }
    if (p_bracket.nativeOcaGroupName.isEmpty())
    {
        p_bracket.nativeOcaGroupName =
            QString("BRK_%1_%2_%3")
                .arg(p_bracket.accountID, p_bracket.symbol, QString::number(QDateTime::currentMSecsSinceEpoch()));
    }

    p_bracket.nativeStopOrderID.clear();
    p_bracket.nativeTakeOrderID.clear();

    const bool stopSent = submitManagedBracketNativeLeg(p_bracket, false);
    const bool takeSent = submitManagedBracketNativeLeg(p_bracket, true);
    return stopSent && takeSent;
}

void MainAlgo::cancelManagedBracketNativeOrders(ManagedBracket& p_bracket)
{
    auto cancelIfPresent = [this](QString& p_orderID)
    {
        if (p_orderID.isEmpty())
        {
            return;
        }
        const QString orderID = p_orderID;
        p_orderID.clear();
        TSClient::getInstance()->cancelOrder(orderID).then(
            [orderID](std::expected<CancelOrderResult, TSClient::Error> p_result)
            {
                if (!p_result.has_value())
                {
                    qCWarning(MainAlgoLog) << "Managed bracket cancel failed for order" << orderID
                                           << "error=" << tsClientErrorToString(p_result.error());
                }
            });
    };

    cancelIfPresent(p_bracket.nativeStopOrderID);
    cancelIfPresent(p_bracket.nativeTakeOrderID);
}

bool MainAlgo::shouldDropManagedBracketAfterNativeFailure(const ManagedBracket& p_bracket) const
{
    Q_UNUSED(p_bracket);
    return !MainApp::isInReplayMode() && MainApp::getCurrentSession() == TradingSession::Regular;
}

void MainAlgo::handleManagedBracketNativeFailure(const QString& p_key,
                                                 ManagedBracket& p_bracket,
                                                 const QString& p_reason)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    Q_UNUSED(p_key);

    const QString reason = p_reason.trimmed().isEmpty() ? QStringLiteral("native bracket failure") : p_reason.trimmed();
    if (shouldDropManagedBracketAfterNativeFailure(p_bracket))
    {
        if (!p_bracket.nativeFailureDropLatched)
        {
            WARNING << "Native managed bracket failed in regular live/sim session; dropping protection for"
                    << p_bracket.symbol << "account=" << p_bracket.accountID << "reason=" << reason;
            p_bracket.nativeFailureDropLatched = true;
            p_bracket.usingNativeOrders = false;
            p_bracket.triggered = false;
            p_bracket.triggerReason.clear();
            emitManagedBracketOverlay(p_bracket, true);
            emit managedBracketProtectionDropped(p_bracket.accountID, p_bracket.symbol, reason);
        }
        else
        {
            DEBUG << "Native failure drop already latched for" << p_bracket.symbol << "reason=" << reason;
        }

        cancelManagedBracketNativeOrders(p_bracket);
        return;
    }

    WARNING << "Managed bracket native leg placement failed for" << p_bracket.symbol << "- falling back to virtual mode"
            << "reason=" << reason;
    p_bracket.usingNativeOrders = false;
    cancelManagedBracketNativeOrders(p_bracket);
    emitManagedBracketOverlay(p_bracket, false);
}

bool MainAlgo::submitManagedBracketVirtualExitOrder(ManagedBracket& p_bracket)
{
    if (p_bracket.protectedQuantity <= 0)
    {
        return false;
    }

    if (!MainApp::isInReplayMode() && p_bracket.accountID == OrderEmulator::getSimulatedAccountID())
    {
        WARNING << "Managed bracket virtual exit skipped for replay account after leaving replay mode:"
                << p_bracket.symbol << "account=" << p_bracket.accountID;
        return false;
    }

    const TradeAction closeAction =
        p_bracket.side == StrategyBracketOverlayEntry::Side::Long ? TradeAction::Sell : TradeAction::BuyToCover;
    const TradingSession session = MainApp::getCurrentSession();

    PlaceOrderRequest request;
    request.setAccountID(p_bracket.accountID);
    request.setSymbol(p_bracket.symbol);
    request.setQuantity(p_bracket.protectedQuantity);
    request.setTradeAction(closeAction);

    if (session == TradingSession::Regular)
    {
        request.setOrderType(OrderType::Type::Market);
        request.setTimeInForce(TimeInForce(OrderDuration::Day));
    }
    else
    {
        QString level2Source;
        const auto latestLevel2 = resolveManagedBracketPricingLevel2(p_bracket, &level2Source);
        if (!latestLevel2.has_value())
        {
            WARNING << "Managed bracket virtual exit skipped for" << p_bracket.symbol
                    << "because pricing data is unavailable in extended session";
            return false;
        }

        const double offsetCents =
            appStateSettings == nullptr
                ? ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS
                : appStateSettings
                      ->value(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS,
                              ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS)
                      .toDouble();
        const auto aggressivePrice = calculateAggressiveMarketableLimitPrice(*latestLevel2, closeAction, offsetCents);
        if (!aggressivePrice.has_value())
        {
            WARNING << "Managed bracket virtual exit pricing failed for" << p_bracket.symbol
                    << "source=" << level2Source << "error=" << aggressivePrice.error();
            return false;
        }

        request.setOrderType(OrderType::Type::Limit);
        request.setLimitPrice(aggressivePrice.value());
        request.setTimeInForce(TimeInForce(OrderDuration::DayPlus));

        DEBUG << "Managed bracket virtual exit submit:" << p_bracket.symbol << "reason=" << p_bracket.triggerReason
              << "qty=" << p_bracket.protectedQuantity << "action=" << tradeActionToText(closeAction)
              << "limit=" << aggressivePrice.value() << "source=" << level2Source;
    }

    if (MainApp::isInReplayMode() || TSClient::getInstance()->getMode() == TSClient::Mode::Replay)
    {
        request.setRoute(QStringLiteral("replay"));
    }

    const QString key = managedBracketKey(p_bracket.accountID, p_bracket.symbol);
    p_bracket.virtualExitOrderPending = true;
    p_bracket.virtualExitOrderID.clear();
    p_bracket.virtualExitReplaceInProgress = false;
    p_bracket.virtualExitLastSubmitTime = MainApp::getCurrentAppTime();
    TSClient::getInstance()->placeOrder(request).then(
        this,
        [this, key](std::expected<PlaceOrderResult, TSClient::Error> p_result)
        {
            auto it = m_managedBrackets.find(key);
            if (it == m_managedBrackets.end())
            {
                return;
            }

            if (!p_result.has_value() || p_result->hasErrors() || p_result->getOrders().isEmpty())
            {
                it->virtualExitOrderPending = false;
                it->virtualExitOrderID.clear();
                it->virtualExitReplaceInProgress = false;
                WARNING << "Managed bracket virtual exit placement failed for" << it->symbol;
                return;
            }

            const QString orderID = p_result->getOrders().first().getOrderID();
            if (orderID.isEmpty())
            {
                it->virtualExitOrderPending = false;
                it->virtualExitOrderID.clear();
                it->virtualExitReplaceInProgress = false;
                WARNING << "Managed bracket virtual exit placement returned empty order ID for" << it->symbol;
                return;
            }

            // Keep virtualExitOrderPending=true until this tracked order reaches a terminal
            // order update or the protected position is gone. This prevents duplicate exits.
            it->virtualExitOrderID = orderID;
            DEBUG << "Managed bracket virtual exit tracked order:" << it->symbol << "orderID=" << orderID;
        });

    return true;
}

void MainAlgo::processUpsertManagedBracket(const QString& p_sourceID,
                                           const QString& p_accountID,
                                           const QString& p_symbol,
                                           const StrategyBracketSide p_side,
                                           const double p_stopPrice,
                                           const double p_takePrice,
                                           const StrategyBracketExecutionPolicy p_executionPolicy,
                                           const std::optional<double>& p_referenceEntryPrice)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    OBJ_ASSUME_FALSE(p_accountID.isEmpty());
    OBJ_ASSUME_FALSE(p_symbol.isEmpty());
    OBJ_ASSUME_GT(p_stopPrice, 0.0);
    OBJ_ASSUME_GT(p_takePrice, 0.0);

    const QString accountID = p_accountID.trimmed().toUpper();
    const QString symbol = p_symbol.trimmed().toUpper();
    const QString key = managedBracketKey(accountID, symbol);
    DEBUG << "Upserting managed bracket source=" << p_sourceID << "account=" << accountID << "symbol=" << symbol
          << "policy=" << managedBracketPolicyToString(p_executionPolicy) << "stop=" << p_stopPrice
          << "take=" << p_takePrice;

    auto existing = m_managedBrackets.find(key);
    if (existing != m_managedBrackets.end())
    {
        cancelManagedBracketNativeOrders(*existing);
    }

    ManagedBracket bracket;
    bracket.sourceID = p_sourceID;
    bracket.accountID = accountID;
    bracket.symbol = symbol;
    bracket.side = toOverlaySide(p_side);
    bracket.stopPrice = std::max(0.01, p_stopPrice);
    bracket.takePrice = std::max(0.01, p_takePrice);
    if (bracket.side == StrategyBracketOverlayEntry::Side::Long && bracket.stopPrice >= bracket.takePrice)
    {
        bracket.takePrice = bracket.stopPrice + 0.01;
    }
    if (bracket.side == StrategyBracketOverlayEntry::Side::Short && bracket.stopPrice <= bracket.takePrice)
    {
        bracket.takePrice = std::max(0.01, bracket.stopPrice - 0.01);
    }
    bracket.requestedExecutionPolicy = p_executionPolicy;
    bracket.usingNativeOrders = false;
    bracket.triggered = false;
    bracket.triggerReason.clear();
    bracket.protectedQuantity = 0;
    bracket.nativeFailureDropLatched = false;
    bracket.virtualExitOrderPending = false;
    bracket.virtualExitOrderID.clear();
    bracket.virtualExitReplaceInProgress = false;
    bracket.virtualExitLastSubmitTime = QDateTime();
    bracket.virtualExitLastCancelRequestTime = QDateTime();
    bracket.armTimestamp = MainApp::getCurrentAppTime();
    bracket.referenceEntryPrice = 0.0;

    if (p_referenceEntryPrice.has_value() && p_referenceEntryPrice.value() > 0.0)
    {
        bracket.referenceEntryPrice = p_referenceEntryPrice.value();
    }

    updateManagedBracketQuantity(bracket);
    if (bracket.referenceEntryPrice <= 0.0)
    {
        const auto inferredReference = managedBracketTriggerPrice(bracket);
        if (inferredReference.has_value() && inferredReference.value() > 0.0)
        {
            bracket.referenceEntryPrice = inferredReference.value();
        }
    }
    if (bracket.referenceEntryPrice <= 0.0)
    {
        bracket.referenceEntryPrice = (bracket.stopPrice + bracket.takePrice) / 2.0;
    }

    const TradingSession session = MainApp::getCurrentSession();
    const bool canUseNativeNow =
        (session == TradingSession::Regular) && (p_executionPolicy == StrategyBracketExecutionPolicy::Auto ||
                                                 p_executionPolicy == StrategyBracketExecutionPolicy::NativeOnly);
    bracket.usingNativeOrders = canUseNativeNow;
    if (bracket.usingNativeOrders)
    {
        if (!submitManagedBracketNativeOrders(bracket))
        {
            WARNING << "Managed bracket native submission unavailable for" << symbol << "- falling back to virtual";
            bracket.usingNativeOrders = false;
        }
    }

    m_managedBrackets.insert(key, bracket);
    emitManagedBracketOverlay(bracket, false);
}

void MainAlgo::processCancelManagedBracket(const QString& p_sourceID,
                                           const QString& p_accountID,
                                           const QString& p_symbol,
                                           const QString& p_reason)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    Q_UNUSED(p_sourceID);

    const QString accountID = p_accountID.trimmed().toUpper();
    const QString symbol = p_symbol.trimmed().toUpper();
    const QString key = managedBracketKey(accountID, symbol);
    auto it = m_managedBrackets.find(key);
    if (it == m_managedBrackets.end())
    {
        return;
    }

    if (!p_reason.isEmpty())
    {
        DEBUG << "Cancelling managed bracket for" << p_symbol << "reason:" << p_reason;
    }
    cancelManagedBracketNativeOrders(*it);
    emitManagedBracketOverlay(*it, true);
    m_managedBrackets.erase(it);
}

void MainAlgo::activateStopLossTightenOnlyLock()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    if (m_stopLossTightenOnlyLockActive)
    {
        return;
    }

    m_stopLossTightenOnlyLockActive = true;
    INFO << "Stop-loss tighten-only lock activated for this app session";
}

void MainAlgo::processAdjustManagedBracketLevels(const QString& p_accountID,
                                                 const QString& p_symbol,
                                                 const std::optional<double>& p_stopPrice,
                                                 const std::optional<double>& p_takePrice,
                                                 const QString& p_reason)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    const QString accountID = p_accountID.trimmed().toUpper();
    const QString symbol = p_symbol.trimmed().toUpper();
    const QString key = managedBracketKey(accountID, symbol);
    auto it = m_managedBrackets.find(key);
    if (it == m_managedBrackets.end())
    {
        return;
    }

    constexpr double kPriceEpsilon = 0.000001;
    ManagedBracket& bracket = *it;
    bool stopAdjustmentBlockedByLock = false;
    if (p_stopPrice.has_value())
    {
        const double requestedStopPrice = std::max(0.01, p_stopPrice.value());
        const bool stopLoosensRisk =
            (bracket.side == StrategyBracketOverlayEntry::Side::Long && requestedStopPrice < bracket.stopPrice) ||
            (bracket.side == StrategyBracketOverlayEntry::Side::Short && requestedStopPrice > bracket.stopPrice);
        if (m_stopLossTightenOnlyLockActive && stopLoosensRisk &&
            std::abs(requestedStopPrice - bracket.stopPrice) > kPriceEpsilon)
        {
            stopAdjustmentBlockedByLock = true;
        }
        else
        {
            bracket.stopPrice = requestedStopPrice;
        }
    }
    if (p_takePrice.has_value())
    {
        bracket.takePrice = std::max(0.01, p_takePrice.value());
    }

    if (bracket.side == StrategyBracketOverlayEntry::Side::Long && bracket.stopPrice >= bracket.takePrice)
    {
        bracket.takePrice = bracket.stopPrice + 0.01;
    }
    if (bracket.side == StrategyBracketOverlayEntry::Side::Short && bracket.stopPrice <= bracket.takePrice)
    {
        bracket.takePrice = std::max(0.01, bracket.stopPrice - 0.01);
    }

    if (stopAdjustmentBlockedByLock)
    {
        DEBUG << "Blocked stop adjustment due to tighten-only lock for" << bracket.symbol
              << "requestedStop=" << p_stopPrice.value() << "currentStop=" << bracket.stopPrice;
    }

    if (!p_reason.isEmpty())
    {
        DEBUG << "Adjusted managed bracket levels for" << bracket.symbol << "reason:" << p_reason
              << "stop=" << bracket.stopPrice << "take=" << bracket.takePrice;
    }

    const bool shouldUpdateStopLeg = p_stopPrice.has_value() && !stopAdjustmentBlockedByLock;
    const bool shouldUpdateTakeLeg = p_takePrice.has_value();
    if (bracket.usingNativeOrders)
    {
        if (shouldUpdateStopLeg)
        {
            if (!bracket.nativeStopOrderID.isEmpty())
            {
                (void)TSClient::getInstance()->cancelOrder(bracket.nativeStopOrderID);
                bracket.nativeStopOrderID.clear();
            }
            submitManagedBracketNativeLeg(bracket, false);
        }
        if (shouldUpdateTakeLeg)
        {
            if (!bracket.nativeTakeOrderID.isEmpty())
            {
                (void)TSClient::getInstance()->cancelOrder(bracket.nativeTakeOrderID);
                bracket.nativeTakeOrderID.clear();
            }
            submitManagedBracketNativeLeg(bracket, true);
        }
    }

    emitManagedBracketOverlay(bracket, false);
}

void MainAlgo::handleManagedBracketOrderUpdate(const Order& p_order)
{
    if (m_managedBrackets.isEmpty())
    {
        return;
    }

    const QString orderID = p_order.getOrderID();
    for (auto it = m_managedBrackets.begin(); it != m_managedBrackets.end(); ++it)
    {
        ManagedBracket& bracket = *it;
        const bool isStopLeg = (orderID == bracket.nativeStopOrderID);
        const bool isTakeLeg = (orderID == bracket.nativeTakeOrderID);
        const bool isVirtualExit = (!bracket.virtualExitOrderID.isEmpty() && (orderID == bracket.virtualExitOrderID));
        if (!isStopLeg && !isTakeLeg && !isVirtualExit)
        {
            continue;
        }

        const Order::Status status = p_order.getOrderStatus();
        if (isVirtualExit)
        {
            if (status == Order::Status::FLL)
            {
                emitManagedBracketOverlay(bracket, true);
                m_managedBrackets.erase(it);
                return;
            }

            if (isTerminalRejectLikeStatus(status) || status == Order::Status::UCN || status == Order::Status::DON)
            {
                const bool wasReplacing = bracket.virtualExitReplaceInProgress;
                bracket.virtualExitOrderPending = false;
                bracket.virtualExitOrderID.clear();
                bracket.virtualExitReplaceInProgress = false;
                if (wasReplacing)
                {
                    DEBUG << "Managed bracket virtual exit replacement unlocked for" << bracket.symbol
                          << "after terminal status" << static_cast<int>(status);
                }
                return;
            }

            // For non-terminal statuses, keep pending=true to avoid duplicate exit submissions.
            return;
        }

        if (isFillLikeStatus(status))
        {
            const QString siblingOrderID = isStopLeg ? bracket.nativeTakeOrderID : bracket.nativeStopOrderID;
            if (!siblingOrderID.isEmpty())
            {
                (void)TSClient::getInstance()->cancelOrder(siblingOrderID);
            }

            emitManagedBracketOverlay(bracket, true);
            m_managedBrackets.erase(it);
            return;
        }

        if (status == Order::Status::CAN)
        {
            if (isStopLeg)
            {
                bracket.nativeStopOrderID.clear();
            }
            if (isTakeLeg)
            {
                bracket.nativeTakeOrderID.clear();
            }
            return;
        }

        if (status == Order::Status::REJ)
        {
            const QString reason = p_order.getRejectReason().has_value() ? p_order.getRejectReason()->trimmed()
                                                                         : p_order.getStatusDescription().trimmed();
            const QString key = it.key();
            handleManagedBracketNativeFailure(key,
                                              bracket,
                                              QStringLiteral("native leg rejected: %1")
                                                  .arg(reason.isEmpty() ? QStringLiteral("order rejected") : reason));
            return;
        }
    }
}

void MainAlgo::monitorManagedBrackets()
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &thread);
    if (m_managedBrackets.isEmpty())
    {
        return;
    }

    QVector<QString> keysToClear;
    const TradingSession session = MainApp::getCurrentSession();
    const bool sessionIsRegular = session == TradingSession::Regular;
    const bool sessionIsExtended = isExtendedHoursSession(session);
    const QDateTime now = MainApp::getCurrentAppTime();
    const bool replayModeActive = MainApp::isInReplayMode();
    const QString replaySimAccountID = OrderEmulator::getSimulatedAccountID();

    for (auto it = m_managedBrackets.begin(); it != m_managedBrackets.end(); ++it)
    {
        ManagedBracket& bracket = *it;
        if (!replayModeActive && bracket.accountID == replaySimAccountID)
        {
            WARNING << "Clearing replay-account managed bracket after replay mode exit:" << bracket.symbol
                    << "account=" << bracket.accountID << "source=" << bracket.sourceID;
            keysToClear.append(it.key());
            continue;
        }

        updateManagedBracketQuantity(bracket);
        if (bracket.protectedQuantity <= 0)
        {
            keysToClear.append(it.key());
            continue;
        }

        if (bracket.nativeFailureDropLatched)
        {
            continue;
        }

        const bool shouldUseNativeNow =
            sessionIsRegular && (bracket.requestedExecutionPolicy == StrategyBracketExecutionPolicy::Auto ||
                                 bracket.requestedExecutionPolicy == StrategyBracketExecutionPolicy::NativeOnly);

        if (shouldUseNativeNow && !bracket.usingNativeOrders)
        {
            bracket.usingNativeOrders = submitManagedBracketNativeOrders(bracket);
            if (!bracket.usingNativeOrders)
            {
                WARNING << "Unable to activate native managed bracket for" << bracket.symbol
                        << "- continuing with virtual monitoring";
            }
            emitManagedBracketOverlay(bracket, false);
        }
        else if (!shouldUseNativeNow && bracket.usingNativeOrders)
        {
            cancelManagedBracketNativeOrders(bracket);
            bracket.usingNativeOrders = false;
            emitManagedBracketOverlay(bracket, false);
        }

        if (bracket.usingNativeOrders)
        {
            continue;
        }

        const auto triggerPrice = managedBracketTriggerPrice(bracket);
        if (!triggerPrice.has_value())
        {
            continue;
        }

        if (!bracket.triggered)
        {
            if (bracket.side == StrategyBracketOverlayEntry::Side::Long)
            {
                if (triggerPrice.value() <= bracket.stopPrice)
                {
                    bracket.triggered = true;
                    bracket.triggerReason = "stop-loss";
                }
                else if (triggerPrice.value() >= bracket.takePrice)
                {
                    bracket.triggered = true;
                    bracket.triggerReason = "take-profit";
                }
            }
            else
            {
                if (triggerPrice.value() >= bracket.stopPrice)
                {
                    bracket.triggered = true;
                    bracket.triggerReason = "stop-loss";
                }
                else if (triggerPrice.value() <= bracket.takePrice)
                {
                    bracket.triggered = true;
                    bracket.triggerReason = "take-profit";
                }
            }

            if (bracket.triggered)
            {
                INFO << "Managed bracket triggered for" << bracket.symbol << "reason=" << bracket.triggerReason
                     << "triggerPrice=" << triggerPrice.value() << "stop=" << bracket.stopPrice
                     << "take=" << bracket.takePrice << "session=" << tradingSessionToString(session);
                emitManagedBracketOverlay(bracket, false);
            }
        }

        const bool stopExitRepriceEnabled =
            bracket.triggered && bracket.triggerReason == QStringLiteral("stop-loss") && sessionIsExtended;
        if (stopExitRepriceEnabled && bracket.virtualExitReplaceInProgress && now.isValid() &&
            bracket.virtualExitLastCancelRequestTime.isValid())
        {
            const qint64 cancelWaitMs = std::max<qint64>(0, bracket.virtualExitLastCancelRequestTime.msecsTo(now));
            if (cancelWaitMs >= kManagedBracketVirtualExitCancelTimeoutMs)
            {
                WARNING << "Managed bracket virtual stop exit cancel timed out, retrying for" << bracket.symbol
                        << "orderID=" << bracket.virtualExitOrderID << "waitMs=" << cancelWaitMs;
                bracket.virtualExitReplaceInProgress = false;
            }
        }

        if (stopExitRepriceEnabled && bracket.virtualExitOrderPending && !bracket.virtualExitOrderID.isEmpty() &&
            !bracket.virtualExitReplaceInProgress)
        {
            const auto trackedOrderIt = m_latestOrdersById.constFind(bracket.virtualExitOrderID);
            if (trackedOrderIt != m_latestOrdersById.constEnd())
            {
                const Order& trackedOrder = trackedOrderIt.value();
                const Order::Status trackedStatus = trackedOrder.getOrderStatus();
                const bool trackedOrderTerminal =
                    trackedStatus == Order::Status::FLL || isTerminalRejectLikeStatus(trackedStatus) ||
                    trackedStatus == Order::Status::UCN || trackedStatus == Order::Status::DON;
                if (!trackedOrderTerminal)
                {
                    const auto orderLimitPrice = trackedOrder.getLimitPrice();
                    QString pricingSource;
                    const auto latestLevel2 = resolveManagedBracketPricingLevel2(bracket, &pricingSource);
                    if (orderLimitPrice.has_value() && latestLevel2.has_value())
                    {
                        const TradeAction closeAction = bracket.side == StrategyBracketOverlayEntry::Side::Long
                                                            ? TradeAction::Sell
                                                            : TradeAction::BuyToCover;
                        const bool marketableNow =
                            isLimitPriceMarketable(*latestLevel2, closeAction, orderLimitPrice.value());
                        qint64 ageMs = 0;
                        if (trackedOrder.getOpenedDateTime().isValid() && now.isValid())
                        {
                            ageMs = std::max<qint64>(0, trackedOrder.getOpenedDateTime().msecsTo(now));
                        }
                        else if (bracket.virtualExitLastSubmitTime.isValid() && now.isValid())
                        {
                            ageMs = std::max<qint64>(0, bracket.virtualExitLastSubmitTime.msecsTo(now));
                        }

                        const bool staleOrder = ageMs >= kManagedBracketVirtualExitStaleMs;
                        const bool shouldCancelReplace = !marketableNow || staleOrder;
                        if (shouldCancelReplace)
                        {
                            qint64 sinceLastCancelMs = std::numeric_limits<qint64>::max();
                            if (bracket.virtualExitLastCancelRequestTime.isValid() && now.isValid())
                            {
                                sinceLastCancelMs =
                                    std::max<qint64>(0, bracket.virtualExitLastCancelRequestTime.msecsTo(now));
                            }

                            if (sinceLastCancelMs >= kManagedBracketVirtualExitCancelRetryMs)
                            {
                                const QString key = it.key();
                                const QString orderID = bracket.virtualExitOrderID;
                                bracket.virtualExitReplaceInProgress = true;
                                bracket.virtualExitLastCancelRequestTime = now;

                                INFO << "Managed bracket virtual stop exit cancel/replace request for" << bracket.symbol
                                     << "orderID=" << orderID << "limit=" << orderLimitPrice.value()
                                     << "marketable=" << marketableNow << "ageMs=" << ageMs
                                     << "source=" << pricingSource;

                                TSClient::getInstance()->cancelOrder(orderID).then(
                                    this,
                                    [this, key, orderID](std::expected<CancelOrderResult, TSClient::Error> p_result)
                                    {
                                        auto bracketIt = m_managedBrackets.find(key);
                                        if (bracketIt == m_managedBrackets.end())
                                        {
                                            return;
                                        }

                                        if (!p_result.has_value())
                                        {
                                            bracketIt->virtualExitReplaceInProgress = false;
                                            WARNING << "Managed bracket virtual stop exit cancel failed for order"
                                                    << orderID << "symbol=" << bracketIt->symbol
                                                    << "error=" << tsClientErrorToString(p_result.error());
                                            return;
                                        }

                                        DEBUG << "Managed bracket virtual stop exit cancel accepted for order"
                                              << orderID << "symbol=" << bracketIt->symbol;
                                    });
                            }
                        }
                    }
                }
            }
        }

        if (bracket.triggered && !bracket.virtualExitOrderPending)
        {
            submitManagedBracketVirtualExitOrder(bracket);
        }
    }

    for (const QString& key: keysToClear)
    {
        auto it = m_managedBrackets.find(key);
        if (it == m_managedBrackets.end())
        {
            continue;
        }
        cancelManagedBracketNativeOrders(*it);
        emitManagedBracketOverlay(*it, true);
        m_managedBrackets.erase(it);
    }
}
