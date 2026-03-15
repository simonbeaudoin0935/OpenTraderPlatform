#include "StrategyManager.h"
#include "StrategySDK.h"
#include "../Algo/MainAlgo.h"
#include "../Core/MainApp.h"
#include "Assume.h"
#include "OrdersDatabase.h"
#include "Settings.h"
#include <QUuid>
#include <QDebug>
#include <QPromise>
#include <QJsonDocument>

#define LOGGING_CATEGORY StrategyManagerLog

#include "Logging.h"

Q_LOGGING_CATEGORY(StrategyManagerLog, "StrategyManager")

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
        return QtFuture::makeReadyFuture(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::RejectedByValidator)));
    }

    // Validate order before sending to MainAlgo
    if (!StrategyOrderValidator::validateOrder(m_strategyID, p_order))
    {
        return QtFuture::makeReadyFuture(
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

const QString& StrategySDK::getStrategyName() const
{
    return m_config.name;
}

std::shared_ptr<QVector<Bar>> StrategySDK::getHistoricalBars(const QString& /* symbol */,
                                                             const QDate& day,
                                                             const QTime& first,
                                                             const QTime& last,
                                                             TimeFrame tf)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    // Request bars from MainAlgo (which has access to all SymbolContext and their BarCaches)
    auto result = m_mainAlgo->requestMissingBarsDisplayedStock(day, first, last, tf);

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

std::expected<QString, QString> StrategyManager::loadStrategy(const StrategyConfig& p_config)
{
    ASSUME_DIFF(m_mainAlgo, nullptr);

    if (p_config.soPath.isEmpty())
    {
        return std::unexpected("Strategy .so path is empty");
    }

    auto pluginResult = StrategyLoader::loadPlugin(p_config.soPath);
    if (!pluginResult)
    {
        return std::unexpected(pluginResult.error());
    }

    auto plugin = pluginResult.value();
    QString strategyID = generateStrategyID();

    // Create strategy logger FIRST (before SDK so it can be passed)
    auto p_logger = std::make_unique<StrategyLogger>(p_config.name);

    auto p_sdk = new StrategySDK(m_mainAlgo, strategyID, p_config, p_logger.get());

    StrategyBase* p_strategy = plugin.createFn(p_config, p_sdk);
    if (!p_strategy)
    {
        delete p_sdk;
        StrategyLoader::unloadPlugin(plugin);
        return std::unexpected("Failed to create strategy instance for: " + p_config.name);
    }

    // Set SDK reference in strategy
    p_strategy->setSdk(p_sdk);

    // Create callback adapter that will live on strategy's thread
    auto p_adapter = new StrategyCallbackAdapter(p_strategy, p_sdk, p_config.symbols);

    auto instance = new StrategyInstance();
    instance->strategyID = strategyID;
    instance->config = p_config;
    instance->plugin = plugin;
    instance->p_strategy = p_strategy;
    instance->p_sdk = p_sdk;
    instance->p_adapter = p_adapter;
    instance->p_logger = std::move(p_logger);
    instance->monitoredSymbols = p_config.symbols;

    // Set thread name for debugging (before starting thread)
    instance->m_thread.setObjectName(QString("Strategy_%1_%2").arg(p_config.name).arg(strategyID.left(8)));

    p_sdk->moveToThread(&instance->m_thread);
    p_adapter->moveToThread(&instance->m_thread);

    // Move strategy to thread if it's a QObject (strategies that use Qt signals/slots)
    if (auto* qobj = dynamic_cast<QObject*>(p_strategy))
    {
        qobj->moveToThread(&instance->m_thread);
    }

    connectStrategyToDataSources(instance);

    // Note: Thread is NOT started here - wait for startStrategy() to be called
    // This allows the UI to show the strategy in LOADED state and wait for user to click Start

    qInfo(StrategyManagerLog) << "Loaded strategy:" << p_config.name << "ID:" << strategyID;

    m_strategies[strategyID] = instance;

    emit strategyLoaded(strategyID, p_config.name);

    persistStrategiesState();

    return strategyID;
}

QString StrategyManager::unloadStrategy(const QString& p_strategyID)
{
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        return "Strategy not found: " + p_strategyID;
    }

    qInfo(StrategyManagerLog) << "Unloading strategy:" << instance->config.name << "ID:" << p_strategyID;

    // Release all symbol claims before disconnecting
    releaseSymbols(p_strategyID);

    disconnectStrategyFromDataSources(instance);

    // Call onStop on the strategy thread before quitting
    // Only if thread is still running (not crashed)
    if (instance->m_thread.isRunning() && instance->p_adapter)
    {
        QMetaObject::invokeMethod(instance->p_adapter, "callOnStop", Qt::BlockingQueuedConnection);
        qInfo(StrategyManagerLog) << "Called onStop for strategy:" << instance->config.name;
    }
    else if (!instance->m_thread.isRunning() && instance->p_adapter)
    {
        // Thread already dead (likely crashed) - call onStop directly in current thread
        // This may not be ideal but prevents deadlock
        qWarning(StrategyManagerLog) << "Strategy thread already dead, calling onStop in current thread:"
                                     << instance->config.name;
        instance->p_adapter->callOnStop();
    }

    // Log the file path (logs are written incrementally, nothing to flush)
    if (instance->p_logger)
    {
        qInfo(StrategyManagerLog) << "Strategy log file:" << instance->p_logger->getLogFilePath();
    }

    // Signal thread to quit gracefully
    instance->m_thread.quit();

    // Wait for thread to finish gracefully (5 second timeout)
    if (!instance->m_thread.wait(5000))
    {
        qWarning(StrategyManagerLog) << "Strategy thread did not finish within timeout, terminating:"
                                     << instance->config.name;
        instance->m_thread.terminate();
        instance->m_thread.wait();
    }

    // Clean up adapter
    if (instance->p_adapter)
    {
        delete instance->p_adapter;
    }

    // Destroy strategy instance via factory function
    if (instance->plugin.destroyFn && instance->p_strategy)
    {
        instance->plugin.destroyFn(instance->p_strategy);
        qInfo(StrategyManagerLog) << "Destroyed strategy instance:" << instance->config.name;
    }

    // Clean up SDK
    if (instance->p_sdk)
    {
        delete instance->p_sdk;
    }

    // Unload plugin .so file
    StrategyLoader::unloadPlugin(instance->plugin);
    qInfo(StrategyManagerLog) << "Unloaded plugin for strategy:" << instance->config.name;

    // Remove from registry and delete instance
    delete m_strategies.take(p_strategyID);

    qInfo(StrategyManagerLog) << "Successfully unloaded strategy:" << p_strategyID;

    emit strategyUnloaded(p_strategyID);

    persistStrategiesState();

    return "";
}

QString StrategyManager::startStrategy(const QString& p_strategyID)
{
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        return "Strategy not found: " + p_strategyID;
    }

    if (instance->state != StrategyState::LOADED)
    {
        return "Strategy is not in LOADED state (current state: " + QString::number(static_cast<int>(instance->state)) +
               ")";
    }

    // Connect thread started signal to install signal handler and call onStart
    QObject::connect(&instance->m_thread,
                     &QThread::started,
                     [this, instance]()
                     {
                         // Capture thread handle for stats reading
                         instance->threadHandle = QThread::currentThreadId();

                         // Install signal handlers for this strategy thread
                         if (!StrategySignalHandler::installSignalHandler(instance->strategyID))
                         {
                             qWarning(StrategyManagerLog)
                                 << "Failed to install signal handler for strategy:" << instance->strategyID;
                         }

                         // Call strategy's onStart hook
                         if (instance->p_strategy)
                         {
                             instance->p_strategy->onStart(instance->p_sdk);
                         }

                         // Update state to RUNNING
                         instance->state = StrategyState::RUNNING;
                         emit strategyStatusChanged(instance->strategyID, true, "");

                         // Persist updated running state (back on MainAlgo thread)
                         QMetaObject::invokeMethod(this, [this]() { persistStrategiesState(); }, Qt::QueuedConnection);
                     });

    // Start the thread
    instance->m_thread.start();

    qInfo(StrategyManagerLog) << "Started strategy:" << instance->config.name << "ID:" << p_strategyID;

    return "";
}

