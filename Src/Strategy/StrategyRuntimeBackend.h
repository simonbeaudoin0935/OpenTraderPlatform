#pragma once

#include <QThread>
#include <QString>
#include <functional>
#include <memory>
#include <expected>

#include "StrategyLoader.h"

class MainAlgo;
class StrategyBase;
class StrategySDK;
class StrategyLogger;
class StrategyCallbackAdapter;
class Bar;
struct Level2;
struct Trade;
class Order;
class Position;
struct StrategyConfig;

class IStrategyRuntimeBackend
{
  public:
    virtual ~IStrategyRuntimeBackend() = default;

    [[nodiscard]] virtual StrategyBase* strategy() const = 0;
    [[nodiscard]] virtual StrategySDK* sdk() const = 0;
    [[nodiscard]] virtual StrategyCallbackAdapter* adapter() const = 0;
    [[nodiscard]] virtual StrategyLogger* logger() const = 0;
    [[nodiscard]] virtual QThread* executionThread() = 0;
    [[nodiscard]] virtual Qt::HANDLE threadHandle() const = 0;
    virtual void setThreadHandle(Qt::HANDLE p_handle) = 0;
    [[nodiscard]] virtual bool isThreadRunning() const = 0;
    [[nodiscard]] virtual QString start(const std::function<void()>& p_onStarted) = 0;
    virtual void trackMonitoredSymbol(const QString& p_symbol) = 0;
    virtual void publishBar(const QString& p_symbol, const Bar& p_bar) = 0;
    virtual void publishLevel2(const QString& p_symbol, const Level2& p_level2) = 0;
    virtual void publishTrade(const QString& p_symbol, const Trade& p_trade) = 0;
    virtual void publishOrder(const Order& p_order) = 0;
    virtual void publishPosition(const Position& p_position) = 0;
    virtual void publishBalance(double p_balance) = 0;
    virtual void invokeOnStop() = 0;
    virtual void shutdownExecutionThread() = 0;
    virtual void destroyRuntime() = 0;
};

class PluginStrategyRuntimeBackend final : public IStrategyRuntimeBackend
{
  public:
    [[nodiscard]] static std::expected<std::unique_ptr<PluginStrategyRuntimeBackend>, QString>
    create(MainAlgo* p_mainAlgo, const QString& p_strategyID, const StrategyConfig& p_config);

    ~PluginStrategyRuntimeBackend() override;

    [[nodiscard]] StrategyBase* strategy() const override;
    [[nodiscard]] StrategySDK* sdk() const override;
    [[nodiscard]] StrategyCallbackAdapter* adapter() const override;
    [[nodiscard]] StrategyLogger* logger() const override;
    [[nodiscard]] QThread* executionThread() override;
    [[nodiscard]] Qt::HANDLE threadHandle() const override;
    void setThreadHandle(Qt::HANDLE p_handle) override;
    [[nodiscard]] bool isThreadRunning() const override;
    [[nodiscard]] QString start(const std::function<void()>& p_onStarted) override;
    void trackMonitoredSymbol(const QString& p_symbol) override;
    void publishBar(const QString& p_symbol, const Bar& p_bar) override;
    void publishLevel2(const QString& p_symbol, const Level2& p_level2) override;
    void publishTrade(const QString& p_symbol, const Trade& p_trade) override;
    void publishOrder(const Order& p_order) override;
    void publishPosition(const Position& p_position) override;
    void publishBalance(double p_balance) override;
    void invokeOnStop() override;
    void shutdownExecutionThread() override;
    void destroyRuntime() override;

  private:
    PluginStrategyRuntimeBackend(QString p_strategyID,
                                 StrategyLoader::LoadedPlugin p_plugin,
                                 StrategyBase* p_strategy,
                                 StrategySDK* p_sdk,
                                 StrategyCallbackAdapter* p_adapter,
                                 std::unique_ptr<StrategyLogger>&& p_logger);

    QString m_strategyID;
    StrategyLoader::LoadedPlugin m_plugin;
    StrategyBase* m_strategy = nullptr;
    StrategySDK* m_sdk = nullptr;
    StrategyCallbackAdapter* m_adapter = nullptr;
    QThread m_thread;
    std::unique_ptr<StrategyLogger> m_logger;
    Qt::HANDLE m_threadHandle = nullptr;
    bool m_runtimeDestroyed = false;
};
