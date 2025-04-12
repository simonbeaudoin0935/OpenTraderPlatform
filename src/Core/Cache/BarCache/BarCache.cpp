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
    
    if (!cachedBars.isEmpty()) {
        // If we have a complete set, return it
        if (cachedBars.size() == (adjustedFirst.secsTo(adjustedLast) / 60)) {
            lastHitType = HitType::Hit;
            return cachedBars;
        }
        
        // We have a partial hit, need to fetch missing bars
        QDateTime firstMissing = adjustedFirst;
        QDateTime lastMissing = adjustedLast;
        
        // Find the first missing bar
        for (const Bar& bar : cachedBars) {
            QDateTime barTime = QDateTime::fromString(bar.getTimeStamp(), Qt::ISODate);
            if (barTime == firstMissing) {
                firstMissing = firstMissing.addSecs(60);
            } else {
                break;
            }
        }
        
        // Find the last missing bar
        for (int i = cachedBars.size() - 1; i >= 0; --i) {
            QDateTime barTime = QDateTime::fromString(cachedBars[i].getTimeStamp(), Qt::ISODate);
            if (barTime == lastMissing) {
                lastMissing = lastMissing.addSecs(-60);
            } else {
                break;
            }
        }
        
        // Fetch missing bars
        QVector<Bar> fetchedBars;
        bool success = TSClient::getInstance().getBarsSync(fetchedBars, symbol, 1, Bar::BarUnit::Minute, 0, Bar::BarSessionTemplate::USEQ24Hour, firstMissing, lastMissing);
        
        lastNumberFetchedBars = fetchedBars.size();

        // Important : adjust the timestamp of the returned bars to substract one minute
        for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();

        if (success) {
            qCDebug(BarCacheLog) << "Successfully fetched" << fetchedBars.size() 
                                << "missing bars from API for" << symbol;
            
            // Store the fetched bars in cache
            storeBarsInCache(fetchedBars);
            
            // Combine cached and fetched bars
            QVector<Bar> allBars;
            allBars.reserve(cachedBars.size() + fetchedBars.size());
            
            // Add all bars in chronological order
            QDateTime currentTime = adjustedFirst;
            int cachedIndex = 0;
            int fetchedIndex = 0;
            
            while (currentTime <= adjustedLast) {
                if (cachedIndex < cachedBars.size()) {
                    QDateTime cachedTime = QDateTime::fromString(cachedBars[cachedIndex].getTimeStamp(), Qt::ISODate);
                    if (cachedTime == currentTime) {
                        allBars.append(cachedBars[cachedIndex]);
                        cachedIndex++;
                        currentTime = currentTime.addSecs(60);
                        continue;
                    }
                }
                
                if (fetchedIndex < fetchedBars.size()) {
                    QDateTime fetchedTime = QDateTime::fromString(fetchedBars[fetchedIndex].getTimeStamp(), Qt::ISODate);
                    if (fetchedTime == currentTime) {
                        allBars.append(fetchedBars[fetchedIndex]);
                        fetchedIndex++;
                        currentTime = currentTime.addSecs(60);
                        continue;
                    }
                }
                
                currentTime = currentTime.addSecs(60);
            }
            
            lastHitType = HitType::PartialHit;
            return allBars;
        } else {
            qCWarning(BarCacheLog) << "Failed to fetch missing bars from API for" << symbol;
            return cachedBars; // Return what we have in cache
        }
    }
    
    // If we get here, we have no bars in cache, fetch all requested bars
    QVector<Bar> fetchedBars;
    bool success = TSClient::getInstance().getBarsSync(fetchedBars, symbol, 1, Bar::BarUnit::Minute, 0, Bar::BarSessionTemplate::USEQ24Hour, adjustedFirst, adjustedLast);
    
    lastNumberFetchedBars = fetchedBars.size();

    // Important : adjust the timestamp of the returned bars to substract one minute
    for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();

    if (success) {
        qCDebug(BarCacheLog) << "Successfully fetched" << fetchedBars.size() 
                            << "bars from API for" << symbol;
        storeBarsInCache(fetchedBars);
        lastHitType = HitType::Miss;
        return fetchedBars;
    } else {
        qCWarning(BarCacheLog) << "Failed to fetch bars from API for" << symbol;
        return QVector<Bar>();
    }
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
    
    while (expectedTime < end) {
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
