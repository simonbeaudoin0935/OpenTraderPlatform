#include "StrategyManager.h"
#include "StrategySDK.h"
#include "../Algo/MainAlgo.h"
#include <QUuid>
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(StrategyManagerLog)
Q_LOGGING_CATEGORY(StrategyManagerLog, "StrategyManager", QtWarningMsg)

// StrategySDK implementation
StrategySDK::StrategySDK(MainAlgo* p_mainAlgo, const QString& p_strategyID, const StrategyConfig& p_config)
    : QObject(nullptr), m_mainAlgo(p_mainAlgo), m_strategyID(p_strategyID), m_config(p_config)
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
    Q_UNUSED(p_level);
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

    auto p_sdk = new StrategySDK(m_mainAlgo, strategyID, p_config);

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

    // Create strategy logger
    auto p_logger = std::make_unique<StrategyLogger>(p_config.name);

    auto instance = new StrategyInstance();
    instance->strategyID = strategyID;
    instance->config = p_config;
    instance->plugin = plugin;
    instance->p_strategy = p_strategy;
    instance->p_sdk = p_sdk;
    instance->p_adapter = p_adapter;
    instance->p_logger = std::move(p_logger);
    instance->monitoredSymbols = p_config.symbols;

    p_sdk->moveToThread(&instance->m_thread);
    p_adapter->moveToThread(&instance->m_thread);

    connectStrategyToDataSources(instance);

    // Connect thread started signal to install signal handler and call onStart
    QObject::connect(&instance->m_thread,
                     &QThread::started,
                     [this, instance]()
                     {
                         // Install SIGSEGV handler for this strategy thread
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
                     });

    instance->m_thread.start();

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

    // Call onStop after thread has stopped
    if (instance->p_strategy)
    {
        instance->p_strategy->onStop();
        qInfo(StrategyManagerLog) << "Called onStop for strategy:" << instance->config.name;
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
        return instance->m_thread.isRunning();
    }
    return false;
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