void StrategyManager::markStrategyFailed(const QString& p_strategyID, const QString& p_errorMessage)
{
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        qWarning(StrategyManagerLog) << "markStrategyFailed: Strategy not found:" << p_strategyID;
        return;
    }

    qCritical(StrategyManagerLog) << "Strategy marked as FAILED:" << instance->config.name
                                  << "Error:" << p_errorMessage;

    // Emit status changed signal with error state
    emit strategyStatusChanged(p_strategyID, false, p_errorMessage);

    // Stop the thread if it's still running (it may have already crashed)
    if (instance->m_thread.isRunning())
    {
        instance->m_thread.quit();
        if (!instance->m_thread.wait(2000))
        {
            qWarning(StrategyManagerLog) << "Strategy thread did not quit, terminating:" << instance->config.name;
            instance->m_thread.terminate();
            instance->m_thread.wait();
        }
    }
}

void StrategyManager::stopAllStrategies()
{
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

void StrategyManager::markStrategyFailedFromSignal(const QString& p_strategyID, const QString& p_errorMessage)
{
    // Logging here since we moved qCritical out of signal handler
    qCritical(StrategyManagerLog) << "Strategy thread crashed with signal:" << p_strategyID << "-" << p_errorMessage;
    markStrategyFailed(p_strategyID, p_errorMessage);
}

QVector<QString> StrategyManager::getActiveStrategies() const
{
    return m_strategies.keys().toVector();
}

StrategyConfig StrategyManager::getStrategyConfig(const QString& p_strategyID) const
{
    const auto* instance = findStrategy(p_strategyID);
    if (instance)
    {
        return instance->config;
    }
    return StrategyConfig{};
}

bool StrategyManager::isStrategyRunning(const QString& p_strategyID) const
{
    const auto* instance = findStrategy(p_strategyID);
    if (instance)
    {
        return instance->state == StrategyState::RUNNING;
    }
    return false;
}

int StrategyManager::getStrategyPositionCount(const QString& p_strategyID) const
{
    Q_UNUSED(p_strategyID);
    // TODO: Implement position tracking per strategy (Phase 1.6)
    // For now, return 0 (placeholder)
    return 0;
}

qint64 StrategyManager::getStrategyThreadId(const QString& p_strategyID) const
{
    const auto* instance = findStrategy(p_strategyID);
    if (instance)
    {
        // Return as qint64 (cast from Qt::HANDLE)
        return static_cast<qint64>(reinterpret_cast<uintptr_t>(instance->threadHandle));
    }
    return 0;
}

double StrategyManager::getStrategyBalance(const QString& p_strategyID) const
{
    const auto* instance = findStrategy(p_strategyID);
    if (instance && instance->p_sdk)
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
    if (instance && instance->p_sdk)
    {
        QVector<Order> allOrders = instance->p_sdk->getOrders();
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
    const auto* instance = findStrategy(p_strategyID);
    if (instance && instance->p_sdk)
    {
        return instance->p_sdk->getPositions();
    }
    return QVector<Position>();
}

void StrategyManager::onBarReceived(const QString& p_symbol, const Bar& p_bar)
{
    for (auto* instance: m_strategies)
    {
        if (instance && instance->monitoredSymbols.contains(p_symbol))
        {
            if (instance->p_strategy)
            {
                instance->p_strategy->onBar(p_bar);
            }
        }
    }
}

void StrategyManager::onLevel2Received(const QString& p_symbol, const Level2& p_level2)
{
    for (auto* instance: m_strategies)
    {
        if (instance && instance->monitoredSymbols.contains(p_symbol))
        {
            if (instance->p_strategy)
            {
                instance->p_strategy->onLevel2(p_level2);
            }
        }
    }
}

void StrategyManager::onOrderFilled(const Order& p_order)
{
    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_strategy)
        {
            instance->p_strategy->onOrderFilled(p_order);
        }
    }
}

