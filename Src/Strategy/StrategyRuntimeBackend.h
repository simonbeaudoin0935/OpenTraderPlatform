#pragma once

#include <QThread>
#include <QString>
#include <QStringList>
#include <expected>
#include <functional>
#include <memory>

class StrategySDK;
class StrategyLogger;
class Bar;
struct Level2;
struct Trade;
class Order;
class Position;
struct StrategyConfig;
struct MarketDataSubscriptionError;

enum class StrategyManualOrderDecision : quint8
{
    Accepted,
    Rejected,
    TimedOut,
    Cancelled
};

class IStrategyRuntimeBackend
{
  public:
    virtual ~IStrategyRuntimeBackend() = default;

    [[nodiscard]] virtual StrategySDK* sdk() const = 0;
    [[nodiscard]] virtual StrategyLogger* logger() const = 0;
    [[nodiscard]] virtual Qt::HANDLE threadHandle() const = 0;
    virtual void setThreadHandle(Qt::HANDLE p_handle) = 0;
    [[nodiscard]] virtual bool isThreadRunning() const = 0;
    [[nodiscard]] virtual QString start(const std::function<void()>& p_onStarted) = 0;
    [[nodiscard]] virtual QString pause(const QString& p_reason) = 0;
    [[nodiscard]] virtual QString resume() = 0;
    [[nodiscard]] virtual QString updateConfig(const StrategyConfig& p_config) = 0;
    virtual void trackMonitoredSymbol(const QString& p_symbol) = 0;
    virtual void publishBar(const QString& p_symbol, const Bar& p_bar) = 0;
    virtual void publishLevel2(const QString& p_symbol, const Level2& p_level2) = 0;
    virtual void publishTrade(const QString& p_symbol, const Trade& p_trade) = 0;
    virtual void publishSubscriptionError(const MarketDataSubscriptionError& p_error) = 0;
    virtual void publishOrder(const Order& p_order) = 0;
    virtual void publishClaimedSymbols(const QStringList& p_claimedSymbols) = 0;
    virtual void publishManualOrderDecision(const QString& p_requestID,
                                            const QString& p_symbol,
                                            StrategyManualOrderDecision p_decision,
                                            const QString& p_reason) = 0;
    virtual void publishPosition(const Position& p_position) = 0;
    virtual void publishBalance(double p_balance) = 0;
    virtual void invokeOnStop() = 0;
    virtual void shutdownExecutionThread() = 0;
    virtual void destroyRuntime() = 0;
};
