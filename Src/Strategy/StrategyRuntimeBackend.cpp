#include "StrategyRuntimeBackend.h"

#include <QDebug>
#include <QObject>

#include "../Algo/MainAlgo.h"
#include "StrategyConfig.h"
#include "StrategyLogger.h"
#include "StrategyManager.h"
#include "StrategySDK.h"
#include "StrategySignalHandler.h"

Q_DECLARE_LOGGING_CATEGORY(StrategyManagerLog)

std::expected<std::unique_ptr<PluginStrategyRuntimeBackend>, QString>
PluginStrategyRuntimeBackend::create(MainAlgo* p_mainAlgo, const QString& p_strategyID, const StrategyConfig& p_config)
{
    auto pluginResult = StrategyLoader::loadPlugin(p_config.soPath);
    if (!pluginResult)
    {
        return std::unexpected(pluginResult.error());
    }

    auto plugin = pluginResult.value();
    auto p_logger = std::make_unique<StrategyLogger>(p_config.name);
    auto p_sdk = new StrategySDK(p_mainAlgo, p_strategyID, p_config, p_logger.get());

    StrategyBase* p_strategy = plugin.createFn(p_config, p_sdk);
    if (!p_strategy)
    {
        delete p_sdk;
        StrategyLoader::unloadPlugin(plugin);
        return std::unexpected("Failed to create strategy instance for: " + p_config.name);
    }

    p_strategy->setSdk(p_sdk);

    auto p_adapter = new StrategyCallbackAdapter(p_strategy, p_sdk, p_config.symbols);

    auto backend = std::unique_ptr<PluginStrategyRuntimeBackend>(
        new PluginStrategyRuntimeBackend(p_strategyID, plugin, p_strategy, p_sdk, p_adapter, std::move(p_logger)));

    backend->m_thread.setObjectName(QString("Strategy_%1_%2").arg(p_config.name).arg(p_strategyID.left(8)));

    p_sdk->moveToThread(&backend->m_thread);
    p_adapter->moveToThread(&backend->m_thread);

    if (auto* qobj = dynamic_cast<QObject*>(p_strategy))
    {
        qobj->moveToThread(&backend->m_thread);
    }

    return backend;
}

PluginStrategyRuntimeBackend::PluginStrategyRuntimeBackend(QString p_strategyID,
                                                           StrategyLoader::LoadedPlugin p_plugin,
                                                           StrategyBase* p_strategy,
                                                           StrategySDK* p_sdk,
                                                           StrategyCallbackAdapter* p_adapter,
                                                           std::unique_ptr<StrategyLogger>&& p_logger)
    : m_strategyID(std::move(p_strategyID))
    , m_plugin(std::move(p_plugin))
    , m_strategy(p_strategy)
    , m_sdk(p_sdk)
    , m_adapter(p_adapter)
    , m_logger(std::move(p_logger))
{
}

PluginStrategyRuntimeBackend::~PluginStrategyRuntimeBackend()
{
    destroyRuntime();
}

StrategyBase* PluginStrategyRuntimeBackend::strategy() const
{
    return m_strategy;
}

StrategySDK* PluginStrategyRuntimeBackend::sdk() const
{
    return m_sdk;
}

StrategyCallbackAdapter* PluginStrategyRuntimeBackend::adapter() const
{
    return m_adapter;
}

StrategyLogger* PluginStrategyRuntimeBackend::logger() const
{
    return m_logger.get();
}

QThread* PluginStrategyRuntimeBackend::executionThread()
{
    return &m_thread;
}

Qt::HANDLE PluginStrategyRuntimeBackend::threadHandle() const
{
    return m_threadHandle;
}

void PluginStrategyRuntimeBackend::setThreadHandle(Qt::HANDLE p_handle)
{
    m_threadHandle = p_handle;
}

bool PluginStrategyRuntimeBackend::isThreadRunning() const
{
    return m_thread.isRunning();
}

QString PluginStrategyRuntimeBackend::start(const std::function<void()>& p_onStarted)
{
    if (m_thread.isRunning())
    {
        return "Strategy runtime backend is already running";
    }

    QObject::connect(
        &m_thread,
        &QThread::started,
        m_adapter,
        [this, p_onStarted]()
        {
            setThreadHandle(QThread::currentThreadId());

            if (!StrategySignalHandler::installSignalHandler(m_strategyID))
            {
                qWarning(StrategyManagerLog) << "Failed to install signal handler for strategy:" << m_strategyID;
            }

            if (m_strategy)
            {
                m_strategy->onStart(m_sdk);
            }

            if (p_onStarted)
            {
                p_onStarted();
            }
        },
        Qt::SingleShotConnection);

    m_thread.start();
    return "";
}

void PluginStrategyRuntimeBackend::invokeOnStop()
{
    if (m_adapter == nullptr)
    {
        return;
    }

    if (m_thread.isRunning())
    {
        QMetaObject::invokeMethod(m_adapter, "callOnStop", Qt::BlockingQueuedConnection);
        return;
    }

    m_adapter->callOnStop();
}

void PluginStrategyRuntimeBackend::shutdownExecutionThread()
{
    if (!m_thread.isRunning())
    {
        return;
    }

    m_thread.quit();
    if (!m_thread.wait(5000))
    {
        qWarning(StrategyManagerLog) << "Strategy thread did not finish within timeout, terminating:" << m_strategyID;
        m_thread.terminate();
        m_thread.wait();
    }
}

void PluginStrategyRuntimeBackend::destroyRuntime()
{
    if (m_runtimeDestroyed)
    {
        return;
    }
    m_runtimeDestroyed = true;

    shutdownExecutionThread();

    if (m_adapter)
    {
        delete m_adapter;
        m_adapter = nullptr;
    }

    if (m_plugin.destroyFn && m_strategy)
    {
        m_plugin.destroyFn(m_strategy);
        m_strategy = nullptr;
    }

    if (m_sdk)
    {
        delete m_sdk;
        m_sdk = nullptr;
    }

    StrategyLoader::unloadPlugin(m_plugin);
}
