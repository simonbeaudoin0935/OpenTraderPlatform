#include <QTimeZone>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFutureSynchronizer>

#include "BarCache.h"
#include "TSClient.h"
#include "Settings.h"

Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")

#define DEBUG qCDebug(BarCacheLog) << this->objectName()

BarCache::BarCache(const QString &symbol, bool isStreaming, QObject *parent):
    QObject(parent),
    m_symbol(symbol),
    m_isStreaming(isStreaming)
{
    Q_ASSERT(parent != nullptr);
    
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
    m_db = QSqlDatabase::addDatabase("QSQLITE", "BarCache_" + symbol);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        qFatal("Failed to open database for %s: %s", qPrintable(symbol), qPrintable(m_db.lastError().text()));
    } else {
        if (dbFileExisted) {
            qCInfo(BarCacheLog) << "Opened existing database for symbol" << symbol << "at" << dbPath;
        } else {
            qCInfo(BarCacheLog) << "Created new database for symbol" << symbol << "at" << dbPath;
        }
        
        // Create table if not exists
        QSqlQuery query(m_db);
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
        m_streamBar = TSClient::getInstance()->openStreamBars(symbol,
                                                           1,
                                                           Bar::BarUnit::Minute,
                                                           2,
                                                           Bar::BarSessionTemplate::USEQ24Hour);
        Q_ASSERT(m_streamBar != nullptr);

        connect(m_streamBar, &StreamBars::receivedNewBar, this, &BarCache::onReceivedNewLiveBar);
        connect(m_streamBar, &Stream::streamErrorOccurred, this, &BarCache::onStreamError);
    }
}

