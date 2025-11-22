#include <QTimeZone>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

#include "BarCache.h"
#include "TSClient.h"
#include "Settings.h"

Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")

BarCache::BarCache(const QString &symbol, bool isStreaming, QObject *parent):
    QObject(parent),
    symbol(symbol),
    isStreaming(isStreaming)
{
    this->setObjectName("BarCache::" + symbol);

    // Set up database - one database file per symbol
    QString cacheLocation = getCacheLocation();
    QString dbPath = cacheLocation + "/bars_cache_" + symbol + ".db";
    bool dbFileExisted = QFileInfo::exists(dbPath);
    {
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
        connect(streamBar, &Stream::streamErrorOccurred, this, &BarCache::onStreamError);
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
    
    // Check memory cache first
    QVector<Bar> cachedBars = getBarsFromCache(first, last);
    
    // complete HIT : If we have a complete set, return it
    if (cachedBars.size() == (first.secsTo(last) / 60) + 1) {
        lastHitType = HitType::Hit;

        qCDebug(BarCacheLog) << cacheName << " : Returning complete HIT from memory cache";

        return cachedBars;
    }

    // If we have a partial hit or complete miss, check database for missing bars
    if (!cachedBars.isEmpty() || cachedBars.isEmpty()) {
        // Identify missing time ranges
        QVector<QPair<QDateTime, QDateTime>> missingRanges = identifyMissingRanges(first, last, cachedBars);
        
        // Load missing bars from database
        for (const auto& range : missingRanges) {
            QVector<Bar> dbBars = getBarsFromDatabase(range.first, range.second);
            if (!dbBars.isEmpty()) {
                qCInfo(BarCacheLog) << cacheName << "Loaded" << dbBars.size() 
                                   << "bars from database into memory cache for" << symbol
                                   << "in range" << range.first << "to" << range.second;
                storeBarsInCache(dbBars);
            }
        }
        
        // Check memory cache again after loading from database
        cachedBars = getBarsFromCache(first, last);
        
        // complete HIT after database load : If we have a complete set now, return it
        if (cachedBars.size() == (first.secsTo(last) / 60) + 1) {
            lastHitType = HitType::Hit;

            qCDebug(BarCacheLog) << cacheName << " : Returning HIT (after database load)";

            return cachedBars;
        }
    }

    // At this point, we have loaded everything we could from memory + database
    // Track whether we had any bars before API call
    bool hadBarsBeforeApi = !cachedBars.isEmpty();

    // If we still don't have a complete set after database loading, fetch from API
    if (cachedBars.size() != (first.secsTo(last) / 60) + 1) {
        // Identify remaining missing ranges
        QVector<QPair<QDateTime, QDateTime>> missingRanges = identifyMissingRanges(first, last, cachedBars);
        
        // Fetch missing bars from API for each range
        QVector<Bar> allFetchedBars;
        for (const auto& range : missingRanges) {
            QVector<Bar> fetchedBars;
            QDateTime fetchLast = range.second.addSecs(60); // Need to add a minute because the API bounds are excluding the last minute
            
            bool success = TSClient::getInstance().getBarsSync(fetchedBars, symbol, 1, Bar::BarUnit::Minute, 0, Bar::BarSessionTemplate::USEQ24Hour, range.first, fetchLast);
            
            if (!success) {
                qCCritical(BarCacheLog) << cacheName << "Failed to fetch bars from API for" << symbol
                                       << "in range" << range.first << "to" << range.second;
                return QVector<Bar>();
            }
            
            // Adjust timestamps
            for(Bar& bar: fetchedBars) bar.ajustTimeStampToOpeningMinute();
            allFetchedBars.append(fetchedBars);
        }
        
        qCDebug(BarCacheLog) << cacheName << "Successfully fetched" << allFetchedBars.size()
                             << "bars from API for" << symbol;
        
        // Account for bar holes where no activity happened
        QVector<Bar> resultBars;
        int voidBarsCreated = 0;
        for (const auto& range : missingRanges) {
            QDateTime expectedTime = range.first;
            qsizetype i = 0;
            
            while (expectedTime <= range.second) {
                Bar bar;
                
                if (i >= allFetchedBars.size()) {
                    bar = Bar::nullBar(expectedTime);
                    voidBarsCreated++;
                } else {
                    if (expectedTime == allFetchedBars[i].getTimeStamp()) {
                        bar = allFetchedBars[i];
                        i++;
                    } else {
                        bar = Bar::nullBar(expectedTime);
                        voidBarsCreated++;
                    }
                }
                
                resultBars.append(bar);
                expectedTime = expectedTime.addSecs(60);
            }
        }
        
        if (voidBarsCreated > 0) {
            qCDebug(BarCacheLog) << cacheName << "Created" << voidBarsCreated 
                                << "void bars to account for periods with no trading activity";
        }
        
        storeBarsInCache(resultBars);
        lastNumberFetchedBars = allFetchedBars.size();
        
        // Rebuild complete result
        cachedBars = getBarsFromCache(first, last);
        lastHitType = hadBarsBeforeApi ? HitType::PartialHit : HitType::Miss;
        
        qCDebug(BarCacheLog) << cacheName << " : Returning" 
                            << (hadBarsBeforeApi ? "PARTIAL HIT" : "MISS");
        
        return cachedBars;
    }
    
    // If we get here, we should have a complete set
    Q_ASSERT(cachedBars.size() == (first.secsTo(last) / 60) + 1);
    return cachedBars;
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

void BarCache::onStreamError(Stream::StreamError error, QString errorMessage)
{
    QString cacheName = this->objectName();

    qCWarning(BarCacheLog) << cacheName << "Stream error occurred - Error:" << static_cast<int>(error)
                           << "Message:" << errorMessage;

    // Log specific error types for better diagnostics
    switch (error) {
        case Stream::StreamError::Timeout:
            qCWarning(BarCacheLog) << cacheName << "Stream timeout - no data or heartbeat received";
            break;
        case Stream::StreamError::InvalidSymbol:
            qCCritical(BarCacheLog) << cacheName << "Invalid symbol error - this should not happen";
            break;
        case Stream::StreamError::DualLogon:
            qCCritical(BarCacheLog) << cacheName << "Dual logon detected - another session may be active";
            break;
        case Stream::StreamError::GoAway:
            qCWarning(BarCacheLog) << cacheName << "Server requested stream closure";
            break;
        case Stream::StreamError::InternalServerError:
            qCCritical(BarCacheLog) << cacheName << "Internal server error";
            break;
        case Stream::StreamError::BadRequest:
            qCCritical(BarCacheLog) << cacheName << "Bad request error";
            break;
        case Stream::StreamError::Unknown:
            qCCritical(BarCacheLog) << cacheName << "Unknown stream error";
            break;
    }

    // Note: Stream errors in BarCache are logged but not automatically recovered
    // The stream is in error state and will need to be recreated by closing and
    // reopening the BarCache if recovery is needed
}

QVector<Bar> BarCache::getBarsFromDatabase(QDateTime start, QDateTime end) const {
    QVector<Bar> bars;
    Q_ASSERT_X(db.isOpen(), qPrintable(objectName()), "Database must be open for reading bars");

    qCDebug(BarCacheLog) << "Reading bars from database for" << symbol
                         << "from" << start.toString() << "to" << end.toString();

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
            
            Bar bar;
            // Check if this is a null/void bar (all OHLC values and volume are 0)
            if (open == 0.0 && high == 0.0 && low == 0.0 && close == 0.0 && volume == 0) {
                bar = Bar::nullBar(ts);
            } else {
                bar = Bar(ts, open, high, low, close, volume);
            }
            
            bars.append(bar);
        }
        qCInfo(BarCacheLog) << "Loaded" << bars.size() << "bars from database for" << symbol;
    } else {
        qCWarning(BarCacheLog) << "Database query failed for" << symbol << ":" << query.lastError().text();
    }
    return bars;
}

