#include "StrategyManager.h"
#include "ExternalStrategyDescription.h"
#include "ProcessStrategyRuntimeBackend.h"
#include "StrategySDK.h"
#include "../Algo/MainAlgo.h"
#include "../Core/MainApp.h"
#include "Assume.h"
#include "OrdersDatabase.h"
#include "Settings.h"
#include "CONSTANTS.h"
#include <QUuid>
#include <QDebug>
#include <QPromise>
#include <QJsonDocument>
#include <QtGlobal>
#include <algorithm>
#include <utility>

#define LOGGING_CATEGORY StrategyManagerLog

#include "Logging.h"

Q_LOGGING_CATEGORY(StrategyManagerLog, "StrategyManager")

namespace
{
    template<typename T> [[nodiscard]] auto makeReadyValueFutureCompat(T&& p_value)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        return QtFuture::makeReadyValueFuture(std::forward<T>(p_value));
#else
        return QtFuture::makeReadyFuture(std::forward<T>(p_value));
#endif
    }

    void assumeMainAlgoThread([[maybe_unused]] const MainAlgo* p_mainAlgo)
    {
        ASSUME_DIFF(p_mainAlgo, nullptr);
        ASSUME_EQUAL(QThread::currentThread(), p_mainAlgo->QObject::thread());
    }

    [[nodiscard]] QStringList normalizeRequestedSymbols(const QStringList& p_symbols)
    {
        QStringList normalized;
        for (const QString& symbol: p_symbols)
        {
            const QString trimmed = symbol.trimmed().toUpper();
            if (!trimmed.isEmpty() && !normalized.contains(trimmed))
            {
                normalized.append(trimmed);
            }
        }
        return normalized;
    }

    [[nodiscard]] std::expected<StrategyConfig, QString> hydrateExternalProcessConfig(const StrategyConfig& p_config)
    {
        if (!p_config.usesExternalProcess())
        {
            return std::unexpected(QStringLiteral(
                "Legacy in-process strategy plugins are no longer supported. Rebuild this strategy as an external process."));
        }

        if (p_config.fieldValues.contains("_legacySymbols"))
        {
            return std::unexpected(QStringLiteral(
                "This saved strategy configuration used multiple legacy symbols and must be reconfigured with the new executable-defined fields."));
        }

        auto descriptionResult = ExternalStrategyDescription::describeExecutable(p_config.executablePath);
        if (!descriptionResult.has_value())
        {
            return std::unexpected(descriptionResult.error());
        }

        const QStringList validationErrors =
            ExternalStrategyDescription::validateFieldValues(descriptionResult.value(), p_config.fieldValues);
        if (!validationErrors.isEmpty())
        {
            return std::unexpected(QStringLiteral("Strategy configuration requires review before loading:\n- %1")
                                       .arg(validationErrors.join(QStringLiteral("\n- "))));
        }

        StrategyConfig hydrated = p_config;
        hydrated.name = descriptionResult->name;
        hydrated.version = descriptionResult->version;
        hydrated.executablePath = descriptionResult->executablePath;
        return hydrated;
    }

    [[nodiscard]] bool isFillLikeStatus(const Order::Status p_status)
    {
        return p_status == Order::Status::FLL || p_status == Order::Status::FLP || p_status == Order::Status::FPR;
    }

    [[nodiscard]] double positionMarkPrice(const Position& p_position)
    {
        const double markPrice = p_position.getMarkToMarketPrice().toDouble();
        if (markPrice > 0.0)
        {
            return markPrice;
        }

        const double lastPrice = p_position.getLast().toDouble();
        if (lastPrice > 0.0)
        {
            return lastPrice;
        }

        return p_position.getAveragePrice().toDouble();
    }
} // namespace

// StrategySDK implementation
StrategySDK::StrategySDK(MainAlgo* p_mainAlgo,
                         const QString& p_strategyID,
                         const StrategyConfig& p_config,
                         StrategyLogger* p_logger)
    : QObject(nullptr), m_mainAlgo(p_mainAlgo), m_strategyID(p_strategyID), m_config(p_config), m_logger(p_logger)
{
}

QFuture<std::expected<PlaceOrderResult, TSClient::Error>> StrategySDK::placeOrder(const PlaceOrderRequest& p_order)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    // Enforce: strategy may only trade symbols it has claimed exclusive authority over
    if (!m_claimedSymbols.contains(p_order.getSymbol()))
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected order for unclaimed symbol:" << p_order.getSymbol()
                                     << "- call claimSymbols() first";
        return makeReadyValueFutureCompat(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
    }

    // Validate order before sending to MainAlgo
    if (!StrategyOrderValidator::validateOrder(m_strategyID, p_order))
    {
        return makeReadyValueFutureCompat(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
    }

    uint64_t requestId = m_mainAlgo->getNextRequestId();

    // Create a promise that will be resolved when order ACK is received
    auto promise = std::make_shared<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>>();
    promise->start();
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> future = promise->future();

    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, requestId, p_order, promise]()
        { m_mainAlgo->processPlaceOrder(requestId, m_strategyID, p_order, promise); },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<PlaceOrderResult, TSClient::Error>>
StrategySDK::placeOrderWithUserConfirmation(const QString& p_strategyRequestID,
                                            const PlaceOrderRequest& p_order,
                                            const QString& p_promptText,
                                            const StrategyManualOrderExecutionMode p_executionMode)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    const QString strategyRequestID = p_strategyRequestID.trimmed();
    auto rejectAndNotify =
        [this, &strategyRequestID, &p_order](const QString& p_reason, const StrategyManualOrderDecision p_decision)
    {
        if (!strategyRequestID.isEmpty() && m_mainAlgo->getStrategyManager() != nullptr)
        {
            m_mainAlgo->getStrategyManager()->publishManualOrderDecision(m_strategyID,
                                                                         strategyRequestID,
                                                                         p_order.getSymbol(),
                                                                         p_decision,
                                                                         p_reason);
        }
        return makeReadyValueFutureCompat(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
    };

    if (strategyRequestID.isEmpty())
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected placeOrderWithUserConfirmation with an empty strategyRequestID";
        return rejectAndNotify(QStringLiteral("Manual confirmation request_id must not be empty"),
                               StrategyManualOrderDecision::Cancelled);
    }

    // Enforce: strategy may only trade symbols it has claimed exclusive authority over
    if (!m_claimedSymbols.contains(p_order.getSymbol()))
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected manual-confirmed order for unclaimed symbol:" << p_order.getSymbol()
                                     << "- call claimSymbols() first";
        return rejectAndNotify(QStringLiteral("Strategy must claim symbol before confirmed order placement"),
                               StrategyManualOrderDecision::Rejected);
    }

    // Validate order before sending to MainAlgo
    if (!StrategyOrderValidator::validateOrder(m_strategyID, p_order))
    {
        return rejectAndNotify(QStringLiteral("Strategy order validation failed"),
                               StrategyManualOrderDecision::Rejected);
    }

    const uint64_t requestId = m_mainAlgo->getNextRequestId();

    auto promise = std::make_shared<QPromise<std::expected<PlaceOrderResult, TSClient::Error>>>();
    promise->start();
    QFuture<std::expected<PlaceOrderResult, TSClient::Error>> future = promise->future();

    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, requestId, strategyRequestID, p_order, p_promptText, p_executionMode, promise]()
        {
            m_mainAlgo->processPlaceOrderWithUserConfirmation(requestId,
                                                              m_strategyID,
                                                              strategyRequestID,
                                                              p_order,
                                                              p_promptText,
                                                              p_executionMode,
                                                              promise);
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<CancelOrderResult, TSClient::Error>> StrategySDK::cancelOrder(const QString& p_orderID)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    auto promise = std::make_shared<QPromise<std::expected<CancelOrderResult, TSClient::Error>>>();
    promise->start();
    QFuture<std::expected<CancelOrderResult, TSClient::Error>> future = promise->future();

    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, p_orderID, promise]() { m_mainAlgo->processCancelOrder(p_orderID, promise); },
        Qt::QueuedConnection);

    return future;
}

