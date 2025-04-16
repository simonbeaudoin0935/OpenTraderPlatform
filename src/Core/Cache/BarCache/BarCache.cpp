#include <QTimeZone>

#include "BarCache.h"
#include "TSClient.h"

Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")

BarCache::BarCache(const QString &symbol, bool isStreaming, QObject *parent):
    QObject(parent),
    symbol(symbol),
    isStreaming(isStreaming)
{
    if (isStreaming) {
        streamBar = TSClient::getInstance().openStreamBars(symbol,
                                                           1,
                                                           Bar::BarUnit::Minute,
                                                           10,
                                                           Bar::BarSessionTemplate::USEQ24Hour);
        Q_ASSERT(streamBar != nullptr);

        connect(streamBar, &StreamBars::receivedNewBar, this, &BarCache::onReceivedNewBar);
    }

}

BarCache::~BarCache()
{
    if (streamBar != nullptr) {
        TSClient::getInstance().closeStreamBars(streamBar);
    }
}


bool BarCache::warmUpBars(QDateTime first, QDateTime last)
{
    Q_UNUSED(first);
    Q_UNUSED(last);

    return false;
}

const QVector<Bar> BarCache::getBars(const QDateTime &first, const QDateTime &last) {
    Q_ASSERT(first.date().dayOfWeek() >= 1 && first.date().dayOfWeek() <= 5);
    Q_ASSERT(first.date() == last.date());
    Q_ASSERT(first.toTimeZone(QTimeZone("America/New_York")).time() >= QTime(6,0,0)); // Tradestation bars start at 6
    Q_ASSERT(last.toTimeZone(QTimeZone("America/New_York")).time() <= QTime(20,0,0));

    lastHitType = HitType::None;
    // Reset the counter of fetched bar for the last request
    lastNumberFetchedBars = 0;
    
    // Check cache first
    QVector<Bar> cachedBars = getBarsFromCache(first, last);
    
    // complete HIT : If we have a complete set, return it
    if (cachedBars.size() == (first.secsTo(last) / 60) + 1) {
        lastHitType = HitType::Hit;

        qCDebug(BarCacheLog) << Q_FUNC_INFO << " : Returning HIT";

        return cachedBars;
    }

    // complete MISS
    if (cachedBars.isEmpty()) {

        // If we get here, we have no bars in cache, fetch all requested bars
        QVector<Bar> fetchedBars;

        QDateTime fetchLast  = last.addSecs(60); // Need to add a minute because the API to bound is excluding the last minute

        bool success = TSClient::getInstance().getBarsSync(fetchedBars, symbol, 1, Bar::BarUnit::Minute, 0, Bar::BarSessionTemplate::USEQ24Hour, first, fetchLast);

        if (!success) {
          qCWarning(BarCacheLog) << "Failed to fetch bars from API for" << symbol;
            return QVector<Bar>();
        }

        qCDebug(BarCacheLog) << "Successfully fetched" << fetchedBars.size()
                             << "bars from API for" << symbol;

        // Important : adjust the timestamp of the returned bars to substract one minute
        for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();

        // Account for bar holes where no activity happened
        QVector<Bar> resultBars;
        {
            QDateTime expectedTime = first;
            qsizetype i = 0;

            while (expectedTime <= last) {

                Bar bar;

                if (i >= fetchedBars.size()) {
                    bar = Bar::nullBar(expectedTime);
                } else {
                    if (expectedTime == fetchedBars[i].getTimeStamp()) {
                        bar = fetchedBars[i];
                        i++;
                    } else {
                        bar = Bar::nullBar(expectedTime);
                    }
                }

                resultBars.append(bar);
                expectedTime = expectedTime.addSecs(60);  // Add one minute
            }
        }

        storeBarsInCache(resultBars);
        lastNumberFetchedBars = fetchedBars.size();
        lastHitType = HitType::Miss;

        qCDebug(BarCacheLog) << Q_FUNC_INFO << " : Returning MISS";

        return resultBars;
    }



    // partial HIT

    // Find gaps in the cached bars
    // Fetch missing bars for each gap
    QVector<Bar> allBars;
    int totalFetched = 0;

    QDateTime firstCachedBarTime = cachedBars.first().getTimeStamp();
    if (firstCachedBarTime > first) {

        qCDebug(BarCacheLog) << "Filling hole before";

        QVector<Bar> fetchedBars;
        bool success = TSClient::getInstance().getBarsSync(
            fetchedBars,
            symbol,
            1,
            Bar::BarUnit::Minute,
            0,
            Bar::BarSessionTemplate::USEQ24Hour,
            first,
            firstCachedBarTime);

        if (!success) {
            qCWarning(BarCacheLog) << "Failed to fetch missing bars from API for" << symbol
                                   << "in range" << first
                                   << "to" << firstCachedBarTime;
        }

        for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();

        QVector<Bar> resultBars;
        {
            QDateTime expectedTime = first;
            qsizetype i = 0;

            while (expectedTime < firstCachedBarTime) { // no <= here, as we fucked around with a addSec(60) to adjustedLast above

                Bar bar;

                if (i >= fetchedBars.size()) {
                    bar = Bar::nullBar(expectedTime);
                } else {
                    if (expectedTime == fetchedBars[i].getTimeStamp()) {
                        bar = fetchedBars[i];
                        i++;
                    } else {
                        bar = Bar::nullBar(expectedTime);
                    }
                }

                resultBars.append(bar);
                expectedTime = expectedTime.addSecs(60);  // Add one minute
            }
        }

        storeBarsInCache(resultBars);

        // Begin by adding those fetched pre-bars first
        allBars.append(fetchedBars);
        totalFetched += fetchedBars.size();
    }

    // Then, add the bars that were already cached
    allBars.append(cachedBars);

    QDateTime lastCachedBarTime = cachedBars.last().getTimeStamp();
    if (last > lastCachedBarTime) {

        qCDebug(BarCacheLog) << "Filling hole after";

        QDateTime fetchFirst = lastCachedBarTime.addSecs(60);
        QDateTime fetchLast  = last.addSecs(60); // Need to add a minute because the API to bound is excluding the last minute

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
                                   << "to" << last;
        }

        for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();

        QVector<Bar> resultBars;
        {
            QDateTime expectedTime = fetchFirst;
            qsizetype i = 0;

            while (expectedTime < fetchLast) { // no <= here, as we fucked around with a addSec(60) to adjustedLast above

                Bar bar;

                if (i >= fetchedBars.size()) {
                    bar = Bar::nullBar(expectedTime);
                } else {
                    if (expectedTime == fetchedBars[i].getTimeStamp()) {
                        bar = fetchedBars[i];
                        i++;
                    } else {
                        bar = Bar::nullBar(expectedTime);
                    }
                }

                resultBars.append(bar);
                expectedTime = expectedTime.addSecs(60);  // Add one minute
            }
        }


        storeBarsInCache(resultBars);

        // Then finally add those post-bars last
        allBars.append(resultBars);
        totalFetched += fetchedBars.size();
    }

    lastNumberFetchedBars = totalFetched;
    lastHitType = HitType::PartialHit;

    qCDebug(BarCacheLog) << Q_FUNC_INFO << " : Returning PARTIAL HIT";

    return allBars;
}

