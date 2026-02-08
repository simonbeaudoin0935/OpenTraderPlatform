#include "CrashTestStrategy.h"
#include "Strategy/StrategySDK.h"
#include <QTimer>

extern "C"
{
    // Factory function to create the strategy
    StrategyBase* createStrategy(const StrategyConfig& config, StrategySDK* sdk)
    {
        return new CrashTestStrategy(config, sdk);
    }

    // Factory function to destroy the strategy
    void destroyStrategy(StrategyBase* strategy)
    {
        delete strategy;
    }

    // API version export
    const char* getStrategyAPIVersion()
    {
        return "1.0.0";
    }
}

CrashTestStrategy::CrashTestStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk) : m_crashTimer(nullptr)
{
    Q_UNUSED(p_config);
    Q_UNUSED(p_sdk);
}

void CrashTestStrategy::onStart(StrategySDK* p_sdk)
{
    Q_UNUSED(p_sdk);
    log("CrashTestStrategy started");
    log("Strategy will crash in 5 seconds to test signal handling...");

    // Create timer without parent (we'll manage it manually)
    m_crashTimer = new QTimer();
    m_crashTimer->moveToThread(QThread::currentThread());

    // Connect timer to crash after 5 seconds
    connect(m_crashTimer, &QTimer::timeout, this, &CrashTestStrategy::triggerCrash);
    m_crashTimer->setSingleShot(true);
    m_crashTimer->start(5000); // 5 seconds
}

void CrashTestStrategy::onStop()
{
    if (m_crashTimer)
    {
        m_crashTimer->stop();
    }
}

void CrashTestStrategy::onPause()
{
    if (m_crashTimer)
    {
        m_crashTimer->stop();
    }
}

void CrashTestStrategy::triggerCrash()
{
    log("Triggering crash now!");

    // Intentionally dereference null pointer to trigger segfault
    int* p = nullptr;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
    *p = 42; // BOOM!
#pragma GCC diagnostic pop
}

void CrashTestStrategy::onBar(const Bar& p_bar)
{
    Q_UNUSED(p_bar);
}

void CrashTestStrategy::onMarketDepth(const MarketDepthQuote& p_quote)
{
    Q_UNUSED(p_quote);
}

void CrashTestStrategy::onOrderUpdated(const Order& p_order)
{
    Q_UNUSED(p_order);
}

void CrashTestStrategy::onOrderFilled(const Order& p_order)
{
    Q_UNUSED(p_order);
}

void CrashTestStrategy::onOrderCancelled(const Order& p_order, const std::string& p_reason)
{
    Q_UNUSED(p_order);
    Q_UNUSED(p_reason);
}

void CrashTestStrategy::onOrderRejected(const Order& p_order, const std::string& p_reason)
{
    Q_UNUSED(p_order);
    Q_UNUSED(p_reason);
}

void CrashTestStrategy::onPositionUpdated(const Position& p_position)
{
    Q_UNUSED(p_position);
}

void CrashTestStrategy::onBalanceUpdated(double p_balance)
{
    Q_UNUSED(p_balance);
}

void CrashTestStrategy::onError(const std::string& p_error)
{
    Q_UNUSED(p_error);
}
