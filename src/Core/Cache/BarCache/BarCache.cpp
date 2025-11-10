#include <QTimeZone>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

#include "BarCache.h"
#include "TSClient.h"

Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")

BarCache::BarCache(const QString &symbol, bool isStreaming, QObject *parent):
    QObject(parent),
    symbol(symbol),
    isStreaming(isStreaming)
{
    this->setObjectName("BarCache::" + symbol);

    // Set up database - one database file per symbol
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/bars_cache_" + symbol + ".db";
    bool dbFileExisted = QFileInfo::exists(dbPath);
    {
        QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        qCInfo(BarCacheLog) << "Cache location:" << cacheLocation;
        qCInfo(BarCacheLog) << "Using database file:" << dbPath;
        qCInfo(BarCacheLog) << "Database file existed:" << dbFileExisted;

        QFileInfo dbInfo(dbPath);
        qCInfo(BarCacheLog) << "Database directory:" << dbInfo.absolutePath()
                           << "Dir exists:" << dbInfo.dir().exists()
                           << "File readable:" << dbInfo.isReadable()
                           << "File writable:" << dbInfo.isWritable();

        // Create the directory if it doesn't exist
        if (!dbInfo.dir().exists()) {
            if (!dbInfo.dir().mkpath(".")) {
                qFatal("Failed to create cache directory: %s", qPrintable(dbInfo.absolutePath()));
            }
            qCInfo(BarCacheLog) << "Created cache directory:" << dbInfo.absolutePath();
        }

        qCInfo(BarCacheLog) << "SQLite connection name to be used:" << ("BarCache_" + symbol);
    }
    db = QSqlDatabase::addDatabase("QSQLITE", "BarCache_" + symbol);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qFatal("Failed to open database for %s: %s", qPrintable(symbol), qPrintable(db.lastError().text()));
    } else {
        if (dbFileExisted) {
            qCInfo(BarCacheLog) << "Opened existing database for symbol" << symbol << "at" << dbPath;
        } else {
            qCInfo(BarCacheLog) << "Created new database for symbol" << symbol << "at" << dbPath;
        }
        
        // Create table if not exists
        QSqlQuery query(db);
        query.exec("CREATE TABLE IF NOT EXISTS bars ("
                   "timestamp INTEGER PRIMARY KEY, "
                   "open REAL, "
                   "high REAL, "
                   "low REAL, "
                   "close REAL, "
                   "volume INTEGER)");
        if (query.lastError().isValid()) {
            qWarning() << "Failed to create table:" << query.lastError().text();
        }
    }

    if (isStreaming) {
        streamBar = TSClient::getInstance().openStreamBars(symbol,
                                                           1,
                                                           Bar::BarUnit::Minute,
                                                           2,
                                                           Bar::BarSessionTemplate::USEQ24Hour);
        Q_ASSERT(streamBar != nullptr);

        connect(streamBar, &StreamBars::receivedNewBar, this, &BarCache::onReceivedNewBar);
    }

}

BarCache::~BarCache()
{
    QString cacheName = this->objectName();

    if (streamBar != nullptr) {
        TSClient::getInstance().closeStreamBars(streamBar);
    }

    qCDebug(BarCacheLog) << cacheName << "Destroyed";
}

bool BarCache::warmUpBarsOfDayUntilNow(QDate date)
{
    QString cacheName = this->objectName();

    QDateTime _6AM = QDateTime(date, QTime(6,0), QTimeZone("America/New_York"));
    QDateTime _4PM = QDateTime(date, QTime(15,59), QTimeZone("America/New_York"));

    QDateTime now = QDateTime::currentDateTime();


    if (now.date() == date) {
        qCDebug(BarCacheLog) << cacheName << "Warming up cache with all bars from 6AM to now";

        getBars(_6AM, now);
    } else {
        qCDebug(BarCacheLog) << cacheName << "Warming up cache with all bars from " << date;
        getBars(_6AM, _4PM);
    }

    return true;
}