BarCache::~BarCache()
{
    const QString cacheName = this->objectName();

    if (m_streamBar != nullptr) {
        TSClient::getInstance()->closeStreamBars(m_streamBar);
    }

    qCDebug(BarCacheLog) << cacheName << "Destroyed";

    Q_ASSERT(false); // TODO clean up database connection properly
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

/*
 * Fetch bars for the given datetime range.
 * 
 * @note : Both date-times must be in America/New_York timezone
 *
 * This function breaks the requested range into trading day ranges, in case the range spans multiple days.
 * For each trading day range, it attempts to retrieve bars from the in-memory cache first. If some bars are missing, it checks the local database.
 * If bars are still missing after checking the database, it sends all the asynchronous requests needed to complete the set of bars in the future
 * 
 * @return 
 *   - QVector<Bar> if the bars were found in cache (memory and/or database)
 *   - QFuture<QVector<Bar>> if some bars are missing and async requests were sent to fetch them from the API
 * 
 */
BarCache::GetBarsResult_t BarCache::getBars(const QDateTime &first, const QDateTime &last) {
    DEBUG << "getBars() called for range" << first << "to" << last;

    #warning allow multiple concurrent getBars requests later, use invoke method auto
    Q_ASSERT(first.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(last.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(first < last);

    // Accumulate all bars until we find a missing range, then we stop accumulating.
    QVector<Bar> allBars;

    // As we accumulate bars for each trading day range, we also keep track of any missing ranges that require async fetching.
    // At the end, if this vector is empty, it means we had a complete hit and can return allBars.
    QVector<QFuture<QVector<Bar>>> futuresOrMissingRanges;

    // Try to get bars for every trading day range.
    for (const auto& range : splitIntoTradingDayRanges(first, last)) {

        // If all the bars are found for this range, we get a QVector<Bar>.
        // If there were missing bars that need to be fetched from the API,
        // we will get a QFuture<QVector<Bar>> for that range.
        GetBarsResult_t rangeBars = getBarsInRange(range.first, range.second);
        
        if (std::holds_alternative<QFuture<QVector<Bar>>>(rangeBars)) {
            // This range of bars will require waiting for the API
            futuresOrMissingRanges.append(std::move(std::get<QFuture<QVector<Bar>>>(rangeBars)));
        } else {
            if (futuresOrMissingRanges.isEmpty()) {
                // If there are already missing ranges being fetched async,
                // Theres no purpuse to waste copying the fetches ranges because we won't return
                // the allBars anyway. Only if there are no missing ranges, we keep on accumulating
                // the bars and hope that no missing range show up. If one show up, we still continue
                // probing all ranges so that we can send all the async requests needed to fetch all the
                // ranges needed. 
                allBars.append(std::move(std::get<QVector<Bar>>(rangeBars)));
            }
        }
    }

    if (!futuresOrMissingRanges.isEmpty()) {
        savedFirst = first;
        savedLast = last;

        DEBUG << " : Returning MISS/PARTIAL HIT, waiting for async fetches to complete";

        Q_ASSERT_X(false, qPrintable(this->objectName()), "TODO implement combining multiple futures into one and acting on it");

    } else {
        DEBUG << " : Returning complete HIT from cache/database";
        return allBars;
    }
}

BarCache::GetBarsResult_t BarCache::getBarsInRange(const QDateTime &first, const QDateTime &last)
{
    DEBUG << "getBarsInRange() called for range" << first << "to" << last;

    Q_ASSERT(first.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(last.timeZone() == QTimeZone("America/New_York"));

    // Original assertions for single-day requestsk
    const QString cacheName = this->objectName();
    Q_ASSERT_X(first.date() == last.date(), qPrintable(cacheName), "getBarsInRange() only supports single-day ranges");
    Q_ASSERT_X(first.date().dayOfWeek() >= MONDAY && first.date().dayOfWeek() <= FRIDAY, qPrintable(cacheName), "getBars() called with date outside Monday-Friday range");
    Q_ASSERT_X(first.time() >= QTime(TRADING_START_HOUR, 0, 0), qPrintable(cacheName), "Fetching bars before 6am"); // Tradestation bars start at 6
    Q_ASSERT_X(last.time() <= QTime(TRADING_END_HOUR, 0, 0), qPrintable(cacheName), "Fetching bars after 8pm");

   
    // Check memory cache first
    QVector<Bar> cachedBars = getBarsFromCache(first, last);
    
    // complete HIT : If we have a complete set, return it
    if (cachedBars.size() == (first.secsTo(last) / 60) + 1) {
        DEBUG << " : Returning complete HIT from memory cache";
        return cachedBars;
    }

    if (cachedBars.isEmpty()) {
        DEBUG << " : miss in memory-cache, check database for missing bars";
    } else {
        DEBUG << " : partial hit in memory-cache, check database for missing bars";
    }
    

    // Load missing bars from database
    for (const auto& range : identifyMissingRanges(first, last, cachedBars)) {
        Q_ASSERT(range.first <= range.second); // Could be equal if asking just one bar

        DEBUG << "Will fetch from Database for range" << range.first << "to" << range.second;

        QVector<Bar> dbBars = getBarsFromDatabase(range.first, range.second);
        if (dbBars.isEmpty()) {
            DEBUG << " : miss in database in range" << range.first << "to" << range.second;
        } else {
            DEBUG << "Loaded" << dbBars.size() << "bars from database into memory cache in range" << range.first << "to" << range.second;
            storeBarsInCache(dbBars);
        }
    }
        
    // Check memory cache again after loading from database
    cachedBars = getBarsFromCache(first, last);
        
    // complete HIT after database load : If we have a complete set now, return it
    if (cachedBars.size() == (first.secsTo(last) / 60) + 1) {
        DEBUG << " : Returning HIT (after database load)";
        return cachedBars;
    }

    if (!cachedBars.isEmpty()) {
        DEBUG << " : partial hit in cache/database, fetch from API for missing bars";
    } else {
        DEBUG << " : miss in cache/database, fetch from API for missing bars";
    }

    // At this point, we still have missing bars after checking both memory cache and database
    // For algorithmic simplicity, we will fetch the whole first@last range from API, even if some bars may likely be already in cache/database,
    // as to only send one API request for that day range.
    qCDebug(BarCacheLog) << cacheName << "Fetching bars from API in range" << first << "to" << last;


    QFuture<QVector<Bar>> future = TSClient::getInstance()->getBars(m_symbol,
                                                                    1,
                                                                    Bar::BarUnit::Minute,
                                                                    0,
                                                                    Bar::BarSessionTemplate::USEQ24Hour,
                                                                    first,
                                                                    last.addSecs(60)); // Need to add a minute because the API bounds are excluding the last minute
                                                                    // FIXME the addSecs(60) is likely the source of why we often fetch one bar too far

    future.then([this, first, last](QVector<Bar> bars){
        DEBUG << "Asynchronous getBars() from API completed for range" << first << "to" << last
              << "with" << bars.size() << "bars received";

        QVector<Bar> barsFromApiHolesFilled = fillHolesOfReceivedRequest(first, last, bars);
        // Handle the received bars


    }).onFailed([this, first, last](QException exception){
        DEBUG << "Asynchronous getBars() from API FAILED for range" << first << "to" << last
              << "with error message:" << exception.what();

        Q_ASSERT(false); // "TODO handle failed async getBars request");
    });
 

    return future; // Indicate that async requests have been sent; result will come via signal later
}




void BarCache::storeBarInCache(const Bar& bar) {
    QString cacheName = this->objectName();

    QDateTime dateTime = bar.getTimeStamp();

    if (!bar.getIsRealtime()) {
        if (m_barCacheOneMinute.contains(dateTime)) {
            qCWarning(BarCacheLog) << cacheName << "Bar already in cache for" << m_symbol
                                 << "at " << dateTime;
            m_duplicateStoreCount++;
        }
    } else {
        if (bar.getBarStatus() == Bar::BarStatus::Closed) {
            qCDebug(BarCacheLog) << cacheName << "Real time cache insertion; received the closing bar";
        }
    }

    {
        QWriteLocker locker(&m_barCacheOneMinuteRwLock);
        m_barCacheOneMinute.insert(dateTime, bar);
    }

    qCDebug(BarCacheLog) << cacheName << "Inserted bar in cache with timestamp :" << bar.getTimeStamp();
}

void BarCache::storeBarsInCache(const QVector<Bar>& bars) {
    for (const Bar& bar : bars) {
        storeBarInCache(bar);
    }
}

QVector<Bar> BarCache::getBarsFromCache(QDateTime start, QDateTime end) const
{
    DEBUG << "Checking in-memory cache from" << start << " to " << end;

    QVector<Bar> result;
 
    size_t numMissingBars = 0;
    
    // Check for complete set and identify missing bars
    {
        QReadLocker locker(&m_barCacheOneMinuteRwLock);

        // Find the first bar at or after start
        auto it = m_barCacheOneMinute.lowerBound(start);

        for (QDateTime expectedTime = start; expectedTime <= end; expectedTime = expectedTime.addSecs(60)) {
            if (it == m_barCacheOneMinute.end() || it.key() != expectedTime) {
                numMissingBars++;
                DEBUG << "Cache MISS - missing bar at" << expectedTime;
            } else {
                result.append(it.value());
                ++it;
            }
        }
    }
    
    if (numMissingBars == 0) {
        DEBUG << "Cache HIT - found complete set of bars for" << m_symbol;
    } else if (!result.isEmpty()) {
        DEBUG << "Cache PARTIAL HIT - found" << result.size() << "bars, missing" << numMissingBars << "bars";
    } else {
        DEBUG << "Cache MISS - no bars found for" << m_symbol;
    }
    
    return result;
}

void BarCache::onReceivedNewLiveBar(QString symbol, Bar newBar)
{
    QString cacheName = this->objectName();

    Q_ASSERT_X(m_symbol == symbol, qPrintable(cacheName), "Mismatch in symbol");

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

    Q_ASSERT_X(m_db.isOpen(), qPrintable(objectName()), "Database must be open for reading bars");

    qCDebug(BarCacheLog) << cacheName << "Checking database cache for" << m_symbol
                         << "from" << start << " to " << end;
    
    QSqlQuery query(m_db);
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
        qCInfo(BarCacheLog) << "Loaded" << bars.size() << "bars from database for" << m_symbol;
    } else {
        qCWarning(BarCacheLog) << "Database query failed for" << m_symbol << ":" << query.lastError().text();
    }
    return bars;
}

void BarCache::storeBarsInDatabase(const QVector<Bar>& bars) {
    Q_ASSERT_X(m_db.isOpen(), qPrintable(objectName()), "Database must be open for storing bars");
   
   
    Q_ASSERT(bars.isEmpty() == false);

    qCDebug(BarCacheLog) << "Storing" << bars.size() << "bars in database for" << m_symbol;
    QSqlQuery query(m_db);
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
            qCWarning(BarCacheLog) << "Failed to store bar in database for" << m_symbol
                                   << "at" << bar.getTimeStamp().toString() << ":"
                                   << query.lastError().text();
        }
    }

    qCInfo(BarCacheLog) << "Successfully stored" << storedCount << "bars in database for" << m_symbol;
}

void BarCache::clearDatabase() {
    Q_ASSERT_X(m_db.isOpen(), qPrintable(objectName()), "Database must be open for clearing");
    
    qCInfo(BarCacheLog) << "Clearing all bars from database for" << m_symbol;
    
    QSqlQuery query(m_db);
    if (query.exec("DELETE FROM bars")) {
        qCInfo(BarCacheLog) << "Successfully cleared database for" << m_symbol;
    } else {
        qCWarning(BarCacheLog) << "Failed to clear database for" << m_symbol << ":" << query.lastError().text();
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