const QVector<Bar> BarCache::getAfterHourBars(const QDate &date)
{
    Q_ASSERT(date.dayOfWeek() >= 1 && date.dayOfWeek() <= 5);

    QTimeZone newYorkTimeZone("America/New_York");

    const QTime _4PM(16, 0, 0);
    const QTime _8PM(19, 59, 0);

    QDateTime fromDate = QDateTime(date, _4PM, newYorkTimeZone);
    QDateTime toDate   = QDateTime(date, _8PM, newYorkTimeZone);

    return getBars(fromDate, toDate);
}

void BarCache::storeBarInCache(const Bar& bar) {
    QDateTime dateTime = bar.getTimeStamp();

    if (!bar.getIsRealtime()) {
        if (barCacheOneMinute.contains(dateTime)) {
            qCDebug(BarCacheLog) << "Bar already in cache for" << symbol
                                 << "at " << dateTime;
            duplicateStoreCount++;
        }
    } else {
        if (bar.getBarStatus() == Bar::BarStatus::Closed) {
            qCDebug(BarCacheLog) << "Real time cache insertion; received the closing bar";
        }
    }

    barCacheOneMinute.insert(dateTime, bar);

    qCDebug(BarCacheLog) << "Inserted bar in cache with timestamp :" << bar.getTimeStamp();
}

void BarCache::storeBarsInCache(const QVector<Bar>& bars) {
    QWriteLocker locker(&rwLock);
    for (const Bar& bar : bars) {
        storeBarInCache(bar);
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
            qCDebug(BarCacheLog) << "Cache MISS - missing bar at" << expectedTime;
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

void BarCache::onReceivedNewBar(QString symbol, Bar newBar)
{
    if (this->symbol != symbol) {
        qFatal(BarCacheLog) << "Bars received for symbol " << symbol << " is not for this cache that is for " << this->symbol;
        return;
    }

    newBar.ajustTimeStampToOpeningMinute();

    storeBarInCache(newBar);

    //qCDebug(BarCacheLog).noquote() << "Received stream bar : " << newBar.toJsonString();
}