const QVector<Bar> BarCache::getBars(const QDateTime &first, const QDateTime &last) {
    QString cacheName = this->objectName();

    Q_ASSERT_X(first.date().dayOfWeek() >= 1 && first.date().dayOfWeek() <= 5,
               qPrintable(cacheName),
               "getBars() called not strictly in between monday to friday");
    Q_ASSERT_X(first.date() == last.date(),
               qPrintable(cacheName),
               "Not sure yet what this is"); // TODO crashes whenever the zoom of the stock chart goes too far out and the chart asks for bars that are from the previous day, spanning accross the night
    Q_ASSERT_X(first.toTimeZone(QTimeZone("America/New_York")).time() >= QTime(6,0,0),
               qPrintable(cacheName),
               "Fetching bars before 6am"); // Tradestation bars start at 6
    Q_ASSERT_X(last.toTimeZone(QTimeZone("America/New_York")).time() <= QTime(20,0,0),
               qPrintable(cacheName),
               "Fetching bars after 8pm");

    lastHitType = HitType::None;
    // Reset the counter of fetched bar for the last request
    lastNumberFetchedBars = 0;
    
    // Check database first and load into memory cache
    QVector<Bar> dbBars = getBarsFromDatabase(first, last);
    if (!dbBars.isEmpty()) {
        storeBarsInCache(dbBars);
    }
    
    // Check cache first
    QVector<Bar> cachedBars = getBarsFromCache(first, last);
    
    // complete HIT : If we have a complete set, return it
    if (cachedBars.size() == (first.secsTo(last) / 60) + 1) {
        lastHitType = HitType::Hit;

        qCDebug(BarCacheLog) << cacheName << " : Returning HIT";

        return cachedBars;
    }

    // complete MISS
    if (cachedBars.isEmpty()) {

        // If we get here, we have no bars in cache, fetch all requested bars
        QVector<Bar> fetchedBars;

        QDateTime fetchLast  = last.addSecs(60); // Need to add a minute because the API to bound is excluding the last minute

        bool success = TSClient::getInstance().getBarsSync(fetchedBars, symbol, 1, Bar::BarUnit::Minute, 0, Bar::BarSessionTemplate::USEQ24Hour, first, fetchLast);

        if (!success) {
          qCCritical(BarCacheLog) << cacheName << "Failed to fetch bars from API for" << symbol;
            return QVector<Bar>();
        }

        qCDebug(BarCacheLog) << cacheName << "Successfully fetched" << fetchedBars.size()
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

        qCDebug(BarCacheLog) << cacheName << " : Returning MISS";

        return resultBars;
    }

    // partial HIT

    // Find gaps in the cached bars
    // Fetch missing bars for each gap
    QVector<Bar> allBars;
    int totalFetched = 0;

    QDateTime firstCachedBarTime = cachedBars.first().getTimeStamp();
    if (firstCachedBarTime > first) {

        qCDebug(BarCacheLog) << cacheName << "Filling hole before";

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
            qCWarning(BarCacheLog) << cacheName << "Failed to fetch missing bars from API for" << symbol
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

        qCDebug(BarCacheLog) << cacheName << "Filling hole after";

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
            qCCritical(BarCacheLog) << cacheName << "Failed to fetch missing bars from API for" << symbol
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

    qCDebug(BarCacheLog) << cacheName << " : Returning PARTIAL HIT";

    return allBars;
}

const QVector<Bar> BarCache::getAfterHourBars(const QDate &date)
{
    QString cacheName = this->objectName();

    Q_ASSERT_X(date.dayOfWeek() >= 1 && date.dayOfWeek() <= 5, qPrintable(cacheName), "Not monday to friday");

    QTimeZone newYorkTimeZone("America/New_York");

    const QTime _4PM(16, 0, 0);
    const QTime _8PM(19, 59, 0);

    QDateTime fromDate = QDateTime(date, _4PM, newYorkTimeZone);
    QDateTime toDate   = QDateTime(date, _8PM, newYorkTimeZone);

    return getBars(fromDate, toDate);
}

void BarCache::storeBarInCache(const Bar& bar) {
    QString cacheName = this->objectName();

    QDateTime dateTime = bar.getTimeStamp();

    if (!bar.getIsRealtime()) {
        if (barCacheOneMinute.contains(dateTime)) {
            qCWarning(BarCacheLog) << cacheName << "Bar already in cache for" << symbol
                                 << "at " << dateTime;
            duplicateStoreCount++;
        }
    } else {
        if (bar.getBarStatus() == Bar::BarStatus::Closed) {
            qCDebug(BarCacheLog) << cacheName << "Real time cache insertion; received the closing bar";
        }
    }

    {
        QWriteLocker locker(&rwLock);
        barCacheOneMinute.insert(dateTime, bar);
    }

    qCDebug(BarCacheLog) << cacheName << "Inserted bar in cache with timestamp :" << bar.getTimeStamp();
}

void BarCache::storeBarsInCache(const QVector<Bar>& bars) {
    for (const Bar& bar : bars) {
        storeBarInCache(bar);
    }
    storeBarsInDatabase(bars);
}

QVector<Bar> BarCache::getBarsFromCache(QDateTime start, QDateTime end) const {
    QString cacheName = this->objectName();

    QVector<Bar> result;
    QVector<QDateTime> missingTimes;
    bool hasCompleteSet = true;
    qCDebug(BarCacheLog) << cacheName << "Checking cache for" << symbol
                         << "from" << start.toString() 
                         << "to" << end.toString();
    


    // Check for complete set and identify missing bars
    {
        QReadLocker locker(&rwLock);

        // Find the first bar at or after start
        auto it = barCacheOneMinute.lowerBound(start);

        for (QDateTime expectedTime = start; expectedTime <= end; expectedTime = expectedTime.addSecs(60)) {
            if (it == barCacheOneMinute.end() || it.key() != expectedTime) {
                hasCompleteSet = false;
                missingTimes.append(expectedTime);
                qCDebug(BarCacheLog) << cacheName << "Cache MISS - missing bar at" << expectedTime;
            } else {
                result.append(it.value());
                ++it;
            }
        }
    }
    
    if (hasCompleteSet) {
        qCDebug(BarCacheLog) << cacheName << "Cache HIT - found complete set of bars for" << symbol;
    } else if (!result.isEmpty()) {
        qCDebug(BarCacheLog) << cacheName << "Cache PARTIAL HIT - found" << result.size()
                            << "bars, missing" << missingTimes.size() << "bars";
        // We have a partial hit, return what we have and let the caller handle the missing bars
        return result;
    } else {
        qCDebug(BarCacheLog) << cacheName << "Cache MISS - no bars found for" << symbol;
    }
    
    return result;
}

void BarCache::onReceivedNewBar(QString symbol, Bar newBar)
{
    QString cacheName = this->objectName();

    Q_ASSERT_X(this->symbol == symbol, qPrintable(cacheName), "Mismatch in symbol");

    newBar.ajustTimeStampToOpeningMinute();

    storeBarInCache(newBar);

    emit receivedNewBar(symbol, newBar);
}

QVector<Bar> BarCache::getBarsFromDatabase(QDateTime start, QDateTime end) const {
    QVector<Bar> bars;
    if (!db.isOpen()) return bars;

    QSqlQuery query(db);
    query.prepare("SELECT timestamp, open, high, low, close, volume FROM bars WHERE timestamp >= ? AND timestamp <= ? ORDER BY timestamp");
    query.addBindValue(start.toSecsSinceEpoch());
    query.addBindValue(end.toSecsSinceEpoch());

    if (query.exec()) {
        while (query.next()) {
            QDateTime ts = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
            double open = query.value(1).toDouble();
            double high = query.value(2).toDouble();
            double low = query.value(3).toDouble();
            double close = query.value(4).toDouble();
            qint64 volume = query.value(5).toLongLong();
            bars.append(Bar(ts, open, high, low, close, volume));
        }
    } else {
        qWarning() << "Database query failed:" << query.lastError().text();
    }
    return bars;
}

void BarCache::storeBarsInDatabase(const QVector<Bar>& bars) {
    if (!db.isOpen() || bars.isEmpty()) return;

    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO bars (timestamp, open, high, low, close, volume) VALUES (?, ?, ?, ?, ?, ?)");

    for (const Bar& bar : bars) {
        query.addBindValue(bar.getTimeStamp().toSecsSinceEpoch());
        query.addBindValue(bar.getOpen());
        query.addBindValue(bar.getHigh());
        query.addBindValue(bar.getLow());
        query.addBindValue(bar.getClose());
        query.addBindValue(bar.getTotalVolume());
        if (!query.exec()) {
            qWarning() << "Failed to store bar in database:" << query.lastError().text();
        }
    }
}
