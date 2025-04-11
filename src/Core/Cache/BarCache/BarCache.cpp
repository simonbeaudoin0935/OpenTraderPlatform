#include <QTimeZone>

#include "BarCache.h"
#include "TSClient.h"
#include "GetBars.h"

Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")

bool isEpochOutsideMarketHours(qint64 epochSeconds) {
    // Convert epoch seconds to QDateTime (UTC)
    QDateTime utcDateTime = QDateTime::fromSecsSinceEpoch(epochSeconds, Qt::UTC);
    // Convert to Eastern Time (ET) with DST awareness
    QTimeZone easternTime("America/New_York");
    QDateTime localDateTime = utcDateTime.toTimeZone(easternTime);

    // Get the day of the week (1 = Monday, 7 = Sunday)
    int dayOfWeek = localDateTime.date().dayOfWeek();

    // If weekend (Saturday or Sunday), it's outside market hours
    if (dayOfWeek == 6 || dayOfWeek == 7) {
        return true;
    }

    // Define market session times in ET (24-hour format)
    QTime preMarketStart(4, 0, 0);      // 4:00 AM ET
    QTime marketOpen(9, 30, 0);         // 9:30 AM ET
    QTime marketClose(16, 0, 0);        // 4:00 PM ET
    QTime afterMarketEnd(20, 0, 0);     // 8:00 PM ET

    // Get the local time of day
    QTime localTime = localDateTime.time();

    // Check if time falls within any session
    bool inPreMarket = (localTime >= preMarketStart && localTime < marketOpen);
    bool inRegularMarket = (localTime >= marketOpen && localTime <= marketClose);
    bool inAfterMarket = (localTime > marketClose && localTime <= afterMarketEnd);

    // Return true if outside all sessions, false if within any
    return !(inPreMarket || inRegularMarket || inAfterMarket);
}

BarCache::BarCache(const QString &symbol, bool isStreaming, QObject *parent):
    QObject(parent),
    symbol(symbol),
    isStreaming(isStreaming)
{
    connect(TSClient::getInstancePtr(), &TSClient::getBarsAsyncReceived, this, &BarCache::onGetBarsReceived);
}

const QVector<Bar> BarCache::getBars(QDateTime first, QDateTime last) {
    QVector<Bar> result;
    
    const int interval = 1;  // 1 minute bars
    const Bar::BarUnit unit = Bar::BarUnit::Minute;
    const int barsback = 0;
    const Bar::BarSessionTemplate sessionTemplate = Bar::BarSessionTemplate::USEQ24Hour;  // 24-hour session including pre/post market
    
    // For 24-hour session, we don't need to adjust times for market hours
    // but we should still handle weekends
    QDateTime adjustedFirst = first;
    QDateTime adjustedLast = last;
    
    // Skip to next trading day if we're in a weekend
    while (adjustedFirst.date().dayOfWeek() > 5) {  // Saturday or Sunday
        adjustedFirst = adjustedFirst.addDays(1);
        adjustedFirst.setTime(QTime(0, 0));  // Start at midnight
    }
    
    // Skip to previous trading day if we're in a weekend
    while (adjustedLast.date().dayOfWeek() > 5) {
        adjustedLast = adjustedLast.addDays(-1);
        adjustedLast.setTime(QTime(23, 59, 59));  // End at end of day
    }
    
    qCDebug(BarCacheLog) << "Requesting bars for" << symbol 
                         << "from" << adjustedFirst.toString() 
                         << "to" << adjustedLast.toString();
    
    // Check cache first
    QVector<Bar> cachedBars = getBarsFromCache(adjustedFirst, adjustedLast);
    
    if (!cachedBars.isEmpty()) {
        qCDebug(BarCacheLog) << "Cache HIT for" << symbol 
                            << "returning" << cachedBars.size() << "bars";
        return cachedBars;
    }
    
    qCDebug(BarCacheLog) << "Cache MISS for" << symbol 
                         << "fetching from API";
    
    // If not in cache, fetch from API
    QVector<Bar> fetchedBars;
    bool success = TSClient::getInstance().getBarsSync(
        fetchedBars,
        symbol,
        interval,
        unit,
        barsback,
        sessionTemplate,
        adjustedFirst,
        adjustedLast
    );
    
    if (success) {
        qCDebug(BarCacheLog) << "Successfully fetched" << fetchedBars.size() 
                            << "bars from API for" << symbol;
        // Store in cache
        storeBarsInCache(fetchedBars);
        return fetchedBars;
    } else {
        qCWarning(BarCacheLog) << "Failed to fetch bars from API for" << symbol 
                              << "between" << adjustedFirst << "and" << adjustedLast;
        return QVector<Bar>();
    }
}