void StrategyManager::onOrderCancelled(const Order& p_order)
{
    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_strategy)
        {
            instance->p_strategy->onOrderCancelled(p_order, "");
        }
    }
}

void StrategyManager::onOrderRejected(const Order& p_order, const QString& p_reason)
{
    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_strategy)
        {
            instance->p_strategy->onOrderRejected(p_order, p_reason.toStdString());
        }
    }
}

void StrategyManager::onPositionUpdated(const Position& p_position)
{
    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_strategy)
        {
            // Update SDK state so getStrategyOpenPositions() returns current data
            if (instance->p_sdk)
                instance->p_sdk->updatePosition(p_position);
            instance->p_strategy->onPositionUpdated(p_position);
        }
    }
}

void StrategyManager::onMainAlgoPositionUpdated(const QString& p_account, const Position& p_position)
{
    Q_UNUSED(p_account);
    onPositionUpdated(p_position);
}

void StrategyManager::onBalanceUpdated(double p_newBalance)
{
    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_strategy)
        {
            instance->p_strategy->onBalanceUpdated(p_newBalance);
            // Emit signal for UI updates
            emit strategyBalanceUpdated(instance->strategyID, p_newBalance);
        }
    }
}

void StrategyManager::onMainAlgoBalanceUpdated(const Balance& p_balance)
{
    // Extract balance value and route to all strategies
    double balance = p_balance.getEquity();
    onBalanceUpdated(balance);
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

StrategyBase* StrategyManager::getStrategy(const QString& p_strategyID) const
{
    auto instance = findStrategy(p_strategyID);
    return instance ? instance->p_strategy : nullptr;
}

void StrategyManager::onOrderUpdated(const Order& p_order)
{
    // Broadcast to all strategies (used as fallback when owning strategy is unknown)
    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_strategy)
        {
            instance->p_strategy->onOrderUpdated(p_order);
        }
    }
}