QFuture<std::expected<ClosePositionsResult, QString>> StrategySDK::closePositions(const QString& p_accountID,
                                                                                  const QStringList& p_symbols)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    const QString accountID = p_accountID.trimmed();
    if (accountID.isEmpty())
    {
        return makeReadyValueFutureCompat(std::expected<ClosePositionsResult, QString>(
            std::unexpected(QStringLiteral("Strategy closePositions requires accountId"))));
    }

    const QStringList claimedSymbols = normalizeRequestedSymbols(m_claimedSymbols);
    if (claimedSymbols.isEmpty())
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "requested closePositions without any claimed symbols";
        return makeReadyValueFutureCompat(std::expected<ClosePositionsResult, QString>(
            std::unexpected(QStringLiteral("Strategy must claim symbols before calling closePositions"))));
    }

    const QStringList requestedSymbols = normalizeRequestedSymbols(p_symbols);
    QStringList effectiveSymbols = requestedSymbols.isEmpty() ? claimedSymbols : requestedSymbols;
    QStringList unclaimedSymbols;
    for (const QString& symbol: effectiveSymbols)
    {
        if (!claimedSymbols.contains(symbol))
        {
            unclaimedSymbols.append(symbol);
        }
    }

    if (!unclaimedSymbols.isEmpty())
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected closePositions for unclaimed symbols:" << unclaimedSymbols;
        return makeReadyValueFutureCompat(std::expected<ClosePositionsResult, QString>(
            std::unexpected(QString("Strategy cannot close unclaimed symbols: %1").arg(unclaimedSymbols.join(", ")))));
    }

    ClosePositionsRequest request;
    request.accountId = accountID;
    request.symbols = effectiveSymbols;
    request.aggressivityOffsetCents =
        appStateSettings == nullptr ? ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS
                                    : appStateSettings
                                          ->value(ClosePositionsConstants::SETTINGS_KEY_AGGRESSIVE_LIMIT_OFFSET_CENTS,
                                                  ClosePositionsConstants::DEFAULT_AGGRESSIVE_LIMIT_OFFSET_CENTS)
                                          .toDouble();
    request.executionMode = ClosePositionsExecutionMode::AggressiveMarketable;
    return m_mainAlgo->closePositions(request, m_strategyID);
}

QFuture<bool> StrategySDK::subscribeToSymbol(const QString& p_symbol)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    auto promise = std::make_shared<QPromise<bool>>();
    promise->start();
    QFuture<bool> future = promise->future();

    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, p_symbol, promise]() { m_mainAlgo->processSubscribeToSymbol(m_strategyID, p_symbol, promise); },
        Qt::QueuedConnection);

    return future;
}

QFuture<QStringList> StrategySDK::claimSymbols(const QStringList& p_symbols)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    auto promise = std::make_shared<QPromise<QStringList>>();
    promise->start();
    QFuture<QStringList> future = promise->future();

    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, p_symbols, promise]() { m_mainAlgo->processClaimSymbols(m_strategyID, p_symbols, promise); },
        Qt::QueuedConnection);

    return future;
}

QVector<Position> StrategySDK::getPositions() const
{
    return m_positions;
}

QVector<Order> StrategySDK::getOrders() const
{
    return m_orders;
}

double StrategySDK::getAccountBalance() const
{
    return m_balance;
}

void StrategySDK::updateOrder(const Order& p_order)
{
    // Replace existing order with same ID, or append
    for (auto& existing: m_orders)
    {
        if (existing.getOrderID() == p_order.getOrderID())
        {
            existing = p_order;
            return;
        }
    }
    m_orders.append(p_order);
}

void StrategySDK::updatePosition(const Position& p_position)
{
    // Replace existing position with same ID, or append
    for (auto& existing: m_positions)
    {
        if (existing.getPositionID() == p_position.getPositionID())
        {
            existing = p_position;
            return;
        }
    }
    m_positions.append(p_position);
}

void StrategySDK::updateBalance(double p_balance)
{
    m_balance = p_balance;
}

void StrategySDK::setClaimedSymbols(const QStringList& symbols)
{
    m_claimedSymbols = symbols;
}

void StrategySDK::clearClaimedSymbols()
{
    m_claimedSymbols.clear();
}

void StrategySDK::log(const QString& p_message, LogLevel p_level)
{
    // Convert LogLevel to QtMsgType
    QtMsgType qtLevel = QtDebugMsg;
    switch (p_level)
    {
    case LogLevel::Debug:
        qtLevel = QtDebugMsg;
        break;
    case LogLevel::Info:
        qtLevel = QtInfoMsg;
        break;
    case LogLevel::Warning:
        qtLevel = QtWarningMsg;
        break;
    case LogLevel::Error:
        qtLevel = QtCriticalMsg;
        break;
    }

    // Log to strategy logger if available
    if (m_logger)
    {
        m_logger->log(qtLevel, p_message);
    }

    // Also log to global category for debugging
    qInfo(StrategyManagerLog) << "Strategy [" << m_strategyID << "]:" << p_message;
}

const StrategyConfig& StrategySDK::getConfig() const
{
    return m_config;
}

void StrategySDK::setConfig(const StrategyConfig& p_config)
{
    m_config = p_config;
}

void StrategySDK::logToChart(const QString& p_symbol, const QString& p_message)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);
    ASSUME_FALSE(p_symbol.isEmpty());
    ASSUME_FALSE(p_message.isEmpty());
    ASSUME_TRUE(m_claimedSymbols.contains(p_symbol));

    StrategyLogEntry entry;
    entry.strategyID = m_strategyID;
    entry.symbol = p_symbol;
    entry.timestamp = getCurrentTime();
    entry.message = p_message;

    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, entry]() { m_mainAlgo->processStrategyLog(entry); },
        Qt::QueuedConnection);
}

bool StrategySDK::requestChartDisplaySwitch(const QString& p_symbol, const QString& p_reason)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    const QString symbol = p_symbol.trimmed().toUpper();
    if (symbol.isEmpty())
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected chart-display switch request with empty symbol";
        return false;
    }
    if (!m_claimedSymbols.contains(symbol))
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected chart-display switch request for unclaimed symbol:" << symbol
                                     << "- call claimSymbols() first";
        return false;
    }

    const QString reason = p_reason.trimmed();
    qInfo(StrategyManagerLog) << "Strategy" << m_strategyID << "requesting chart-display switch to" << symbol
                              << "reason=" << (reason.isEmpty() ? QStringLiteral("<none>") : reason);
    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, symbol, reason]()
        { m_mainAlgo->processStrategyChartDisplaySwitchRequest(m_strategyID, symbol, reason); },
        Qt::QueuedConnection);

    return true;
}

