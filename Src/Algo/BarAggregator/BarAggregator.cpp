#include "BarAggregator.h"

#include "Logging.h"
#include "BarUtils.h"

#define LOGGING_CATEGORY BarAggregatorLog
Q_LOGGING_CATEGORY(BarAggregatorLog, "BarAggregator")

// All timescales we accumulate into from 1m live bars.
// Order matters: lower TFs first so barClosed cascades can be logged in order.
const QList<TimeFrame> BarAggregator::AGGREGATED_TIMEFRAMES = {TimeFrame::FIVE_MINUTES,
                                                               TimeFrame::FIFTEEN_MINUTES,
                                                               TimeFrame::THIRTY_MINUTES,
                                                               TimeFrame::ONE_HOUR,
                                                               TimeFrame::FOUR_HOURS,
                                                               TimeFrame::ONE_DAY,
                                                               TimeFrame::ONE_WEEK,
                                                               TimeFrame::ONE_MONTH};

BarAggregator::BarAggregator(QObject* parent) : QObject(parent)
{
    for (TimeFrame tf: AGGREGATED_TIMEFRAMES)
    {
        m_accum[tf] = AccumulatingBar{};
    }
}

bool BarAggregator::isBarClosing(TimeFrame tf, const QDateTime& barTs) const
{
    const QTime time = barTs.time();

    if (tf == TimeFrame::ONE_WEEK)
    {
        // A weekly bar closes when the day is Friday and the 1m bar is the last of the day
        return barTs.date().dayOfWeek() == Qt::Friday && time == TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;
    }

    if (tf == TimeFrame::ONE_MONTH)
    {
        // A monthly bar closes on the last trading day of the calendar month
        // We check: the next calendar day is in a different month (weekend-aware)
        QDate nextTradingDay = barTs.date().addDays(1);
        while (nextTradingDay.dayOfWeek() > Qt::Friday)
            nextTradingDay = nextTradingDay.addDays(1);
        const bool isLastDayOfMonth = (nextTradingDay.month() != barTs.date().month());
        return isLastDayOfMonth && time == TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;
    }

    if (tf == TimeFrame::ONE_DAY)
    {
        return time == TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;
    }

    // Intraday: bar closes when (minutesFromOpen + 1) is a multiple of tf's interval
    const int minutesFromOpen = (time.hour() - 4) * 60 + time.minute();
    const int tfMinutes = BarUtils::minutesPerBar(tf);
    return (minutesFromOpen + 1) % tfMinutes == 0;
}

void BarAggregator::onNewBar(const QString& /*symbol*/, const Bar& bar)
{
    if (bar.getBarStatus() == Bar::BarStatus::Null || bar.getBarStatus() == Bar::BarStatus::Uninitialized)
        return;

    const bool isClosed = (bar.getBarStatus() == Bar::BarStatus::Closed);
    const QDateTime& ts = bar.getTimeStamp();

    for (TimeFrame tf: AGGREGATED_TIMEFRAMES)
    {
        AccumulatingBar& acc = m_accum[tf];

        if (!acc.active)
        {
            acc.start(bar);
        }
        else
        {
            acc.update(bar);
        }

        // Always emit barUpdated so the chart can show the live candle
        emit barUpdated(tf, acc.toBar());

        // If this 1m bar closes the TF period, emit barClosed and reset
        if (isClosed && isBarClosing(tf, ts))
        {
            Bar closedBar = acc.toBar();
            acc.reset();
            DEBUG << "Bar closed for tf=" << static_cast<int>(tf) << "at" << closedBar.getTimeStamp();
            emit barClosed(tf, closedBar);
        }
    }
}
