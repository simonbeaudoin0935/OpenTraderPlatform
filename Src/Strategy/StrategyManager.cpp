#include "StrategyManager.h"
#include "StrategySDK.h"
#include "../Algo/MainAlgo.h"
#include <QUuid>
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(StrategyManagerLog)
Q_LOGGING_CATEGORY(StrategyManagerLog, "StrategyManager", QtWarningMsg)

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
    if (!m_mainAlgo)
    {
        return QtFuture::makeReadyFuture(
            std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::Other)));
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
    Q_UNUSED(p_orderID);
    return QtFuture::makeReadyFuture(
        std::expected<CancelOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::Other)));
}

QVector<Position> StrategySDK::getPositions() const
{
    return QVector<Position>();
}

QVector<Order> StrategySDK::getOrders() const
{
    return QVector<Order>();
}

double StrategySDK::getAccountBalance() const
{
    return 0.0;
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

const QString& StrategySDK::getStrategyName() const
{
    return m_config.name;
}

std::shared_ptr<QVector<Bar>>
StrategySDK::getHistoricalBars(const QString& /* symbol */, const QDate& day, const QTime& first, const QTime& last)
{
    if (!m_mainAlgo)
    {
        return std::make_shared<QVector<Bar>>();
    }

    // Request bars from MainAlgo (which has access to all StockInstruments and their BarCaches)
    auto result = m_mainAlgo->requestMissingBarsDisplayedStock(day, first, last);

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
    // Return current time in America/New_York timezone (market timezone)
    return QDateTime::currentDateTime().toTimeZone(QTimeZone("America/New_York"));
}

// StrategyManager implementation

StrategyManager::StrategyManager(MainAlgo* p_mainAlgo)
    : QObject(nullptr), m_mainAlgo(p_mainAlgo), m_registry(std::make_unique<StrategyRegistry>())
{
    if (!m_mainAlgo)
    {
        qWarning(StrategyManagerLog) << "StrategyManager created with null MainAlgo";
    }
}

StrategyManager::~StrategyManager()
{
    qInfo(StrategyManagerLog) << "StrategyManager shutdown: unloading" << m_strategies.size() << "active strategies";

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
    if (!m_mainAlgo)
    {
        return std::unexpected("StrategyManager not initialized with MainAlgo");
    }

    if (p_config.soPath.isEmpty())
    {
        return std::unexpected("Strategy .so path is empty");
    }

    auto pluginResult = StrategyLoader::loadPlugin(p_config.soPath);
    if (!pluginResult)
    {
        return std::unexpected("Failed to load strategy plugin: " + p_config.soPath);
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
    auto p_adapter = new StrategyCallbackAdapter(p_strategy, p_config.symbols);

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

    disconnectStrategyFromDataSources(instance);

    // Call onStop on the strategy thread before quitting
    if (instance->p_adapter)
    {
        QMetaObject::invokeMethod(instance->p_adapter, "callOnStop", Qt::BlockingQueuedConnection);
        qInfo(StrategyManagerLog) << "Called onStop for strategy:" << instance->config.name;
    }

    // Save logs before shutdown
    if (instance->p_logger)
    {
        QString logFile = instance->p_logger->saveToFile();
        qInfo(StrategyManagerLog) << "Saved strategy logs to:" << logFile;
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
                         if (!StrategySignalHandler::installSignalHandler(instance->strategyID, this))
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

void StrategyManager::onMarketDepthReceived(const QString& p_symbol, const MarketDepthQuote& p_quote)
{
    for (auto* instance: m_strategies)
    {
        if (instance && instance->monitoredSymbols.contains(p_symbol))
        {
            if (instance->p_strategy)
            {
                instance->p_strategy->onMarketDepth(p_quote);
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
    // Broadcast to all strategies for now
    // TODO: Later, route only to the strategy that placed the order
    for (auto* instance: m_strategies)
    {
        if (instance && instance->p_strategy)
        {
            instance->p_strategy->onOrderUpdated(p_order);
        }
    }
}

void StrategyManager::onMainAlgoOrderUpdated(const QString& p_account, const Order& p_order)
{
    Q_UNUSED(p_account);
    onOrderUpdated(p_order);
}

void StrategyManager::connectStrategyToDataSources(StrategyInstance* p_instance)
{
    if (!p_instance || !m_mainAlgo || !p_instance->p_adapter)
    {
        return;
    }

    // Connect bar data from MainAlgo to strategy adapter
    // MainAlgo emits displayedStockReceivedNewBar on MainAlgo thread
    // -> Connected to adapter->onBar with Qt::QueuedConnection
    // -> Adapter slot executes on strategy thread
    // -> Calls strategy->onBar() on strategy thread (thread-safe)
    connect(m_mainAlgo,
            &MainAlgo::displayedStockReceivedNewBar,
            p_instance->p_adapter,
            &StrategyCallbackAdapter::onBar,
            Qt::QueuedConnection);

    qInfo(StrategyManagerLog) << "Connected strategy to data sources:" << p_instance->strategyID;
}

void StrategyManager::disconnectStrategyFromDataSources(StrategyInstance* p_instance)
{
    if (!p_instance || !p_instance->p_adapter)
    {
        return;
    }

    // Disconnect all signals connected to this strategy's adapter
    QObject::disconnect(m_mainAlgo, nullptr, p_instance->p_adapter, nullptr);

    qInfo(StrategyManagerLog) << "Disconnected strategy from data sources:" << p_instance->strategyID;
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