bool StrategySDK::upsertManagedBracket(const QString& p_symbol,
                                       const QString& p_accountID,
                                       const StrategyBracketSide p_side,
                                       const double p_stopPrice,
                                       const double p_takePrice,
                                       const StrategyBracketExecutionPolicy p_executionPolicy,
                                       const std::optional<double>& p_referenceEntryPrice)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);
    if (p_symbol.isEmpty() || p_accountID.isEmpty() || p_stopPrice <= 0.0 || p_takePrice <= 0.0)
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected upsertManagedBracket due to invalid inputs";
        return false;
    }
    if (!m_claimedSymbols.contains(p_symbol))
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected managed bracket for unclaimed symbol:" << p_symbol
                                     << "- call claimSymbols() first";
        return false;
    }

    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, p_symbol, p_accountID, p_side, p_stopPrice, p_takePrice, p_executionPolicy, p_referenceEntryPrice]()
        {
            m_mainAlgo->processUpsertManagedBracket(m_strategyID,
                                                    p_accountID,
                                                    p_symbol,
                                                    p_side,
                                                    p_stopPrice,
                                                    p_takePrice,
                                                    p_executionPolicy,
                                                    p_referenceEntryPrice);
        },
        Qt::QueuedConnection);

    return true;
}

bool StrategySDK::cancelManagedBracket(const QString& p_symbol, const QString& p_accountID)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);
    if (p_symbol.isEmpty() || p_accountID.isEmpty())
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected cancelManagedBracket due to invalid inputs";
        return false;
    }
    if (!m_claimedSymbols.contains(p_symbol))
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "rejected cancelManagedBracket for unclaimed symbol:" << p_symbol
                                     << "- call claimSymbols() first";
        return false;
    }

    QMetaObject::invokeMethod(
        m_mainAlgo,
        [this, p_symbol, p_accountID]()
        { m_mainAlgo->processCancelManagedBracket(m_strategyID, p_accountID, p_symbol); },
        Qt::QueuedConnection);

    return true;
}

const QString& StrategySDK::getStrategyName() const
{
    return m_config.name;
}

std::shared_ptr<QVector<Bar>> StrategySDK::getHistoricalBars(const QString& p_symbol,
                                                             const QDate& day,
                                                             const QTime& first,
                                                             const QTime& last,
                                                             TimeFrame tf)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    QString symbol = p_symbol;
    if (symbol.isEmpty() && !m_claimedSymbols.isEmpty())
    {
        symbol = m_claimedSymbols[0];
    }
    if (symbol.isEmpty())
    {
        qWarning(StrategyManagerLog) << "Strategy" << m_strategyID
                                     << "requested historical bars without a symbol or claimed-symbol fallback";
        return std::make_shared<QVector<Bar>>();
    }

    // Request bars from MainAlgo (which has access to all SymbolContext and their BarCaches)
    auto result = m_mainAlgo->requestHistoricalBarsForSymbol(symbol, day, first, last, tf);

    // Result is a variant of either std::shared_ptr<QVector<Bar>> or QFuture
    if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(result))
    {
        // Already cached, return immediately
        return std::get<std::shared_ptr<QVector<Bar>>>(result);
    }
    else
    {
        // Need to wait for QFuture to resolve
        auto future = std::get<QFuture<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(result);
        future.waitForFinished(); // Block and wait for result

        if (future.isValid() && future.resultCount() > 0)
        {
            auto expected = future.result();
            if (expected.has_value())
            {
                return expected.value();
            }
        }
    }

    // Return empty vector on error
    return std::make_shared<QVector<Bar>>();
}

QDateTime StrategySDK::getCurrentTime() const
{
    return MainApp::getCurrentAppTime();
}

double StrategySDK::getTradeRate(const QString& p_symbol) const
{
    return m_mainAlgo->getActivityMetrics(p_symbol).tradeRateHz;
}

double StrategySDK::getL2UpdateRate(const QString& p_symbol) const
{
    return m_mainAlgo->getActivityMetrics(p_symbol).l2RateHz;
}

bool StrategySDK::isSymbolActive(const QString& p_symbol) const
{
    return m_mainAlgo->getActivityMetrics(p_symbol).isActive;
}

// StrategyManager implementation

StrategyManager::StrategyManager(MainAlgo* p_mainAlgo) : QObject(nullptr), m_mainAlgo(p_mainAlgo)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);
}

StrategyManager::~StrategyManager()
{
    assumeMainAlgoThread(m_mainAlgo);
    qInfo(StrategyManagerLog) << "StrategyManager shutdown: unloading" << m_strategies.size() << "active strategies";

    m_persistEnabled = false; // Don't overwrite persisted state during shutdown teardown

    QVector<QString> strategyIDs = getActiveStrategies();
    for (const auto& strategyID: strategyIDs)
    {
        auto error = unloadStrategy(strategyID);
        if (!error.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Error unloading strategy:" << strategyID << "-" << error;
        }
    }

    qInfo(StrategyManagerLog) << "StrategyManager shutdown complete";
}

StrategyManager::StrategyExecutionState StrategyManager::toExecutionState(const StrategyState p_state)
{
    switch (p_state)
    {
    case StrategyState::LOADED:
        return StrategyExecutionState::Loaded;
    case StrategyState::QUEUED:
        return StrategyExecutionState::Primed;
    case StrategyState::RUNNING:
        return StrategyExecutionState::Running;
    case StrategyState::PAUSED:
        return StrategyExecutionState::Paused;
    case StrategyState::STOPPED:
        return StrategyExecutionState::Stopped;
    }

    Q_UNREACHABLE();
    return StrategyExecutionState::Stopped;
}

void StrategyManager::setStrategyState(StrategyInstance* p_instance,
                                       const StrategyState p_state,
                                       const QString& p_errorMessage)
{
    ASSUME_DIFF(p_instance, nullptr);
    p_instance->state = p_state;
    emit strategyStatusChanged(p_instance->strategyID, toExecutionState(p_state), p_errorMessage);
    persistStrategiesState();
}

QString StrategyManager::queueStrategyStart(StrategyInstance* p_instance)
{
    ASSUME_DIFF(p_instance, nullptr);

    if (p_instance->state == StrategyState::QUEUED)
    {
        return "";
    }

    if (p_instance->state != StrategyState::LOADED)
    {
        return QStringLiteral("Strategy is not in a loadable state for replay priming");
    }

    setStrategyState(p_instance, StrategyState::QUEUED);
    qInfo(StrategyManagerLog) << "Primed strategy for first replay play:" << p_instance->config.name
                              << "ID:" << p_instance->strategyID;
    return "";
}

QString StrategyManager::startStrategyInternal(StrategyInstance* p_instance, const bool p_startPaused)
{
    ASSUME_DIFF(p_instance, nullptr);
    ASSUME_DIFF(p_instance->p_backend.get(), nullptr);

    const QString error = p_instance->p_backend->start(
        [this, p_instance, p_startPaused]()
        { setStrategyState(p_instance, p_startPaused ? StrategyState::PAUSED : StrategyState::RUNNING); });
    if (!error.isEmpty())
    {
        return error;
    }

    if (p_startPaused)
    {
        const QString pauseError = p_instance->p_backend->pause(QStringLiteral("Replay is paused"));
        if (!pauseError.isEmpty())
        {
            return pauseError;
        }
    }

    qInfo(StrategyManagerLog) << "Started strategy:" << p_instance->config.name << "ID:" << p_instance->strategyID
                              << "paused:" << p_startPaused;
    return "";
}

