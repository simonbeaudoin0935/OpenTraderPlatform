#include "HistoricalBarsStrategy.h"
#include "Strategy/StrategySDK.h"
#include <QDateTime>
#include <QTimeZone>

HistoricalBarsStrategy::HistoricalBarsStrategy(const StrategyConfig& p_config, StrategySDK* p_sdk)
    : m_config(p_config), m_sdk(nullptr), m_timer(nullptr), m_daysBack(0)
{
    Q_UNUSED(p_sdk);
}

HistoricalBarsStrategy::~HistoricalBarsStrategy()
{
    onStop();
}

void HistoricalBarsStrategy::onStart(StrategySDK* sdk)
{
    if (!sdk)
    {
        return;
    }

    m_sdk = sdk;
    m_daysBack = 0;

    // Get today's date in America/New_York timezone
    QDateTime nowNY = m_sdk->getCurrentTime();
    QDate today = nowNY.date();

    // Start with the previous trading day (skip weekends: Saturday=6, Sunday=7 in Qt)
    // Qt::DayOfWeek: Monday=1, ..., Friday=5, Saturday=6, Sunday=7
    m_currentDate = today.addDays(-1);

    // Skip weekends: if we land on Saturday (6), go back to Friday
    // If we land on Sunday (7), go back to Friday
    int dayOfWeek = m_currentDate.dayOfWeek();
    if (dayOfWeek == Qt::Saturday)
    {
        m_currentDate = m_currentDate.addDays(-1); // Saturday -> Friday
    }
    else if (dayOfWeek == Qt::Sunday)
    {
        m_currentDate = m_currentDate.addDays(-2); // Sunday -> Friday
    }

    const QString& symbol = m_sdk->getConfig().symbols.isEmpty() ? "UNKNOWN" : m_sdk->getConfig().symbols[0];

    m_sdk->log(QString("HistoricalBarsStrategy started for symbol: %1, first fetch date: %2")
                   .arg(symbol, m_currentDate.toString("yyyy-MM-dd")),
               LogLevel::Info);

    // Create timer in this thread (strategy thread)
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &HistoricalBarsStrategy::fetchNextDay);

    // Fetch first day immediately
    fetchNextDay();
}

void HistoricalBarsStrategy::onStop()
{
    if (m_timer)
    {
        // Use invokeMethod to stop timer on the correct thread (strategy thread)
        QMetaObject::invokeMethod(m_timer, "stop", Qt::QueuedConnection);
        m_timer->deleteLater();
        m_timer = nullptr;
    }

    if (m_sdk)
    {
        m_sdk->log("HistoricalBarsStrategy stopped", LogLevel::Info);
        m_sdk = nullptr;
    }
}

void HistoricalBarsStrategy::fetchNextDay()
{
    if (!m_sdk)
    {
        if (m_timer)
        {
            m_timer->stop();
        }
        return;
    }

    const QString& symbol = m_sdk->getConfig().symbols.isEmpty() ? "AAPL" : m_sdk->getConfig().symbols[0];

    // Trading hours: 9:31 AM to 4:00 PM (390 bars total)
    // Note: Bar timestamps represent closing time, so 9:31 bar closes at 9:31 (covers 9:30-9:31 minute)
    // and 16:00 bar closes at 16:00 (covers 15:59-16:00 minute)
    QTime startTime(9, 31);
    QTime endTime(16, 0);

    m_sdk->log(QString("Fetching day %1 for %2 on %3")
                   .arg(m_daysBack + 1)
                   .arg(symbol)
                   .arg(m_currentDate.toString("yyyy-MM-dd")),
               LogLevel::Info);

    // Fetch bars synchronously (blocks until result is ready)
    auto bars = m_sdk->getHistoricalBars(symbol, m_currentDate, startTime, endTime);

    if (bars)
    {
        m_fetchedBars.append(bars);
        m_sdk->log(QString("Fetched %1 bars for %2 on %3 (total days stored: %4)")
                       .arg(bars->size())
                       .arg(symbol)
                       .arg(m_currentDate.toString("yyyy-MM-dd"))
                       .arg(m_fetchedBars.size()),
                   LogLevel::Info);
    }
    else
    {
        m_sdk->log(QString("Failed to fetch bars for %1 on %2").arg(symbol, m_currentDate.toString("yyyy-MM-dd")),
                   LogLevel::Warning);
    }

    m_daysBack++;
    m_currentDate = m_currentDate.addDays(-1);

    // Skip weekends (Saturday=6, Sunday=7)
    int dayOfWeek = m_currentDate.dayOfWeek();
    if (dayOfWeek == Qt::Saturday)
    {
        m_currentDate = m_currentDate.addDays(-1); // Saturday -> Friday
    }
    else if (dayOfWeek == Qt::Sunday)
    {
        m_currentDate = m_currentDate.addDays(-2); // Sunday -> Friday
    }

    // Schedule next fetch for 2 seconds later
    if (m_timer)
    {
        m_timer->start(2000);
    }
}

void HistoricalBarsStrategy::onBar(const Bar& p_bar)
{
    Q_UNUSED(p_bar);
}

void HistoricalBarsStrategy::onOrderUpdated(const Order& p_order)
{
    Q_UNUSED(p_order);
}

void HistoricalBarsStrategy::onOrderFilled(const Order& p_order)
{
    Q_UNUSED(p_order);
}

void HistoricalBarsStrategy::onOrderCancelled(const Order& p_order, const std::string& p_reason)
{
    Q_UNUSED(p_order);
    Q_UNUSED(p_reason);
}

void HistoricalBarsStrategy::onOrderRejected(const Order& p_order, const std::string& p_reason)
{
    Q_UNUSED(p_order);
    Q_UNUSED(p_reason);
}

void HistoricalBarsStrategy::onPositionUpdated(const Position& p_position)
{
    Q_UNUSED(p_position);
}

void HistoricalBarsStrategy::onBalanceUpdated(double p_balance)
{
    Q_UNUSED(p_balance);
}

void HistoricalBarsStrategy::onLevel2(const Level2& p_level2)
{
    Q_UNUSED(p_level2);
}

void HistoricalBarsStrategy::onError(const std::string& p_error)
{
    Q_UNUSED(p_error);
}

// Export plugin interface
extern "C"
{
    StrategyBase* createStrategy(const StrategyConfig& config, StrategySDK* sdk)
    {
        Q_UNUSED(config);
        Q_UNUSED(sdk);
        return new HistoricalBarsStrategy(config, sdk);
    }

    void destroyStrategy(StrategyBase* strategy)
    {
        delete strategy;
    }

    const char* getStrategyAPIVersion()
    {
        return "1.0.0";
    }
}