void StrategyManager::onOrderUpdatedForStrategy(const QString& p_strategyID, const Order& p_order)
{
    qDebug(StrategyManagerLog) << "onOrderUpdatedForStrategy: strategyID=" << p_strategyID
                               << "orderID=" << p_order.getOrderID()
                               << "status=" << static_cast<int>(p_order.getOrderStatus());
    auto* instance = findStrategy(p_strategyID);
    if (!instance || !instance->p_adapter)
    {
        qWarning(StrategyManagerLog) << "onOrderUpdatedForStrategy: strategy not found:" << p_strategyID;
        return;
    }

    // Route via adapter (runs on strategy thread)
    QMetaObject::invokeMethod(
        instance->p_adapter,
        [instance, p_order]()
        {
            qDebug() << "[StrategyCallbackAdapter] dispatching onOrderUpdated orderID=" << p_order.getOrderID();
            instance->p_adapter->onOrderUpdated(p_order);
        },
        Qt::QueuedConnection);
}

void StrategyManager::onMainAlgoOrderUpdated(const QString& p_account, const Order& p_order)
{
    Q_UNUSED(p_account);
    onOrderUpdated(p_order);
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
    if (!p_instance || !p_instance->p_adapter)
    {
        return;
    }

    // Disconnect all tracked connections to this adapter
    for (const auto& connection : p_instance->m_connections)
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
    auto* instance = findStrategy(p_strategyID);
    if (!instance || !instance->p_adapter)
    {
        qWarning(StrategyManagerLog) << "connectSymbolToStrategy: strategy not found:" << p_strategyID;
        return;
    }

    // Add symbol to adapter's monitored set (must happen on strategy thread)
    QMetaObject::invokeMethod(
        instance->p_adapter,
        [adapter = instance->p_adapter, p_symbol]() { adapter->addMonitoredSymbol(p_symbol); },
        Qt::BlockingQueuedConnection);

    // Connect SymbolContext signals directly to the adapter (no MainAlgo hop)
    OBJ_ASSUME_DIFF(p_instrument, nullptr);

    auto c1 = connect(&p_instrument->barReceiver,
                      &BarReceiver::receivedNewBar,
                      instance->p_adapter,
                      &StrategyCallbackAdapter::onBar,
                      Qt::QueuedConnection);
    ASSUME_TRUE(c1);
    instance->m_connections.push_back(c1);

    auto c2 = connect(&p_instrument->m_level2Receiver,
                      &Level2Receiver::receivedNewLevel2,
                      instance->p_adapter,
                      &StrategyCallbackAdapter::onLevel2,
                      Qt::QueuedConnection);
    ASSUME_TRUE(c2);
    instance->m_connections.push_back(c2);

    auto c3 = connect(p_instrument,
                      &SymbolContext::receivedNewTrade,
                      instance->p_adapter,
                      &StrategyCallbackAdapter::onTrade,
                      Qt::QueuedConnection);
    ASSUME_TRUE(c3);
    instance->m_connections.push_back(c3);

    qInfo(StrategyManagerLog) << "Connected symbol" << p_symbol << "to strategy" << p_strategyID;
}