std::expected<QString, QString> StrategyManager::loadStrategy(const StrategyConfig& p_config)
{
    assumeMainAlgoThread(m_mainAlgo);

    if (p_config.runtimePath().isEmpty())
    {
        return std::unexpected("Strategy runtime path is empty");
    }

    const auto hydratedConfigResult = hydrateExternalProcessConfig(p_config);
    if (!hydratedConfigResult.has_value())
    {
        return std::unexpected(hydratedConfigResult.error());
    }

    const StrategyConfig hydratedConfig = hydratedConfigResult.value();
    QString strategyID = generateStrategyID();

    std::expected<std::unique_ptr<IStrategyRuntimeBackend>, QString> runtimeResult = std::unexpected(QString{});
    auto processRuntimeResult = ProcessStrategyRuntimeBackend::create(m_mainAlgo, strategyID, hydratedConfig);
    if (processRuntimeResult)
    {
        runtimeResult = std::unique_ptr<IStrategyRuntimeBackend>(std::move(processRuntimeResult.value()));
    }
    else
    {
        runtimeResult = std::unexpected(processRuntimeResult.error());
    }
    if (!runtimeResult)
    {
        return std::unexpected(runtimeResult.error());
    }

    auto instance = new StrategyInstance();
    instance->strategyID = strategyID;
    instance->config = hydratedConfig;
    instance->p_backend = std::move(runtimeResult.value());
    instance->monitoredSymbols.clear();
    instance->blockedSymbols.clear();

    connectStrategyToDataSources(instance);

    // Note: Process is NOT started here - wait for startStrategy() to be called
    // This allows the UI to show the strategy in LOADED state and wait for user to click Start

    qInfo(StrategyManagerLog) << "Loaded strategy:" << hydratedConfig.name << "ID:" << strategyID;

    m_strategies[strategyID] = instance;

    emit strategyLoaded(strategyID, hydratedConfig.name);

    persistStrategiesState();

    return strategyID;
}

QString StrategyManager::unloadStrategy(const QString& p_strategyID)
{
    assumeMainAlgoThread(m_mainAlgo);
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        return "Strategy not found: " + p_strategyID;
    }

    qInfo(StrategyManagerLog) << "Unloading strategy:" << instance->config.name << "ID:" << p_strategyID;

    m_mainAlgo->cancelManualOrderConfirmationsForStrategy(p_strategyID, QStringLiteral("Strategy was unloaded"), true);

    // Release all symbol claims before disconnecting
    releaseSymbols(p_strategyID);
    instance->blockedSymbols.clear();

    disconnectStrategyFromDataSources(instance);

    // Call onStop on the strategy thread before quitting
    // Only if thread is still running (not crashed)
    if (instance->p_backend)
    {
        instance->p_backend->invokeOnStop();
        qInfo(StrategyManagerLog) << "Called onStop for strategy:" << instance->config.name;
    }

    // Log the file path (logs are written incrementally, nothing to flush)
    if (instance->p_backend && instance->p_backend->logger())
    {
        qInfo(StrategyManagerLog) << "Strategy log file:" << instance->p_backend->logger()->getLogFilePath();
    }

    if (instance->p_backend)
    {
        instance->p_backend->destroyRuntime();
        qInfo(StrategyManagerLog) << "Destroyed strategy runtime backend:" << instance->config.name;
    }

    // Remove from registry and delete instance
    delete m_strategies.take(p_strategyID);

    qInfo(StrategyManagerLog) << "Successfully unloaded strategy:" << p_strategyID;

    emit strategyUnloaded(p_strategyID);

    persistStrategiesState();

    return "";
}

QString StrategyManager::startStrategy(const QString& p_strategyID)
{
    assumeMainAlgoThread(m_mainAlgo);
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        return "Strategy not found: " + p_strategyID;
    }

    if (instance->state == StrategyState::QUEUED || instance->state == StrategyState::RUNNING ||
        instance->state == StrategyState::PAUSED)
    {
        return "Strategy is not in LOADED state (current state: " + QString::number(static_cast<int>(instance->state)) +
               ")";
    }

    if (MainApp::isInReplayMode())
    {
        const Playback::State replayState = m_mainAlgo->getReplayState();
        const bool hasStartedPlayback = MainApp::getInstance()->hasReplayPlaybackStarted();

        if (!hasStartedPlayback)
        {
            return queueStrategyStart(instance);
        }

        if (replayState != Playback::State::Playing)
        {
            return startStrategyInternal(instance, true);
        }
    }

    return startStrategyInternal(instance, false);
}

QString StrategyManager::stopStrategy(const QString& p_strategyID)
{
    assumeMainAlgoThread(m_mainAlgo);
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        return "Strategy not found: " + p_strategyID;
    }

    const StrategyState state = instance->state;
    if (state == StrategyState::LOADED || state == StrategyState::STOPPED)
    {
        return QString();
    }

    if (state != StrategyState::QUEUED && state != StrategyState::RUNNING && state != StrategyState::PAUSED)
    {
        return QStringLiteral("Strategy is not in a stoppable state");
    }

    qInfo(StrategyManagerLog) << "Stopping strategy (keeping loaded instance):" << instance->config.name
                              << "ID:" << p_strategyID;

    m_mainAlgo->cancelManualOrderConfirmationsForStrategy(p_strategyID, QStringLiteral("Strategy was stopped"), true);

    if (instance->p_backend && instance->p_backend->isThreadRunning())
    {
        instance->p_backend->invokeOnStop();
        instance->p_backend->shutdownExecutionThread();
    }

    releaseSymbols(p_strategyID);
    instance->blockedSymbols.clear();
    setStrategyState(instance, StrategyState::STOPPED);

    return QString();
}

QString StrategyManager::updateStrategyConfig(const QString& p_strategyID, const StrategyConfig& p_config)
{
    assumeMainAlgoThread(m_mainAlgo);
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        return "Strategy not found: " + p_strategyID;
    }

    if (instance->state != StrategyState::LOADED && instance->state != StrategyState::STOPPED)
    {
        return QStringLiteral("Strategy parameters can only be edited while strategy is stopped/loaded");
    }

    const auto hydratedConfigResult = hydrateExternalProcessConfig(p_config);
    if (!hydratedConfigResult.has_value())
    {
        return hydratedConfigResult.error();
    }
    const StrategyConfig hydratedConfig = hydratedConfigResult.value();

    if (instance->p_backend)
    {
        const QString runtimeError = instance->p_backend->updateConfig(hydratedConfig);
        if (!runtimeError.isEmpty())
        {
            return runtimeError;
        }
    }

    instance->config = hydratedConfig;
    persistStrategiesState();
    qInfo(StrategyManagerLog) << "Updated strategy config:" << instance->config.name << "ID:" << p_strategyID;
    return QString();
}

void StrategyManager::markStrategyFailed(const QString& p_strategyID, const QString& p_errorMessage)
{
    assumeMainAlgoThread(m_mainAlgo);
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        qWarning(StrategyManagerLog) << "markStrategyFailed: Strategy not found:" << p_strategyID;
        return;
    }

    qCritical(StrategyManagerLog) << "Strategy marked as FAILED:" << instance->config.name
                                  << "Error:" << p_errorMessage;

    m_mainAlgo->cancelManualOrderConfirmationsForStrategy(p_strategyID, QStringLiteral("Strategy failed"), true);

    setStrategyState(instance, StrategyState::STOPPED, p_errorMessage);

    // Stop the process if it's still running (it may have already crashed)
    if (instance->p_backend && instance->p_backend->isThreadRunning())
    {
        instance->p_backend->shutdownExecutionThread();
    }
}

void StrategyManager::stopAllStrategies()
{
    assumeMainAlgoThread(m_mainAlgo);
    QVector<QString> activeStrategies = getActiveStrategies();
    INFO << "Stopping all strategies, count:" << activeStrategies.size();

    m_persistEnabled = false; // Don't persist the emptied state during bulk teardown

    for (const QString& strategyID: activeStrategies)
    {
        QString error = unloadStrategy(strategyID);
        if (!error.isEmpty())
        {
            WARNING << "Failed to unload strategy" << strategyID << ":" << error;
        }
        else
        {
            DEBUG << "Unloaded strategy" << strategyID;
        }
    }

    INFO << "All strategies stopped";
}

