#include "StrategyManager.h"
#include "../Algo/MainAlgo.h"
#include <QUuid>
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(StrategyManagerLog)
Q_LOGGING_CATEGORY(StrategyManagerLog, "StrategyManager", QtWarningMsg)

// StrategySDKImpl implementation
StrategySDKImpl::StrategySDKImpl(StrategyManager* p_manager) : mp_manager(p_manager) {}

QFuture<std::expected<PlaceOrderResult, TSClient::Error>> StrategySDKImpl::placeOrder(const PlaceOrderRequest& p_order)
{
    Q_UNUSED(p_order);
    return QtFuture::makeReadyFuture(
        std::expected<PlaceOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::Other)));
}

QFuture<std::expected<CancelOrderResult, TSClient::Error>> StrategySDKImpl::cancelOrder(const QString& p_orderID)
{
    Q_UNUSED(p_orderID);
    return QtFuture::makeReadyFuture(
        std::expected<CancelOrderResult, TSClient::Error>(std::unexpected(TSClient::Error::Other)));
}

QVector<Position> StrategySDKImpl::getPositions() const
{
    return QVector<Position>();
}

QVector<Order> StrategySDKImpl::getOrders() const
{
    return QVector<Order>();
}

double StrategySDKImpl::getAccountBalance() const
{
    return 0.0;
}

void StrategySDKImpl::log(const QString& p_message, LogLevel p_level)
{
    Q_UNUSED(p_level);
    qInfo(StrategyManagerLog) << "Strategy log:" << p_message;
}

const StrategyConfig& StrategySDKImpl::getConfig() const
{
    static const StrategyConfig dummy{};
    return dummy;
}

const QString& StrategySDKImpl::getStrategyName() const
{
    static const QString dummy;
    return dummy;
}

StrategyManager* StrategyManager::s_instance = nullptr;

StrategyManager::StrategyManager() : QObject(nullptr) {}

StrategyManager::~StrategyManager()
{
    QVector<QString> strategyIDs = getActiveStrategies();
    for (const auto& strategyID: strategyIDs)
    {
        auto error = unloadStrategy(strategyID);
        if (!error.isEmpty())
        {
            qWarning(StrategyManagerLog) << "Error unloading strategy:" << error;
        }
    }
}

StrategyManager* StrategyManager::getInstance()
{
    if (!s_instance)
    {
        s_instance = new StrategyManager();
    }
    return s_instance;
}

void StrategyManager::destroyInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

void StrategyManager::initialize(MainAlgo* p_mainAlgo)
{
    if (!p_mainAlgo)
    {
        qWarning(StrategyManagerLog) << "StrategyManager::initialize called with "
                                        "null MainAlgo";
        return;
    }
    mp_mainAlgo = p_mainAlgo;
    qInfo(StrategyManagerLog) << "StrategyManager initialized with MainAlgo";
}

std::expected<QString, QString> StrategyManager::loadStrategy(const StrategyConfig& p_config)
{
    if (!mp_mainAlgo)
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

    auto p_sdk = new StrategySDKImpl(this);

    Strategy* p_strategy = plugin.createFn(p_config, p_sdk);
    if (!p_strategy)
    {
        delete p_sdk;
        StrategyLoader::unloadPlugin(plugin);
        return std::unexpected("Failed to create strategy instance for: " + p_config.name);
    }

    auto instance = new StrategyInstance();
    instance->strategyID = strategyID;
    instance->config = p_config;
    instance->plugin = plugin;
    instance->p_strategy = p_strategy;
    instance->p_sdk = p_sdk;
    instance->monitoredSymbols = p_config.symbols;

    p_sdk->moveToThread(&instance->m_thread);

    connectStrategyToDataSources(instance);

    instance->m_thread.start();

    qInfo(StrategyManagerLog) << "Loaded strategy:" << p_config.name << "ID:" << strategyID;

    m_strategies[strategyID] = instance;

    return strategyID;
}

QString StrategyManager::unloadStrategy(const QString& p_strategyID)
{
    auto* instance = findStrategy(p_strategyID);
    if (!instance)
    {
        return "Strategy not found: " + p_strategyID;
    }

    qInfo(StrategyManagerLog) << "Unloading strategy:" << instance->config.name;

    disconnectStrategyFromDataSources(instance);

    instance->m_thread.quit();
    if (!instance->m_thread.wait(5000))
    {
        qWarning(StrategyManagerLog) << "Strategy thread did not finish, terminating";
        instance->m_thread.terminate();
        instance->m_thread.wait();
    }

    if (instance->plugin.destroyFn && instance->p_strategy)
    {
        instance->plugin.destroyFn(instance->p_strategy);
    }

    if (instance->p_sdk)
    {
        delete instance->p_sdk;
    }

    StrategyLoader::unloadPlugin(instance->plugin);

    delete m_strategies.take(p_strategyID);

    qInfo(StrategyManagerLog) << "Unloaded strategy:" << p_strategyID;
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

void StrategyManager::connectStrategyToDataSources(StrategyInstance* p_instance)
{
    if (!p_instance || !mp_mainAlgo)
    {
        return;
    }

    qInfo(StrategyManagerLog) << "Connected strategy to data sources:" << p_instance->strategyID;
}

void StrategyManager::disconnectStrategyFromDataSources(StrategyInstance* p_instance)
{
    if (!p_instance)
    {
        return;
    }

    qInfo(StrategyManagerLog) << "Disconnected strategy from data sources:" << p_instance->strategyID;
}