StrategyLogger* StrategyManager::getStrategyLogger(const QString& p_strategyID)
{
    auto instance = findStrategy(p_strategyID);
    return instance && instance->p_logger ? instance->p_logger.get() : nullptr;
}

const StrategyLogger* StrategyManager::getStrategyLogger(const QString& p_strategyID) const
{
    auto instance = findStrategy(p_strategyID);
    return instance && instance->p_logger ? instance->p_logger.get() : nullptr;
}

void StrategyManager::processClaimSymbols(const QString& p_strategyID,
                                          const QStringList& p_symbols,
                                          std::shared_ptr<QPromise<QStringList>> p_promise)
{
    QStringList approved;

    for (const QString& symbol: p_symbols)
    {
        if (m_symbolRegistry.contains(symbol))
        {
            const QString& owner = m_symbolRegistry[symbol];
            if (owner != p_strategyID)
            {
                qWarning(StrategyManagerLog) << "Symbol claim denied:" << symbol << "already owned by" << owner
                                             << "(requested by" << p_strategyID << ")";
                continue; // excluded from approved list
            }
            // Same strategy re-claiming — allow
        }
        m_symbolRegistry[symbol] = p_strategyID;
        approved.append(symbol);

        // Subscribe data feeds for this symbol
        // Reuse existing processSubscribeToSymbol machinery (with a discarded bool promise)
        auto boolPromise = std::make_shared<QPromise<bool>>();
        boolPromise->start();
        m_mainAlgo->processSubscribeToSymbol(p_strategyID, symbol, boolPromise);
    }

    // Update SDK's claimed symbols on the strategy thread
    auto* instance = findStrategy(p_strategyID);
    if (instance && instance->p_sdk)
    {
        QMetaObject::invokeMethod(
            instance->p_sdk,
            [sdk = instance->p_sdk, approved]() { sdk->setClaimedSymbols(approved); },
            Qt::QueuedConnection);
    }

    // Update monitored symbols list for StrategyQuickView
    if (instance)
    {
        instance->monitoredSymbols = QVector<QString>(approved.begin(), approved.end());
    }

    qInfo(StrategyManagerLog) << "Strategy" << p_strategyID << "claimed symbols:" << approved;

    emit symbolsClaimed(p_strategyID, approved);

    p_promise->addResult(approved);
    p_promise->finish();
}

void StrategyManager::releaseSymbols(const QString& p_strategyID)
{
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
    if (instance && instance->p_sdk)
    {
        QMetaObject::invokeMethod(
            instance->p_sdk,
            [sdk = instance->p_sdk]() { sdk->clearClaimedSymbols(); },
            Qt::QueuedConnection);
    }
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
        strategiesStateSettings->setValue("wasRunning", instance->state == StrategyState::RUNNING);
    }
    strategiesStateSettings->endArray();
    strategiesStateSettings->sync();
}

void StrategyManager::restoreStrategiesState()
{
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
            QString error = startStrategy(result.value());
            if (!error.isEmpty())
            {
                qWarning(StrategyManagerLog)
                    << "Failed to auto-start restored strategy:" << config.name << "-" << error;
            }
        }
    }
    strategiesStateSettings->endArray();
}
