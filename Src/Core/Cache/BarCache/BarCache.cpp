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
    Q_ASSERT(parent != nullptr);
    Q_ASSERT_X(!barCacheMap.contains(symbol), "BarCache::BarCache", "BarCache for symbol already exists");
    
    barCacheMap.insert(symbol, this);

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

        connect(streamBar, &StreamBars::receivedNewBar, this, &BarCache::onReceivedNewLiveBar);
        connect(streamBar, &Stream::streamErrorOccurred, this, &BarCache::onStreamError);
    }


    // Ensure TSClient signal for async get bars is connected only once to the static slot onReceivedAsyncGetBars
    // which will then forward to the appropriate BarCache instance based on symbol
    {
        static bool TSCLientReceivedAsyncGetBarsSignalConnected = false;

        if (TSCLientReceivedAsyncGetBarsSignalConnected == false) {
            TSCLientReceivedAsyncGetBarsSignalConnected = true;
            connect(TSClient::getInstancePtr(), &TSClient::receivedAsyncGetBars, &BarCache::onReceivedAsyncGetBars);
        }
    }

}

BarCache::~BarCache()
{
    const QString cacheName = this->objectName();

    if (streamBar != nullptr) {
        TSClient::getInstance()->closeStreamBars(streamBar);
    }

    qCDebug(BarCacheLog) << cacheName << "Destroyed";

    #warning need to remove database connection here, and from the static map too
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

/*
 * Static method which is connected to the TSClient singleton instances's receivedAsyncGetBars() signal.
 */
void BarCache::onReceivedAsyncGetBars(TSClient::AsyncRequestID_t requestID, TSClient::AsyncRequestStatus_e status, QVector<Bar> bars)
{
    Q_ASSERT(requestID != 0);

    Q_ASSERT(m_asyncReqIdToSymbol.contains(requestID));
    QString symbol = m_asyncReqIdToSymbol[requestID];
    m_asyncReqIdToSymbol.remove(requestID);
    Q_ASSERT(barCacheMap.contains(symbol));

    BarCache* barCacheInstance = barCacheMap.value(symbol);
    Q_ASSERT(barCacheInstance != nullptr);

    barCacheInstance->onReceivedAsyncGetBarsInstance(requestID, status, bars);
}

void BarCache::onReceivedAsyncGetBarsInstance(TSClient::AsyncRequestID_t requestID, TSClient::AsyncRequestStatus_e status, QVector<Bar> bars)
{
    const QString cacheName = this->objectName();

    Q_ASSERT_X(pendingAsyncGetBarRequests.contains(requestID),
               qPrintable(cacheName),
               "Received async getBars response for unknown requestID");

    qCDebug(BarCacheLog) << cacheName << "Received async getBars response for requestID" << requestID
                        << "with status" << static_cast<int>(status)
                        << "and" << bars.size() << "bars";

    if (status == TSClient::AsyncRequestStatus_e::SUCCESS) {
        storeBarsInCache(bars);
    } else if (status == TSClient::AsyncRequestStatus_e::ERROR){
        qCCritical(BarCacheLog) << cacheName << "Async getBars request failed with status" << static_cast<int>(status);
        Q_ASSERT(false); // TODO handle errors properly
    } else if (status == TSClient::AsyncRequestStatus_e::TIMEOUT) {
        qCCritical(BarCacheLog) << cacheName << "Async getBars request timed out";
        Q_ASSERT(false); // TODO handle errors properly
    }

    pendingAsyncGetBarRequests[requestID].fulfilled = true;
    pendingAsyncGetBarRequests[requestID].bars = bars;

    // Check if all pending requests are fulfilled
    bool allFulfilled = true;
    for (auto it = pendingAsyncGetBarRequests.constBegin(); it != pendingAsyncGetBarRequests.constEnd(); ++it) {
        if (it.value().fulfilled == false) {
            allFulfilled = false;
            break;
        }
    }
    if (allFulfilled) {
        handleReceivedAllPendingGetBarsRequests();
    }
}

QVector<Bar> BarCache::fillHolesOfReceivedRequest(const QDateTime& first, const QDateTime& last, const QVector<Bar>& barsFromAPI)
{
    const QString cacheName = this->objectName();

    Q_ASSERT_X(first.date() == last.date(), qPrintable(cacheName), "fillHolesOfReceivedRequest() only supports single-day ranges");
    Q_ASSERT_X(first.date().dayOfWeek() >= MONDAY && first.date().dayOfWeek() <= FRIDAY, qPrintable(cacheName), "getBars() called with date outside Monday-Friday range");
    Q_ASSERT_X(first.time() >= QTime(TRADING_START_HOUR, 0, 0), qPrintable(cacheName), "Fetching bars before 6am"); // Tradestation bars start at 6
    Q_ASSERT_X(last.time() <= QTime(TRADING_END_HOUR, 0, 0), qPrintable(cacheName), "Fetching bars after 8pm");

    // Account for bar holes where no activity happened
    QVector<Bar> resultBars;
    int voidBarsCreated = 0;

    
    qsizetype i = 0;
            
    for (QDateTime expectedTime = first; expectedTime <= last; expectedTime = expectedTime.addSecs(60)) {
        Bar bar;
                
        if (i >= barsFromAPI.size()) {
            bar = Bar::nullBar(expectedTime);
            voidBarsCreated++;
        } else {
            if (expectedTime == barsFromAPI[i].getTimeStamp()) {
                bar = barsFromAPI[i];
                i++;
            } else {
                bar = Bar::nullBar(expectedTime);
                voidBarsCreated++;
            }
        }
                
        resultBars.append(bar);
    }
        
    if (voidBarsCreated > 0) {
        qCDebug(BarCacheLog) << cacheName << "Created" << voidBarsCreated 
                             << "void bars to account for periods with no trading activity";
    }

    return resultBars;
}

void BarCache::handleReceivedAllPendingGetBarsRequests()
{
    const QString cacheName = this->objectName();

        
    qCDebug(BarCacheLog) << cacheName << "Successfully fetched" << pendingAsyncGetBarRequests.size() << " requests bars from API for" << symbol;
    
    // Loop through all the pendingAsyncGetBarRequests in order
    for (auto it = pendingAsyncGetBarRequests.constBegin(); it != pendingAsyncGetBarRequests.constEnd(); ++it) {
        Q_ASSERT(it.value().fulfilled == true); // Should be fulfilled
        const QVector<Bar>& barsFromAPI = it.value().bars;

        QVector<Bar> barsFromAPIWithHolesFilled = fillHolesOfReceivedRequest(it.value().first, it.value().last, barsFromAPI);

        storeBarsInCache(barsFromAPIWithHolesFilled);
        storeBarsInDatabase(barsFromAPIWithHolesFilled); // TODO optimize by batching inserts later
    }

    // At this point, we have received and stored all the holes-filled bars requested from the API.

    // Clear pending requests not so that we can accept new getBars() calls
    pendingAsyncGetBarRequests.clear();

    // Now retrieve all bars for the original requested range from cache (memory + database), but should be a complete memory hit
    QVector<Bar> allFetchedBars = getBars(savedFirst, savedLast); // TODO assert that it was a complete memory hit (not even fetching from database)

    Q_ASSERT_X(!allFetchedBars.empty(), qPrintable(cacheName), "After fetching missing bars from API, still do not have complete set of bars in cache");
        
    qCDebug(BarCacheLog) << cacheName << " : Returning" << (lastHitType == HitType::PartialHit ? "PARTIAL HIT" : "MISS");
    
    emit receivedAsyncGetBars(allFetchedBars);
}

/*
 * Fetch bars for the given datetime range.
 *
 * @return 
 *   - If the returned vector is not empty, then it means all the requested bars were in cache (either memory or database)
 *   - If the vector is empty, it means that the cache had to send async requests for the missing bars ranges and will emit a receivedAsyncGetBars(QVector<Bar> bars) signal later 
 */
const QVector<Bar> BarCache::getBars(const QDateTime &first, const QDateTime &last) {
    const QString cacheName = this->objectName();

    // FIXME : allow multiple concurrent getBars requests later
    Q_ASSERT_X(pendingAsyncGetBarRequests.isEmpty(), qPrintable(cacheName), "getBars() called while there are still pending async getBars requests. Please wait for those to complete before making new requests.");
    Q_ASSERT(first.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(last.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(first < last);

    qCDebug(BarCacheLog) << cacheName << "getBars() called for range" << first << "to" << last;
         
    QVector<QPair<QDateTime, QDateTime>> tradingRanges = splitIntoTradingDayRanges(first, last);
 
    QVector<Bar> allBars;
    
    bool missingAtLeastOneRange = false;

    for (const auto& range : tradingRanges) {
        QVector<Bar> rangeBars = getBarsInRange(range.first, range.second);
        allBars.append(rangeBars);
        if (rangeBars.isEmpty()) {
            missingAtLeastOneRange = true;
        }
    }

    if (missingAtLeastOneRange) {
        savedFirst = first;
        savedLast = last;
    }
       
    return missingAtLeastOneRange ? QVector<Bar>() : allBars;
 }

const QVector<Bar> BarCache::getBarsInRange(const QDateTime &first, const QDateTime &last)
{
    const QString cacheName = this->objectName();

    qCDebug(BarCacheLog) << cacheName << "getBarsInRange() called for range" << first << "to" << last;

    // Original assertions for single-day requests
    Q_ASSERT_X(first.date() == last.date(), qPrintable(cacheName), "getBarsInRange() only supports single-day ranges");
    Q_ASSERT_X(first.date().dayOfWeek() >= MONDAY && first.date().dayOfWeek() <= FRIDAY, qPrintable(cacheName), "getBars() called with date outside Monday-Friday range");
    Q_ASSERT_X(first.time() >= QTime(TRADING_START_HOUR, 0, 0), qPrintable(cacheName), "Fetching bars before 6am"); // Tradestation bars start at 6
    Q_ASSERT_X(last.time() <= QTime(TRADING_END_HOUR, 0, 0), qPrintable(cacheName), "Fetching bars after 8pm");

    lastHitType = HitType::None;

    // Reset the counter of fetched bar for the last request
    lastNumberFetchedBars = 0; //TODO
    
    // Check memory cache first
    QVector<Bar> cachedBars = getBarsFromCache(first, last);
    
    // complete HIT : If we have a complete set, return it
    if (cachedBars.size() == (first.secsTo(last) / 60) + 1) {
        lastHitType = HitType::Hit;

        qCDebug(BarCacheLog) << cacheName << " : Returning complete HIT from memory cache";

        return cachedBars;
    }

    qCDebug(BarCacheLog) << cacheName << " : partial hit or complete miss memory-cache, check database for missing bars";

    // Load missing bars from database
    for (const auto& range : identifyMissingRanges(first, last, cachedBars)) {
        Q_ASSERT(range.first.timeZone() == QTimeZone("America/New_York"));
        Q_ASSERT(range.second.timeZone() == QTimeZone("America/New_York"));
        Q_ASSERT(range.first <= range.second); // Could be equal if asking just one bar

        qCDebug(BarCacheLog) << cacheName << "Will fetch from Database for range" << range.first << "to" << range.second;

        QVector<Bar> dbBars = getBarsFromDatabase(range.first, range.second);
        if (dbBars.isEmpty()) {
            qCDebug(BarCacheLog) << cacheName << "No bars found in database for" << symbol << "in range" << range.first << "to" << range.second;
        } else {
            qCDebug(BarCacheLog) << cacheName << "Loaded" << dbBars.size() << "bars from database into memory cache for" << symbol << "in range" << range.first << "to" << range.second;
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

    qCDebug(BarCacheLog) << cacheName << " : still partial hit or complete miss after database load, fetch from API";

    // Fetch missing bars from API for each range
    for (const auto& range : identifyMissingRanges(first, last, cachedBars)) {
        Q_ASSERT(range.first.timeZone() == QTimeZone("America/New_York"));
        Q_ASSERT(range.second.timeZone() == QTimeZone("America/New_York"));
        Q_ASSERT(range.first <= range.second);

        // FIXME this is likely the source of why we often fetch one bar too far
        QDateTime fetchLast = range.second.addSecs(60); // Need to add a minute because the API bounds are excluding the last minute
            
        qCDebug(BarCacheLog) << cacheName << "Fetching bars from API for" << symbol << "in range" << range.first << "to" << fetchLast;

        size_t requestID = TSClient::getInstance()->getBars(symbol, 1, Bar::BarUnit::Minute, 0, Bar::BarSessionTemplate::USEQ24Hour, range.first, fetchLast);

        Q_ASSERT(requestID != 0);

        Q_ASSERT(!m_asyncReqIdToSymbol.contains(requestID));

        m_asyncReqIdToSymbol[requestID] = symbol;

        PendingAsyncGetBarRequest pendingRequest;
        pendingRequest.first = range.first;
        pendingRequest.last = range.second;
        pendingRequest.fulfilled = false;
        pendingRequest.bars = QVector<Bar>();

        pendingAsyncGetBarRequests.insert(requestID, pendingRequest);
    }

    return QVector<Bar>(); // Indicate that async requests have been sent; result will come via signal later
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
}

QVector<Bar> BarCache::getBarsFromCache(QDateTime start, QDateTime end) const {
    QString cacheName = this->objectName();

    QVector<Bar> result;
    QVector<QDateTime> missingTimes;
    bool hasCompleteSet = true;
    qCDebug(BarCacheLog) << cacheName << "Checking in-memory cache for" << symbol
                         << "from" << start << " to " << end;
    


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
    } else {
        qCDebug(BarCacheLog) << cacheName << "Cache MISS - no bars found for" << symbol;
    }
    
    return result;
}

void BarCache::onReceivedNewLiveBar(QString symbol, Bar newBar)
{
    QString cacheName = this->objectName();

    Q_ASSERT_X(this->symbol == symbol, qPrintable(cacheName), "Mismatch in symbol");

    newBar.ajustTimeStampToOpeningMinute();

    storeBarInCache(newBar);

    emit receivedNewBar(symbol, newBar);
}

void BarCache::onStreamError(Stream::ErrorStatus error, QString errorMessage)
{
    QString cacheName = this->objectName();

    qCWarning(BarCacheLog) << cacheName << "Stream error occurred - Error:" << static_cast<int>(error)
                           << "Message:" << errorMessage;

    // Log specific error types for better diagnostics
    switch (error) {
        case Stream::ErrorStatus::Timeout:
            qCWarning(BarCacheLog) << cacheName << "Stream timeout - no data or heartbeat received";
            break;
        case Stream::ErrorStatus::InvalidSymbol:
            qCCritical(BarCacheLog) << cacheName << "Invalid symbol error - this should not happen";
            break;
        case Stream::ErrorStatus::DualLogon:
            qCCritical(BarCacheLog) << cacheName << "Dual logon detected - another session may be active";
            break;
        case Stream::ErrorStatus::GoAway:
            qCWarning(BarCacheLog) << cacheName << "Server requested stream closure";
            break;
        case Stream::ErrorStatus::InternalServerError:
            qCCritical(BarCacheLog) << cacheName << "Internal server error";
            break;
        case Stream::ErrorStatus::BadRequest:
            qCCritical(BarCacheLog) << cacheName << "Bad request error";
            break;
        case Stream::ErrorStatus::Unknown:
            qCCritical(BarCacheLog) << cacheName << "Unknown stream error";
            break;
    }

    // Note: Stream errors in BarCache are logged but not automatically recovered
    // The stream is in error state and will need to be recreated by closing and
    // reopening the BarCache if recovery is needed
}

QVector<Bar> BarCache::getBarsFromDatabase(QDateTime start, QDateTime end) const {
    QString cacheName = this->objectName();
    QVector<Bar> bars;

    Q_ASSERT_X(db.isOpen(), qPrintable(objectName()), "Database must be open for reading bars");

    qCDebug(BarCacheLog) << cacheName << "Checking database cache for" << symbol
                         << "from" << start << " to " << end;
    
    QSqlQuery query(db);
    query.prepare("SELECT timestamp, open, high, low, close, volume FROM bars WHERE timestamp >= ? AND timestamp <= ? ORDER BY timestamp");
    query.addBindValue(start.toSecsSinceEpoch());
    query.addBindValue(end.toSecsSinceEpoch());

    if (query.exec()) {
        while (query.next()) {
            QDateTime ts = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
            ts.setTimeZone(QTimeZone("America/New_York"));
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

QVector<QPair<QDateTime, QDateTime>> BarCache::identifyMissingRanges(const QDateTime &start, const QDateTime &end, const QVector<Bar>& cachedBars) const {
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

QVector<QPair<QDateTime, QDateTime>> BarCache::splitIntoTradingDayRanges(const QDateTime &first, const QDateTime &last) const {
    const QString cacheName = this->objectName();
    QVector<QPair<QDateTime, QDateTime>> ranges;

    Q_ASSERT(first.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(last.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(first < last);
      
    QDate currentDate = first.date();
    QDate endDate     = last.date();
    
    qCDebug(BarCacheLog) << cacheName << "Splitting range into trading days from" << first << "to" << last;
    
    while (currentDate <= endDate) {
        // Skip weekends
        if (currentDate.dayOfWeek() > FRIDAY) {
            currentDate = currentDate.addDays(1);
            continue;
        }
        
        // Define valid trading hours for this date (6AM to 8PM)
        QDateTime dayStart = QDateTime(currentDate, QTime(TRADING_START_HOUR, 0, 0), QTimeZone("America/New_York"));
        QDateTime dayEnd   = QDateTime(currentDate, QTime(TRADING_END_HOUR, 0, 0),   QTimeZone("America/New_York"));
        
        // Determine actual start and end for this day
        QDateTime rangeStart = (currentDate == first.date()) ? first : dayStart;
        QDateTime rangeEnd   = (currentDate == last.date())  ? last : dayEnd;
        
        // Ensure range is within trading hours
        Q_ASSERT(rangeStart.time() >= QTime(TRADING_START_HOUR, 0, 0));
        Q_ASSERT(rangeEnd.time() <= QTime(TRADING_END_HOUR, 0, 0));
        
        ranges.append(qMakePair(rangeStart, rangeEnd));
        qCDebug(BarCacheLog) << cacheName << "  Adding range:" << rangeStart << "to" << rangeEnd;
        
        currentDate = currentDate.addDays(1);
    }
    
    return ranges;
}