void BarCache::storeBarsInDatabase(const QVector<Bar>& bars) {
    Q_ASSERT_X(db.isOpen(), qPrintable(objectName()), "Database must be open for storing bars");
    if (bars.isEmpty()) {
        qCDebug(BarCacheLog) << "No bars to store in database for" << symbol;
        return;
    }

    qCInfo(BarCacheLog) << "Storing" << bars.size() << "bars in database for" << symbol;

    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO bars (timestamp, open, high, low, close, volume) VALUES (?, ?, ?, ?, ?, ?)");

    int storedCount = 0;
    for (const Bar& bar : bars) {
        query.addBindValue(bar.getTimeStamp().toSecsSinceEpoch());
        query.addBindValue(bar.getOpen());
        query.addBindValue(bar.getHigh());
        query.addBindValue(bar.getLow());
        query.addBindValue(bar.getClose());
        query.addBindValue(bar.getTotalVolume());
        if (query.exec()) {
            storedCount++;
        } else {
            qCWarning(BarCacheLog) << "Failed to store bar in database for" << symbol
                                   << "at" << bar.getTimeStamp().toString() << ":"
                                   << query.lastError().text();
        }
    }

    qCInfo(BarCacheLog) << "Successfully stored" << storedCount << "bars in database for" << symbol;
}

void BarCache::clearDatabase() {
    Q_ASSERT_X(db.isOpen(), qPrintable(objectName()), "Database must be open for clearing");
    
    qCInfo(BarCacheLog) << "Clearing all bars from database for" << symbol;
    
    QSqlQuery query(db);
    if (query.exec("DELETE FROM bars")) {
        qCInfo(BarCacheLog) << "Successfully cleared database for" << symbol;
    } else {
        qCWarning(BarCacheLog) << "Failed to clear database for" << symbol << ":" << query.lastError().text();
    }
}

QVector<QPair<QDateTime, QDateTime>> BarCache::identifyMissingRanges(QDateTime start, QDateTime end, const QVector<Bar>& cachedBars) const {
    QVector<QPair<QDateTime, QDateTime>> missingRanges;
    
    if (cachedBars.isEmpty()) {
        // Complete miss - entire range is missing
        missingRanges.append(qMakePair(start, end));
        return missingRanges;
    }
    
    // Check for gap before first cached bar
    QDateTime firstCachedTime = cachedBars.first().getTimeStamp();
    if (firstCachedTime > start) {
        missingRanges.append(qMakePair(start, firstCachedTime.addSecs(-60)));
    }
    
    // Check for gaps between cached bars
    for (int i = 0; i < cachedBars.size() - 1; ++i) {
        QDateTime currentEnd = cachedBars[i].getTimeStamp();
        QDateTime nextStart = cachedBars[i + 1].getTimeStamp();
        
        // Expected next bar time
        QDateTime expectedNext = currentEnd.addSecs(60);
        
        if (expectedNext < nextStart) {
            // There's a gap
            missingRanges.append(qMakePair(expectedNext, nextStart.addSecs(-60)));
        }
    }
    
    // Check for gap after last cached bar
    QDateTime lastCachedTime = cachedBars.last().getTimeStamp();
    if (lastCachedTime < end) {
        missingRanges.append(qMakePair(lastCachedTime.addSecs(60), end));
    }
    
    return missingRanges;
}
