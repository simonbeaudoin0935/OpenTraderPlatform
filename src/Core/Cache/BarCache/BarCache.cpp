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
    lastHitType = HitType::None;
    // Reset the counter of fetched bar for the last request
    lastNumberFetchedBars = 0;

    // Adjust for weekends
    QDateTime adjustedFirst = first;
    QDateTime adjustedLast = last;

    // If first date is on a weekend, move it to the next Monday
    while (adjustedFirst.date().dayOfWeek() > 5) { // 6 = Saturday, 7 = Sunday
        adjustedFirst = adjustedFirst.addDays(1);
    }
    
    // If last date is on a weekend, move it to the previous Friday
    while (adjustedLast.date().dayOfWeek() > 5) {
        adjustedLast = adjustedLast.addDays(-1);
    }
    
    // Check cache first
    QVector<Bar> cachedBars = getBarsFromCache(adjustedFirst, adjustedLast);
    
    // complete MISS
    if (cachedBars.isEmpty()) {

        // If we get here, we have no bars in cache, fetch all requested bars
        QVector<Bar> fetchedBars;

        QDateTime fetchFirst = adjustedFirst;
        QDateTime fetchLast  = adjustedLast.addSecs(60); // Need to add a minute because the API to bound is excluding the last minute

        bool success = TSClient::getInstance().getBarsSync(fetchedBars, symbol, 1, Bar::BarUnit::Minute, 0, Bar::BarSessionTemplate::USEQ24Hour, fetchFirst, fetchLast);

        if (!success) {
          qCWarning(BarCacheLog) << "Failed to fetch bars from API for" << symbol;
            return QVector<Bar>();
        }

        qCDebug(BarCacheLog) << "Successfully fetched" << fetchedBars.size()
                             << "bars from API for" << symbol;

        // Important : adjust the timestamp of the returned bars to substract one minute
        for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();

        storeBarsInCache(fetchedBars);
        lastNumberFetchedBars = fetchedBars.size();
        lastHitType = HitType::Miss;

        qCDebug(BarCacheLog) << Q_FUNC_INFO << " : Returning MISS";

        return fetchedBars;
    }

    // complete HIT : If we have a complete set, return it
    if (cachedBars.size() == (adjustedFirst.secsTo(adjustedLast) / 60) + 1) {
        lastHitType = HitType::Hit;

        qCDebug(BarCacheLog) << Q_FUNC_INFO << " : Returning HIT";

        return cachedBars;
    }

    // partial HIT

    // Find gaps in the cached bars
    // Fetch missing bars for each gap
    QVector<Bar> allBars;
    int totalFetched = 0;

    QDateTime firstCachedBarTime = QDateTime::fromString(cachedBars.first().getTimeStamp(), Qt::ISODate);
    if (firstCachedBarTime > adjustedFirst) {

        qCDebug(BarCacheLog) << "Filling hole before";

        QVector<Bar> fetchedBars;
        bool success = TSClient::getInstance().getBarsSync(
            fetchedBars,
            symbol,
            1,
            Bar::BarUnit::Minute,
            0,
            Bar::BarSessionTemplate::USEQ24Hour,
            adjustedFirst,
            firstCachedBarTime);

        if (!success) {
            qCWarning(BarCacheLog) << "Failed to fetch missing bars from API for" << symbol
                                   << "in range" << adjustedFirst
                                   << "to" << firstCachedBarTime;
        }

        for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();
        storeBarsInCache(fetchedBars);

        // Begin by adding those fetched pre-bars first
        allBars.append(fetchedBars);
        totalFetched += fetchedBars.size();
    }

    // Then, add the bars that were already cached
    allBars.append(cachedBars);

    QDateTime lastCachedBarTime = QDateTime::fromString(cachedBars.last().getTimeStamp(), Qt::ISODate);
    if (adjustedLast > lastCachedBarTime) {

        qCDebug(BarCacheLog) << "Filling hole after";

        QDateTime fetchFirst = lastCachedBarTime.addSecs(60);
        QDateTime fetchLast  = adjustedLast.addSecs(60); // Need to add a minute because the API to bound is excluding the last minute

        QVector<Bar> fetchedBars;
        bool success = TSClient::getInstance().getBarsSync(
            fetchedBars,
            symbol,
            1,
            Bar::BarUnit::Minute,
            0,
            Bar::BarSessionTemplate::USEQ24Hour,
            fetchFirst,
            fetchLast);

        if (!success) {
            qCWarning(BarCacheLog) << "Failed to fetch missing bars from API for" << symbol
                                   << "in range" << lastCachedBarTime
                                   << "to" << adjustedLast;
        }

        for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();
        storeBarsInCache(fetchedBars);

        // Then finally add those post-bars last
        allBars.append(fetchedBars);
        totalFetched += fetchedBars.size();
    }

    lastNumberFetchedBars = totalFetched;
    lastHitType = HitType::PartialHit;

    qCDebug(BarCacheLog) << Q_FUNC_INFO << " : Returning PARTIAL HIT";

    return allBars;
}

void BarCache::storeBarsInCache(const QVector<Bar>& bars) {
    QWriteLocker locker(&rwLock);
    for (const Bar& bar : bars) {
        QDateTime dateTime = QDateTime::fromString(bar.getTimeStamp(), Qt::ISODate);
        if (barCacheOneMinute.contains(dateTime)) {
            qCDebug(BarCacheLog) << "Bar already in cache for" << symbol
                                << "at " << dateTime;
            duplicateStoreCount++;
        }
        barCacheOneMinute.insert(dateTime, bar);

        qCDebug(BarCacheLog) << "Inserted bar in cache with timestamp :" << bar.getTimeStamp();

    }
}

QVector<Bar> BarCache::getBarsFromCache(QDateTime start, QDateTime end) const {
    QVector<Bar> result;
    QVector<QDateTime> missingTimes;
    
    qCDebug(BarCacheLog) << "Checking cache for" << symbol 
                         << "from" << start.toString() 
                         << "to" << end.toString();
    
    // Find the first bar at or after start
    auto it = barCacheOneMinute.lowerBound(start);
    
    // Check for complete set and identify missing bars
    QDateTime expectedTime = start;
    bool hasCompleteSet = true;
    
    while (expectedTime <= end) {
        if (it == barCacheOneMinute.end() || it.key() != expectedTime) {
            hasCompleteSet = false;
            missingTimes.append(expectedTime);
            qCDebug(BarCacheLog) << "Cache MISS - missing bar at" << expectedTime.toString();
        } else {
            result.append(it.value());
            ++it;
        }
        expectedTime = expectedTime.addSecs(60);  // Add one minute
    }
    
    if (hasCompleteSet) {
        qCDebug(BarCacheLog) << "Cache HIT - found complete set of bars for" << symbol;
    } else if (!result.isEmpty()) {
        qCDebug(BarCacheLog) << "Cache PARTIAL HIT - found" << result.size() 
                            << "bars, missing" << missingTimes.size() << "bars";
        // We have a partial hit, return what we have and let the caller handle the missing bars
        return result;
    } else {
        qCDebug(BarCacheLog) << "Cache MISS - no bars found for" << symbol;
    }
    
    return result;
}

void BarCache::onGetBarsReceived(QString symbol, QVector<Bar> newBars)
{
    if (this->symbol != symbol) {
        qCWarning(BarCacheLog) << "Bars received for symbol " << symbol << " is not for this cache that is for " << this->symbol;
        return;
    }

    for(Bar& bar: newBars) {
        bar.ajustTimeStampToOpeningMinute();
    }

    storeBarsInCache(newBars);
}
