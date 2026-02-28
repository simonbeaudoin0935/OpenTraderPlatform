#include "LiveBarAccumulator.h"

#include <algorithm>

#include "CONSTANTS.h"
#include "Logging.h"

#define LOGGING_CATEGORY LiveBarAccumulatorLog

Q_LOGGING_CATEGORY(LiveBarAccumulatorLog, "LiveBarAccumulator")

LiveBarAccumulator::LiveBarAccumulator(QObject* p_parent) : QObject(p_parent) {}

void LiveBarAccumulator::onNewTrade(const QString& p_symbol, const Trade& p_trade)
{
    const float price = static_cast<float>(p_trade.m_price);
    const quint64 size = static_cast<quint64>(p_trade.m_size);
    const QDateTime closeTime = barCloseTimeForTrade(p_trade.m_timestamp);

    auto it = m_formingBars.find(p_symbol);

    if (it == m_formingBars.end())
    {
        // First trade ever for this symbol — start a new forming bar
        FormingBar forming;
        forming.barCloseTime = closeTime;
        forming.open = price;
        forming.high = price;
        forming.low = price;
        forming.close = price;
        forming.volume = size;

        m_formingBars.insert(p_symbol, forming);
        emit barUpdated(p_symbol, toBar(forming, Bar::BarStatus::Open));
        return;
    }

    FormingBar& forming = it.value();

    if (closeTime != forming.barCloseTime)
    {
        // Minute boundary crossed — close the current bar
        Bar closedBar = toBar(forming, Bar::BarStatus::Closed);
        emit barClosed(p_symbol, closedBar);

        // Start new forming bar
        forming.barCloseTime = closeTime;
        forming.open = price;
        forming.high = price;
        forming.low = price;
        forming.close = price;
        forming.volume = size;

        emit barUpdated(p_symbol, toBar(forming, Bar::BarStatus::Open));
        return;
    }

    // Same minute — update the forming bar
    forming.high = std::max(forming.high, price);
    forming.low = std::min(forming.low, price);
    forming.close = price;
    forming.volume += size;

    emit barUpdated(p_symbol, toBar(forming, Bar::BarStatus::Open));
}

QDateTime LiveBarAccumulator::barCloseTimeForTrade(const QDateTime& p_tradeTime)
{
    // Ceiling to next whole minute:
    //   trade at 09:31:04 → bar close 09:32:00
    //   trade at 09:31:00.000 → bar close 09:32:00 (trade at exact minute boundary → next minute's bar)
    const QDateTime utc = p_tradeTime.toUTC();
    const qint64 epochSecs = utc.toSecsSinceEpoch();
    const qint64 minuteFloor = (epochSecs / 60) * 60;
    const qint64 ceilMinute = minuteFloor + 60;

    return QDateTime::fromSecsSinceEpoch(ceilMinute, TradingHours::MARKET_TIMEZONE);
}

Bar LiveBarAccumulator::toBar(const FormingBar& p_forming, Bar::BarStatus p_status)
{
    return Bar(p_forming.barCloseTime,
               p_forming.open,
               p_forming.high,
               p_forming.low,
               p_forming.close,
               p_forming.volume,
               p_status);
}