QVector<QString> StrategyManager::getActiveStrategies() const
{
    assumeMainAlgoThread(m_mainAlgo);
    return m_strategies.keys().toVector();
}

StrategyConfig StrategyManager::getStrategyConfig(const QString& p_strategyID) const
{
    assumeMainAlgoThread(m_mainAlgo);
    const auto* instance = findStrategy(p_strategyID);
    if (instance)
    {
        return instance->config;
    }
    return StrategyConfig{};
}

bool StrategyManager::isStrategyRunning(const QString& p_strategyID) const
{
    assumeMainAlgoThread(m_mainAlgo);
    const auto* instance = findStrategy(p_strategyID);
    if (instance)
    {
        return instance->state == StrategyState::RUNNING;
    }
    return false;
}

StrategyManager::StrategyExecutionState StrategyManager::getStrategyExecutionState(const QString& p_strategyID) const
{
    assumeMainAlgoThread(m_mainAlgo);
    const auto* instance = findStrategy(p_strategyID);
    if (instance == nullptr)
    {
        return StrategyExecutionState::Stopped;
    }

    return toExecutionState(instance->state);
}

int StrategyManager::getStrategyPositionCount(const QString& p_strategyID) const
{
    const auto* instance = findStrategy(p_strategyID);
    if (instance == nullptr)
    {
        return 0;
    }

    int count = 0;
    for (const auto& ledger: instance->symbolLedgers)
    {
        if (ledger.viewState.openQuantity != 0)
        {
            ++count;
        }
    }
    return count;
}

qint64 StrategyManager::getStrategyThreadId(const QString& p_strategyID) const
{
    const auto* instance = findStrategy(p_strategyID);
    if (instance && instance->p_backend)
    {
        // Return as qint64 (cast from Qt::HANDLE)
        return static_cast<qint64>(reinterpret_cast<uintptr_t>(instance->p_backend->threadHandle()));
    }
    return 0;
}

double StrategyManager::getStrategyBalance(const QString& p_strategyID) const
{
    const auto* instance = findStrategy(p_strategyID);
    if (instance && instance->p_backend && instance->p_backend->sdk())
    {
        // StrategySDK tracks balance via onBalanceUpdated callbacks
        // For now, return 0 as a placeholder (would need to store in SDK or Strategy)
        return 0.0; // TODO: Track balance in StrategySDK
    }
    return 0.0;
}

QVector<Order> StrategyManager::getStrategyRecentOrders(const QString& p_strategyID, int limit) const
{
    const auto* instance = findStrategy(p_strategyID);
    if (instance && instance->p_backend && instance->p_backend->sdk())
    {
        QVector<Order> allOrders = instance->p_backend->sdk()->getOrders();
        if (allOrders.size() > limit)
        {
            // Return most recent 'limit' orders
            return QVector<Order>(allOrders.end() - limit, allOrders.end());
        }
        return allOrders;
    }
    return QVector<Order>();
}

QVector<Position> StrategyManager::getStrategyOpenPositions(const QString& p_strategyID) const
{
    assumeMainAlgoThread(m_mainAlgo);
    const auto* instance = findStrategy(p_strategyID);
    if (instance && instance->p_backend && instance->p_backend->sdk())
    {
        return instance->p_backend->sdk()->getPositions();
    }
    return QVector<Position>();
}

QVector<StrategySymbolViewState> StrategyManager::getStrategySymbolViewStates(const QString& p_strategyID) const
{
    assumeMainAlgoThread(m_mainAlgo);
    const auto* instance = findStrategy(p_strategyID);
    if (instance == nullptr)
    {
        return {};
    }

    QVector<StrategySymbolViewState> states;
    states.reserve(instance->symbolLedgers.size());
    for (const auto& ledger: instance->symbolLedgers)
    {
        states.append(ledger.viewState);
    }

    std::sort(states.begin(),
              states.end(),
              [](const StrategySymbolViewState& p_left, const StrategySymbolViewState& p_right)
              {
                  if (p_left.originalOrder != p_right.originalOrder)
                  {
                      return p_left.originalOrder < p_right.originalOrder;
                  }

                  return p_left.symbol < p_right.symbol;
              });

    return states;
}

void StrategyManager::onMainAlgoPositionUpdated(const QString& p_account, const Position& p_position)
{
    assumeMainAlgoThread(m_mainAlgo);
    Q_UNUSED(p_account);

    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_backend)
        {
            instance->p_backend->publishPosition(p_position);
        }

        if (instance == nullptr)
        {
            continue;
        }

        auto ledgerIt = instance->symbolLedgers.find(p_position.getSymbol());
        if (ledgerIt == instance->symbolLedgers.end())
        {
            continue;
        }

        StrategyInstance::StrategySymbolLedger& ledger = ledgerIt.value();
        const qint64 rawQuantity = p_position.getQuantity().toLongLong();
        const double markPrice = positionMarkPrice(p_position);
        qint64 previousRawQuantity = rawQuantity;
        if (ledger.hasSeenRawPosition)
        {
            previousRawQuantity = ledger.lastRawQuantity;
        }
        else if (ledger.latestExecutionPrice.has_value())
        {
            previousRawQuantity = 0;
        }
        ledger.hasSeenRawPosition = true;
        ledger.lastRawQuantity = rawQuantity;
        ledger.lastMarkPrice = markPrice;

        if (ledger.viewState.openQuantity != 0)
        {
            const double signedQuantity = static_cast<double>(ledger.viewState.openQuantity);
            ledger.viewState.unrealizedPnl = (markPrice - ledger.viewState.averagePrice) * signedQuantity;
        }
        else if (ledger.viewState.hasActivity)
        {
            ledger.viewState.unrealizedPnl = 0.0;
        }

        const qint64 deltaQuantity = rawQuantity - previousRawQuantity;
        if (deltaQuantity == 0)
        {
            continue;
        }

        if (!ledger.viewState.hasActivity && !ledger.latestExecutionPrice.has_value())
        {
            continue;
        }

        const double fillPrice = ledger.latestExecutionPrice.value_or(markPrice);
        applyLedgerFillDelta(ledger, deltaQuantity, fillPrice);
        ledger.viewState.hasActivity = true;
        ledger.viewState.hasTradeHistory = true;
        ledger.viewState.latestActivitySequence = m_nextSymbolActivitySequence++;

        if (ledger.viewState.openQuantity != 0)
        {
            ledger.viewState.unrealizedPnl =
                (markPrice - ledger.viewState.averagePrice) * static_cast<double>(ledger.viewState.openQuantity);
        }
        else
        {
            ledger.viewState.unrealizedPnl = 0.0;
        }
    }
}

void StrategyManager::onMainAlgoBalanceUpdated(const Balance& p_balance)
{
    assumeMainAlgoThread(m_mainAlgo);
    double balance = p_balance.getEquity();

    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_backend)
        {
            instance->p_backend->publishBalance(balance);
        }
    }
}