void BarCache::storeBarsInCache(const QVector<Bar>& bars) {
    QWriteLocker locker(&rwLock);
    for (const Bar& bar : bars) {
        // Convert bar time to UTC for consistent storage
        QDateTime barTime = QDateTime::fromMSecsSinceEpoch(bar.getEpoch());
        barTime = barTime.toUTC();
        qint64 epoch = barTime.toMSecsSinceEpoch() * 1000;  // Convert to milliseconds
        
        if (barCacheOneMinute.contains(epoch)) {
            qCDebug(BarCacheLog) << "Bar already in cache for" << symbol 
                                << "at epoch" << epoch 
                                << "(" << QDateTime::fromMSecsSinceEpoch(epoch).toString() << ")";
        }
        barCacheOneMinute.insert(epoch, bar);
    }
}

QVector<Bar> BarCache::getBarsFromCache(QDateTime start, QDateTime end) const {
    QVector<Bar> result;
    QReadLocker locker(&rwLock);
    
    // Convert input times to UTC for consistent comparison
    start = start.toUTC();
    end = end.toUTC();
    
    qCDebug(BarCacheLog) << "Checking cache for" << symbol 
                         << "from" << start.toString() 
                         << "to" << end.toString();
    
    // Find the first bar at or after start
    auto it = barCacheOneMinute.lowerBound(start.toMSecsSinceEpoch() * 1000);  // Convert to milliseconds
    
    // Check if we have a complete set of bars
    bool hasCompleteSet = true;
    QDateTime expectedTime = start;
    
    while (expectedTime <= end) {
        qint64 expectedEpoch = expectedTime.toMSecsSinceEpoch() * 1000;  // Convert to milliseconds
        if (it == barCacheOneMinute.end() || it.key() != expectedEpoch) {
            hasCompleteSet = false;
            qCDebug(BarCacheLog) << "Cache MISS - missing bar at" << expectedTime.toString();
            break;
        }
        ++it;
        expectedTime = expectedTime.addSecs(60);  // Add one minute
    }
    
    if (hasCompleteSet) {
        qCDebug(BarCacheLog) << "Cache HIT - found complete set of bars for" << symbol;
        cacheHitCount++;  // Increment cache hit counter
        // We have all the bars, return them
        it = barCacheOneMinute.lowerBound(start.toMSecsSinceEpoch() * 1000);  // Convert to milliseconds
        while (it != barCacheOneMinute.end() && it.key() <= end.toMSecsSinceEpoch() * 1000) {  // Convert to milliseconds
            result.append(it.value());
            ++it;
        }
    } else {
        qCDebug(BarCacheLog) << "Cache MISS - incomplete set of bars for" << symbol;
        cacheMissCount++;  // Increment cache miss counter
    }
    
    return result;
}

void BarCache::addBar(const Bar &bar)
{
    if (barCacheOneMinute.contains(bar.getEpoch())) {
        qCWarning(BarCacheLog) << "Bar cache already contains a bar with epoch " << bar.getEpoch();
    } else {
        barCacheOneMinute[bar.getEpoch()] = bar;
    }
}

void BarCache::onGetBarsReceived(QString symbol, QVector<Bar> newBars)
{
    if (this->symbol != symbol) {
        qCWarning(BarCacheLog) << "Bars received for symbol " << symbol << " is not for this cache that is for " << this->symbol;
        return;
    }

    const QVector<Bar> &constNewBars = newBars;
    for (const Bar &newBar: constNewBars) {
        addBar(newBar);
    }
}
