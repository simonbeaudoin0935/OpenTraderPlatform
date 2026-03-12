#include "LiveBarAccumulator.h"

#include <algorithm>

#include "CONSTANTS.h"
#include "Logging.h"
#include "LTTng/LTTngTracepoints.h"

#define LOGGING_CATEGORY LiveBarAccumulatorLog

Q_LOGGING_CATEGORY(LiveBarAccumulatorLog, "LiveBarAccumulator")

LiveBarAccumulator::LiveBarAccumulator(QObject* p_parent, int p_intervalSeconds)
    : QObject(p_parent), m_intervalSeconds(p_intervalSeconds)
{
}

void LiveBarAccumulator::onNewTrade(const QString& p_symbol, const Trade& p_trade)
{
    const float price = static_cast<float>(p_trade.m_price);
    const quint64 size = static_cast<quint64>(p_trade.m_size);
    const QDateTime openTime = barOpenTimeForTrade(p_trade.m_timestamp);

    auto it = m_formingBars.find(p_symbol);

    if (it == m_formingBars.end())
    {
        // First trade ever for this symbol — start a new forming bar
        DEBUG << "First trade for" << p_symbol << "at" << openTime.toString("hh:mm:ss");
        FormingBar forming;
        forming.barOpenTime = openTime;
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

    if (openTime != forming.barOpenTime)
    {
        // Interval boundary crossed — close the current bar
        DEBUG << "Interval boundary (" << m_intervalSeconds << "s): closing bar at"
              << forming.barOpenTime.toString("hh:mm:ss") << "starting new at" << openTime.toString("hh:mm:ss");
        Bar closedBar = toBar(forming, Bar::BarStatus::Closed);
        emit barClosed(p_symbol, closedBar);

        L2T_TP(l2trader, livebar_closed, p_symbol.toUtf8().constData(), m_intervalSeconds);

        // Start new forming bar
        forming.barOpenTime = openTime;
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

QDateTime LiveBarAccumulator::barOpenTimeForTrade(const QDateTime& p_tradeTime) const
{
    // Floor to current interval boundary (open-time convention):
    //   For 60s: trade at 09:31:04 → bar open 09:31:00
    //   For 10s: trade at 09:31:14 → bar open 09:31:10
    const QDateTime utc = p_tradeTime.toUTC();
    const qint64 epochSecs = utc.toSecsSinceEpoch();
    const qint64 intervalFloor = (epochSecs / m_intervalSeconds) * m_intervalSeconds;

    return QDateTime::fromSecsSinceEpoch(intervalFloor, TradingHours::MARKET_TIMEZONE);
}

Bar LiveBarAccumulator::toBar(const FormingBar& p_forming, Bar::BarStatus p_status)
{
    return Bar(p_forming.barOpenTime,
               p_forming.open,
               p_forming.high,
               p_forming.low,
               p_forming.close,
               p_forming.volume,
               p_status);
}