QString StrategyManager::generateStrategyID()
{
    return QString("strategy_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

StrategyManager::StrategyInstance* StrategyManager::findStrategy(const QString& p_strategyID)
{
    auto it = m_strategies.find(p_strategyID);
    if (it != m_strategies.end())
    {
        return it.value();
    }
    return nullptr;
}

const StrategyManager::StrategyInstance* StrategyManager::findStrategy(const QString& p_strategyID) const
{
    auto it = m_strategies.find(p_strategyID);
    if (it != m_strategies.end())
    {
        return it.value();
    }
    return nullptr;
}

void StrategyManager::onOrderUpdatedForStrategy(const QString& p_strategyID, const Order& p_order)
{
    assumeMainAlgoThread(m_mainAlgo);
    qDebug(StrategyManagerLog) << "onOrderUpdatedForStrategy: strategyID=" << p_strategyID
                               << "orderID=" << p_order.getOrderID()
                               << "status=" << static_cast<int>(p_order.getOrderStatus());
    auto* instance = findStrategy(p_strategyID);
    if (!instance || !instance->p_backend)
    {
        qWarning(StrategyManagerLog) << "onOrderUpdatedForStrategy: strategy not found:" << p_strategyID;
        return;
    }

    if (auto* ledger = ensureSymbolLedger(instance, p_order.getSymbol()); ledger != nullptr)
    {
        ledger->viewState.hasActivity = true;
        ledger->viewState.latestActivitySequence = m_nextSymbolActivitySequence++;
        if (isFillLikeStatus(p_order.getOrderStatus()) && p_order.getFilledPrice() > 0.0)
        {
            ledger->latestExecutionPrice = p_order.getFilledPrice();
        }
    }

    instance->p_backend->publishOrder(p_order);
}

void StrategyManager::publishManualOrderDecision(const QString& p_strategyID,
                                                 const QString& p_requestID,
                                                 const QString& p_symbol,
                                                 const StrategyManualOrderDecision p_decision,
                                                 const QString& p_reason)
{
    assumeMainAlgoThread(m_mainAlgo);
    auto* instance = findStrategy(p_strategyID);
    if (instance == nullptr || instance->p_backend == nullptr)
    {
        qWarning(StrategyManagerLog) << "publishManualOrderDecision: strategy not found:" << p_strategyID
                                     << "requestID=" << p_requestID;
        return;
    }

    instance->p_backend->publishManualOrderDecision(p_requestID, p_symbol, p_decision, p_reason);
}

StrategyManager::StrategyInstance::StrategySymbolLedger*
StrategyManager::ensureSymbolLedger(StrategyInstance* const p_instance,
                                    const QString& p_symbol,
                                    const int p_originalOrder)
{
    if (p_instance == nullptr)
    {
        return nullptr;
    }

    auto it = p_instance->symbolLedgers.find(p_symbol);
    if (it != p_instance->symbolLedgers.end())
    {
        if (p_originalOrder >= 0 && !it->viewState.hasActivity)
        {
            it->viewState.originalOrder = p_originalOrder;
        }
        return &it.value();
    }

    StrategyInstance::StrategySymbolLedger ledger;
    ledger.viewState.symbol = p_symbol;
    ledger.viewState.originalOrder = p_originalOrder >= 0 ? p_originalOrder : p_instance->symbolLedgers.size();
    auto inserted = p_instance->symbolLedgers.insert(p_symbol, ledger);
    return &inserted.value();
}

void StrategyManager::applyLedgerFillDelta(StrategyInstance::StrategySymbolLedger& p_ledger,
                                           const qint64 p_deltaQuantity,
                                           const double p_fillPrice)
{
    if (p_deltaQuantity == 0 || p_fillPrice <= 0.0)
    {
        return;
    }

    qint64 openQuantity = p_ledger.viewState.openQuantity;
    double averagePrice = p_ledger.viewState.averagePrice;
    const qint64 absoluteDelta = qAbs(p_deltaQuantity);

    if (p_deltaQuantity > 0)
    {
        if (openQuantity >= 0)
        {
            const double totalCost =
                (static_cast<double>(openQuantity) * averagePrice) + (static_cast<double>(absoluteDelta) * p_fillPrice);
            openQuantity += absoluteDelta;
            averagePrice = openQuantity > 0 ? (totalCost / static_cast<double>(openQuantity)) : 0.0;
        }
        else
        {
            const qint64 closedQuantity = qMin(absoluteDelta, -openQuantity);
            p_ledger.viewState.realizedPnl += static_cast<double>(closedQuantity) * (averagePrice - p_fillPrice);
            openQuantity += absoluteDelta;
            if (openQuantity > 0)
            {
                averagePrice = p_fillPrice;
            }
            else if (openQuantity == 0)
            {
                averagePrice = 0.0;
            }
        }
    }
    else
    {
        if (openQuantity <= 0)
        {
            const double existingShortQty = static_cast<double>(qAbs(openQuantity));
            const double totalCost =
                (existingShortQty * averagePrice) + (static_cast<double>(absoluteDelta) * p_fillPrice);
            openQuantity -= absoluteDelta;
            averagePrice = openQuantity < 0 ? (totalCost / static_cast<double>(qAbs(openQuantity))) : 0.0;
        }
        else
        {
            const qint64 closedQuantity = qMin(absoluteDelta, openQuantity);
            p_ledger.viewState.realizedPnl += static_cast<double>(closedQuantity) * (p_fillPrice - averagePrice);
            openQuantity -= absoluteDelta;
            if (openQuantity < 0)
            {
                averagePrice = p_fillPrice;
            }
            else if (openQuantity == 0)
            {
                averagePrice = 0.0;
            }
        }
    }

    p_ledger.viewState.openQuantity = openQuantity;
    p_ledger.viewState.averagePrice = averagePrice;
}

void StrategyManager::connectStrategyToDataSources(StrategyInstance* p_instance)
{
    // Data connections (bar/level2/trade) are now wired per-symbol in connectSymbolToStrategy,
    // directly from each SymbolContext to the adapter without bouncing through MainAlgo.
    // This function is intentionally empty; retained for symmetry with
    // disconnectStrategyFromDataSources which handles global cleanup on unload.
    (void)p_instance;
}

void StrategyManager::disconnectStrategyFromDataSources(StrategyInstance* p_instance)
{
    if (!p_instance || !p_instance->p_backend)
    {
        return;
    }

    // Disconnect all tracked connections to this adapter
    for (const auto& connection: p_instance->m_connections)
    {
        QObject::disconnect(connection);
    }
    p_instance->m_connections.clear();

    qInfo(StrategyManagerLog) << "Disconnected strategy from data sources:" << p_instance->strategyID;
}

void StrategyManager::connectSymbolToStrategy(const QString& p_strategyID,
                                              const QString& p_symbol,
                                              SymbolContext* p_instrument)
{
    assumeMainAlgoThread(m_mainAlgo);
    auto* instance = findStrategy(p_strategyID);
    if (!instance || !instance->p_backend)
    {
        qWarning(StrategyManagerLog) << "connectSymbolToStrategy: strategy not found:" << p_strategyID;
        return;
    }

    instance->p_backend->trackMonitoredSymbol(p_symbol);

    OBJ_ASSUME_DIFF(p_instrument, nullptr);
    auto c1 = connect(
        &p_instrument->barReceiver,
        &BarReceiver::receivedNewBar,
        this,
        [this, p_strategyID](const QString& symbol, const Bar& bar)
        {
            if (auto* strategy = findStrategy(p_strategyID); strategy && strategy->p_backend)
            {
                strategy->p_backend->publishBar(symbol, bar);
            }
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(c1);
    instance->m_connections.push_back(c1);

    auto c2 = connect(
        &p_instrument->m_level2Receiver,
        &Level2Receiver::receivedNewLevel2,
        this,
        [this, p_strategyID](const QString& symbol, const Level2& level2)
        {
            if (auto* strategy = findStrategy(p_strategyID); strategy && strategy->p_backend)
            {
                strategy->p_backend->publishLevel2(symbol, level2);
            }
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(c2);
    instance->m_connections.push_back(c2);

    auto c3 = connect(
        p_instrument,
        &SymbolContext::receivedNewTrade,
        this,
        [this, p_strategyID](const QString& symbol, const Trade& trade)
        {
            if (auto* strategy = findStrategy(p_strategyID); strategy && strategy->p_backend)
            {
                strategy->p_backend->publishTrade(symbol, trade);
            }
        },
        Qt::QueuedConnection);
    ASSUME_TRUE(c3);
    instance->m_connections.push_back(c3);

    qInfo(StrategyManagerLog) << "Connected symbol" << p_symbol << "to strategy" << p_strategyID;
}

StrategyLogger* StrategyManager::getStrategyLogger(const QString& p_strategyID)
{
    assumeMainAlgoThread(m_mainAlgo);
    auto instance = findStrategy(p_strategyID);
    return instance && instance->p_backend ? instance->p_backend->logger() : nullptr;
}

const StrategyLogger* StrategyManager::getStrategyLogger(const QString& p_strategyID) const
{
    assumeMainAlgoThread(m_mainAlgo);
    auto instance = findStrategy(p_strategyID);
    return instance && instance->p_backend ? instance->p_backend->logger() : nullptr;
}

void StrategyManager::processClaimSymbols(const QString& p_strategyID,
                                          const QStringList& p_symbols,
                                          std::shared_ptr<QPromise<QStringList>> p_promise)
{
    assumeMainAlgoThread(m_mainAlgo);
    QStringList approved;
    StrategyInstance* const instance = findStrategy(p_strategyID);
    const QStringList requestedSymbols = normalizeRequestedSymbols(p_symbols);

    for (const QString& symbol: requestedSymbols)
    {
        if (instance != nullptr && instance->blockedSymbols.contains(symbol))
        {
            qInfo(StrategyManagerLog) << "Symbol claim denied:" << symbol << "is blocked for current strategy session"
                                      << p_strategyID;
            continue;
        }

        if (m_symbolRegistry.contains(symbol))
        {
            const QString& owner = m_symbolRegistry[symbol];
            if (owner != p_strategyID)
            {
                qWarning(StrategyManagerLog) << "Symbol claim denied:" << symbol << "already owned by" << owner
                                             << "(requested by" << p_strategyID << ")";
                continue; // excluded from approved list
            }
            approved.append(symbol);
            continue;
        }

        // Subscribe data feeds for this symbol before granting the claim so replay/live
        // unavailability turns into a clean symbol-level rejection instead of a silent claim.
        auto boolPromise = std::make_shared<QPromise<bool>>();
        boolPromise->start();
        QFuture<bool> subscribeFuture = boolPromise->future();
        m_mainAlgo->processSubscribeToSymbol(p_strategyID, symbol, boolPromise);
        subscribeFuture.waitForFinished();

        if (!subscribeFuture.isValid() || subscribeFuture.resultCount() == 0 || !subscribeFuture.result())
        {
            qWarning(StrategyManagerLog) << "Symbol claim rejected after subscription failure:" << symbol
                                         << "(requested by" << p_strategyID << ")";
            continue;
        }

        m_symbolRegistry[symbol] = p_strategyID;
        approved.append(symbol);
    }

    QStringList mergedClaimedSymbols;
    if (instance)
    {
        QStringList existingClaims;
        existingClaims.reserve(instance->monitoredSymbols.size());
        for (const QString& symbol: instance->monitoredSymbols)
        {
            existingClaims.append(symbol);
        }
        mergedClaimedSymbols = normalizeRequestedSymbols(existingClaims);
    }

    for (const QString& symbol: approved)
    {
        if (!mergedClaimedSymbols.contains(symbol))
        {
            mergedClaimedSymbols.append(symbol);
        }
    }

    // Update SDK's claimed symbols on the strategy thread
    if (instance && instance->p_backend && instance->p_backend->sdk())
    {
        QMetaObject::invokeMethod(
            instance->p_backend->sdk(),
            [sdk = instance->p_backend->sdk(), mergedClaimedSymbols]()
            { sdk->setClaimedSymbols(mergedClaimedSymbols); },
            Qt::QueuedConnection);
    }

    // Update monitored symbols list for StrategyQuickView
    if (instance)
    {
        instance->monitoredSymbols = QVector<QString>(mergedClaimedSymbols.begin(), mergedClaimedSymbols.end());
        QMap<QString, StrategyInstance::StrategySymbolLedger> updatedLedgers;
        for (int index = 0; index < mergedClaimedSymbols.size(); ++index)
        {
            const QString& symbol = mergedClaimedSymbols[index];
            StrategyInstance::StrategySymbolLedger* existingLedger = ensureSymbolLedger(instance, symbol, index);
            ASSUME_DIFF(existingLedger, nullptr);
            existingLedger->viewState.symbol = symbol;
            if (!existingLedger->viewState.hasActivity)
            {
                existingLedger->viewState.originalOrder = index;
            }
            updatedLedgers.insert(symbol, *existingLedger);
        }
        instance->symbolLedgers = std::move(updatedLedgers);
    }

    if (instance && instance->p_backend && instance->p_backend->isThreadRunning())
    {
        instance->p_backend->publishClaimedSymbols(mergedClaimedSymbols);
    }

    qInfo(StrategyManagerLog) << "Strategy" << p_strategyID << "claimed symbols:" << approved
                              << "(active claims:" << mergedClaimedSymbols << ")";

    emit symbolsClaimed(p_strategyID, mergedClaimedSymbols);

    p_promise->addResult(approved);
    p_promise->finish();
}

QString
StrategyManager::unclaimSymbol(const QString& p_strategyID, const QString& p_symbol, const bool p_blockForSession)
{
    assumeMainAlgoThread(m_mainAlgo);

    StrategyInstance* const instance = findStrategy(p_strategyID);
    if (instance == nullptr)
    {
        return QStringLiteral("Strategy not found: %1").arg(p_strategyID);
    }

    const QString symbol = p_symbol.trimmed().toUpper();
    if (symbol.isEmpty())
    {
        return QStringLiteral("Symbol is empty");
    }

    const QString owner = m_symbolRegistry.value(symbol);
    if (owner.isEmpty())
    {
        return QStringLiteral("Symbol %1 is not currently claimed").arg(symbol);
    }
    if (owner != p_strategyID)
    {
        return QStringLiteral("Symbol %1 is claimed by another strategy").arg(symbol);
    }

    auto ledgerIt = instance->symbolLedgers.constFind(symbol);
    if (ledgerIt != instance->symbolLedgers.constEnd() && ledgerIt->viewState.openQuantity != 0)
    {
        return QStringLiteral("Cannot unclaim %1 while strategy position is open (%2 shares)")
            .arg(symbol)
            .arg(ledgerIt->viewState.openQuantity);
    }

    if (instance->p_backend != nullptr && instance->p_backend->sdk() != nullptr)
    {
        const QVector<Position> positions = instance->p_backend->sdk()->getPositions();
        for (const Position& position: positions)
        {
            if (position.getSymbol().trimmed().toUpper() == symbol && position.getQuantity().toLongLong() != 0)
            {
                return QStringLiteral("Cannot unclaim %1 while strategy position is open").arg(symbol);
            }
        }
    }

    m_symbolRegistry.remove(symbol);
    emit symbolReleased(symbol);

    if (p_blockForSession)
    {
        instance->blockedSymbols.insert(symbol);
    }

    instance->monitoredSymbols.removeAll(symbol);
    instance->symbolLedgers.remove(symbol);

    QStringList activeClaims =
        normalizeRequestedSymbols(QStringList(instance->monitoredSymbols.begin(), instance->monitoredSymbols.end()));
    instance->monitoredSymbols = QVector<QString>(activeClaims.begin(), activeClaims.end());

    if (instance->p_backend && instance->p_backend->sdk())
    {
        QMetaObject::invokeMethod(
            instance->p_backend->sdk(),
            [sdk = instance->p_backend->sdk(), activeClaims]() { sdk->setClaimedSymbols(activeClaims); },
            Qt::QueuedConnection);
    }
    if (instance->p_backend && instance->p_backend->isThreadRunning())
    {
        instance->p_backend->publishClaimedSymbols(activeClaims);
    }

    m_mainAlgo->processStrategyStatusClear(p_strategyID, symbol);
    emit symbolsClaimed(p_strategyID, activeClaims);

    qInfo(StrategyManagerLog) << "Strategy" << p_strategyID << "unclaimed symbol" << symbol
                              << "(blocked for session:" << p_blockForSession << ", active claims:" << activeClaims
                              << ")";

    return {};
}

void StrategyManager::releaseSymbols(const QString& p_strategyID)
{
    assumeMainAlgoThread(m_mainAlgo);
    const QStringList released = m_symbolRegistry.keys(p_strategyID);
    for (const QString& symbol: released)
    {
        m_symbolRegistry.remove(symbol);
        emit symbolReleased(symbol);
    }
    if (!released.isEmpty())
    {
        qInfo(StrategyManagerLog) << "Released symbols for strategy" << p_strategyID << ":" << released;
    }

    // Clear the SDK's claimed symbols
    auto* instance = findStrategy(p_strategyID);
    if (instance && instance->p_backend && instance->p_backend->sdk())
    {
        QMetaObject::invokeMethod(
            instance->p_backend->sdk(),
            [sdk = instance->p_backend->sdk()]() { sdk->clearClaimedSymbols(); },
            Qt::QueuedConnection);
    }
    if (instance)
    {
        instance->monitoredSymbols.clear();
        instance->symbolLedgers.clear();
        if (instance->p_backend && instance->p_backend->isThreadRunning())
        {
            instance->p_backend->publishClaimedSymbols({});
        }
    }

    emit symbolsClaimed(p_strategyID, {});
}

void StrategyManager::persistStrategiesState()
{
    if (!strategiesStateSettings || !m_persistEnabled)
        return;

    // Pass explicit size so QSettings IniFormat writes the correct "size=N" key.
    // Without it, Qt sets size to the last setArrayIndex() value (0-based) rather
    // than the element count, causing beginReadArray() to return 0 on next launch.
    strategiesStateSettings->beginWriteArray("LoadedStrategies", m_strategies.size());
    int index = 0;
    for (auto it = m_strategies.constBegin(); it != m_strategies.constEnd(); ++it)
    {
        strategiesStateSettings->setArrayIndex(index++);
        const StrategyInstance* instance = it.value();
        QJsonDocument doc(instance->config.toJson());
        // Store as QString so QSettings INI reads it back as a string (not @ByteArray).
        strategiesStateSettings->setValue("config", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        strategiesStateSettings->setValue("wasRunning",
                                          instance->state == StrategyState::QUEUED ||
                                              instance->state == StrategyState::RUNNING ||
                                              instance->state == StrategyState::PAUSED);
    }
    strategiesStateSettings->endArray();
    strategiesStateSettings->sync();
}

void StrategyManager::restoreStrategiesState()
{
    assumeMainAlgoThread(m_mainAlgo);
    if (!strategiesStateSettings)
        return;

    m_persistEnabled = true; // Re-enable so loadStrategy() calls below update the file

    int size = strategiesStateSettings->beginReadArray("LoadedStrategies");
    qInfo(StrategyManagerLog) << "Restoring" << size << "strategies from StrategiesState.ini";

    for (int i = 0; i < size; ++i)
    {
        strategiesStateSettings->setArrayIndex(i);
        QString configJson = strategiesStateSettings->value("config").toString();
        bool wasRunning = strategiesStateSettings->value("wasRunning", false).toBool();

        if (configJson.isEmpty())
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(configJson.toUtf8());
        if (doc.isNull() || !doc.isObject())
        {
            qWarning(StrategyManagerLog) << "Invalid config JSON in StrategiesState.ini at index" << i << ", skipping";
            continue;
        }

        StrategyConfig config = StrategyConfig::fromJson(doc.object());
        auto result = loadStrategy(config);
        if (!result)
        {
            qWarning(StrategyManagerLog) << "Failed to restore strategy:" << config.name << "-" << result.error();
            continue;
        }

        if (wasRunning)
        {
            QString error;
            if (MainApp::isInReplayMode() && !MainApp::getInstance()->hasReplayPlaybackStarted())
            {
                auto* restoredInstance = findStrategy(result.value());
                ASSUME_DIFF(restoredInstance, nullptr);
                error = queueStrategyStart(restoredInstance);
            }
            else
            {
                error = startStrategy(result.value());
            }
            if (!error.isEmpty())
            {
                qWarning(StrategyManagerLog)
                    << "Failed to auto-start restored strategy:" << config.name << "-" << error;
            }
        }
    }
    strategiesStateSettings->endArray();
}

QString StrategyManager::startQueuedReplayStrategies()
{
    assumeMainAlgoThread(m_mainAlgo);

    for (auto it = m_strategies.begin(); it != m_strategies.end(); ++it)
    {
        StrategyInstance* const instance = it.value();
        ASSUME_DIFF(instance, nullptr);

        if (instance->state != StrategyState::QUEUED)
        {
            continue;
        }

        const QString error = startStrategyInternal(instance, true);
        if (!error.isEmpty())
        {
            return QString("Failed to start queued strategy \"%1\": %2").arg(instance->config.name, error);
        }
    }

    return "";
}

void StrategyManager::pauseReplayStrategies()
{
    assumeMainAlgoThread(m_mainAlgo);

    for (auto it = m_strategies.begin(); it != m_strategies.end(); ++it)
    {
        StrategyInstance* const instance = it.value();
        ASSUME_DIFF(instance, nullptr);

        if (instance->state != StrategyState::RUNNING || instance->p_backend == nullptr)
        {
            continue;
        }

        const QString error = instance->p_backend->pause(QStringLiteral("Replay paused"));
        if (!error.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to pause strategy for replay:" << instance->strategyID << error;
            continue;
        }

        setStrategyState(instance, StrategyState::PAUSED);
    }
}

void StrategyManager::resumeReplayStrategies()
{
    assumeMainAlgoThread(m_mainAlgo);

    for (auto it = m_strategies.begin(); it != m_strategies.end(); ++it)
    {
        StrategyInstance* const instance = it.value();
        ASSUME_DIFF(instance, nullptr);

        if (instance->state != StrategyState::PAUSED || instance->p_backend == nullptr)
        {
            continue;
        }

        const QString error = instance->p_backend->resume();
        if (!error.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Failed to resume strategy for replay:" << instance->strategyID << error;
            continue;
        }

        setStrategyState(instance, StrategyState::RUNNING);
    }
}

void StrategyManager::onReplayPaused()
{
    if (!MainApp::isInReplayMode() || !MainApp::getInstance()->hasReplayPlaybackStarted())
    {
        return;
    }

    pauseReplayStrategies();
}

void StrategyManager::onReplayResumed()
{
    if (!MainApp::isInReplayMode() || !MainApp::getInstance()->hasReplayPlaybackStarted())
    {
        return;
    }

    resumeReplayStrategies();
}
