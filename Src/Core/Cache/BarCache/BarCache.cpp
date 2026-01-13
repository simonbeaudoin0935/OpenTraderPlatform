#include <QTimeZone>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFutureSynchronizer>
#include <QtConcurrent>
#include <memory>

#include "MainApp.h"
#include "BarCache.h"
#include "TSClient.h"
#include "Settings.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY BarCacheLog
Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")

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
    
    DEBUG << "Cache location:" << cacheLocation;
    DEBUG << "Using database file:" << dbPath;
    DEBUG << "Database file existed:" << dbFileExisted;

    QFileInfo dbInfo(dbPath);
    DEBUG << "Database directory:" << dbInfo.absolutePath()
          << "Dir exists:" << dbInfo.dir().exists()
          << "File readable:" << dbInfo.isReadable()
          << "File writable:" << dbInfo.isWritable();

    // Create the directory if it doesn't exist
    if (!dbInfo.dir().exists()) {
        if (!dbInfo.dir().mkpath(".")) {
            qFatal("Failed to create cache directory: %s", qPrintable(dbInfo.absolutePath()));
        }
        INFO << "Created cache directory:" << dbInfo.absolutePath();
    }

    INFO << "SQLite connection name to be used:" << ("BarCache_" + symbol);

    m_db = QSqlDatabase::addDatabase("QSQLITE", "BarCache_" + symbol);
    m_db.setDatabaseName(dbPath);

    m_db.open();

    OBJ_ASSUME_TRUE(m_db.isOpen());

    if (dbFileExisted) {
        INFO << "Opened existing database for symbol" << symbol << "at" << dbPath;
    } else {
        INFO << "Created new database for symbol" << symbol << "at" << dbPath;
    }
        
    // Create table if not exists
    QSqlQuery query(m_db);
    query.exec("CREATE TABLE IF NOT EXISTS bars ("
               "date TEXT, "
               "[index] INTEGER, "
               "open REAL, "
               "high REAL, "
               "low REAL, "
               "close REAL, "
               "volume INTEGER, "
               "status INTEGER DEFAULT 0, "
               "PRIMARY KEY (date, [index]))");

    if (query.lastError().isValid()) {
        CRITICAL << "Failed to create table:" << query.lastError().text();

        Q_ASSERT(false);
    }

    if (isStreaming) {
        startStream();
    }
}


void BarCache::startStream(){
    DEBUG << "Starting bars stream for symbol " << m_symbol;

    m_stream = TSClient::getInstance()->openStreamBars(m_symbol,
                                                       1,
                                                       Bar::BarUnit::Minute,
                                                       3,
                                                       Bar::BarSessionTemplate::USEQ24Hour);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamBars::newBarReceived, this, &BarCache::onReceivedNewLiveBar);

    m_stream->future().then(this,
        [this](std::optional<QString> error)
        {
            if (error.has_value()){
                CRITICAL << "Bars cache bar future failed for" << m_symbol
                         << "- Exception:" << error.value();

                CRITICAL << "Restarting bars stream for symbol " << m_symbol;

                // Turns out closeStream() is already done implicitely when the TSClient handles the stream error
                // TSClient::getInstance()->closeStream(m_stream);

                startStream();
            } else {
                CRITICAL << "Bars cache bar future finished, which should not happen";
                Q_UNREACHABLE();
            }
        }
    );
}

BarCache::~BarCache()
{
    if (m_isStreaming) {
        Q_CHECK_PTR(m_stream);
        
        TSClient::getInstance()->closeStream(m_stream);
    }

    DEBUG << "Destroyed";

    // Close database connection
    m_db.close();

    // TODO deal with scenario where we would destroy a barcache
    Q_UNREACHABLE();
}

/**
 * @brief Converts a QTime timestamp to the corresponding index in the daily bar cache vector.
 *
 * TradeStation timestamps 1-minute bars using the **closing time** of the interval.
 * For extended hours trading (6:00 AM – 8:00 PM ET):
 *   - The first bar (covering 6:00:00 – 6:00:59) is timestamped 6:01 AM
 *   - The last bar  (covering 7:59:00 – 7:59:59) is timestamped 8:00 PM
 *
 * Therefore, valid bar timestamps range from 6:01 AM to 8:00 PM inclusive.
 * The cache vector is pre-allocated with exactly 840 elements (14 hours × 60 minutes),
 * where index 0 corresponds to the 6:01 AM bar and index 839 to the 8:00 PM bar.
 *
 * @param time The timestamp of the bar (must be a valid bar close time)
 * @return size_t The zero-based index in the daily cache vector (0 to 839)
 *
 * @pre time is a valid 1-minute bar close time in extended hours:
 *      - 6:01 AM ≤ time ≤ 8:00 PM
 * @note The function asserts on invalid inputs in debug builds.
 */
size_t BarCache::timeToIndex(const QTime& time)
{
    // Validate: must be between 6:01 AM and 8:00 PM inclusive
    ASSUME_GTE(time, TRADING_START_TIME);
    ASSUME_LTE(time, TRADING_END_TIME);

    // Total minutes since 6:00 AM
    size_t minutesSince6AM = (time.hour() - TRADING_START_TIME.hour()) * 60 + time.minute();

    // Adjust by -1 because first bar is timestamped at 6:01 (index 0)
    size_t index = minutesSince6AM - 1;

    ASSUME_LT(index, BARS_PER_DAY); // 0 ≤ index ≤ 839

    return index;
}

/**
 * @brief Converts a daily bar cache index to the corresponding bar timestamp (QTime).
 *
 * The timestamp returned is the **close time** of the 1-minute bar, as used by TradeStation.
 * For extended hours (6:00 AM – 8:00 PM ET):
 *   - index 0   → 6:01 AM  (bar covering 6:00:00 – 6:00:59)
 *   - index 839 → 8:00 PM (bar covering 7:59:00 – 7:59:59)
 *
 * @param index Zero-based index in the daily cache vector (0 to 839)
 * @return QTime The timestamp (close time) of the bar
 *
 * @pre index < BARS_PER_DAY (840)
 * @note Returned times are always valid bar timestamps: 6:01 AM to 8:00 PM inclusive.
 */
QTime BarCache::indexToTime(size_t index)
{
    ASSUME_LT(index, BARS_PER_DAY);

    // Add 1 to offset the fact that index 0 = 6:01, not 6:00
    size_t adjustedMinutes = index + 1;

    int hour = TRADING_START_TIME.hour() + (adjustedMinutes / 60);
    int minute = adjustedMinutes % 60;

    // At index 839: adjustedMinutes = 840 → 840 / 60 = 14 hours → 6 + 14 = 20 (8 PM), minute = 0
    return QTime(hour, minute, 0);
}

// Get or create a day vector for a given date
QVector<Bar>& BarCache::getOrCreateDayVector(const QDate& date)
{
    ASSUME_GTE(date.dayOfWeek(), Qt::Monday);
    ASSUME_LTE(date.dayOfWeek(), Qt::Friday);

    // NOTE: Caller must already hold m_barCacheRwLock write lock
    
    if (!m_barCacheByDay.contains(date)) {
        DEBUG << "m_barCacheByDay map doesn't contain day " << date << ", creating new vector.";

        // Pre-allocate to full capacity with uninitialized bars by default ctor of Bar
        m_barCacheByDay[date] = QVector<Bar>(BARS_PER_DAY);
    }

    return m_barCacheByDay[date];
}

QVector<Bar> BarCache::fillHolesOfReceivedRequest(const QDateTime& first, const QDateTime& last, const QVector<Bar>& barsFromAPI) const
{   
    OBJ_ASSUME_EQUAL(first.date(), last.date());
    OBJ_ASSUME_GTE(first.date().dayOfWeek(), Qt::Monday);
    OBJ_ASSUME_LTE(first.date().dayOfWeek(), Qt::Friday);
    OBJ_ASSUME_GTE(first.time(), TRADING_START_TIME);
    OBJ_ASSUME_LTE(last.time(), TRADING_END_TIME);

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
        DEBUG << "Created" << voidBarsCreated << "void bars to account for periods with no trading activity";
    }

    return resultBars;
}

/**
 * @brief Retrieves bars for a specific date and time range.
 *
 * This function fetches 1-minute bars for the given symbol within the specified date and time range.
 * It follows a hierarchy: first checks the in-memory cache, then the database, and finally fetches
 * from the API if necessary. The entire day is cached once fetched to ensure completeness.
 *
 * For the current day, bars are fetched up to the current time. For past days, the full trading day
 * (6:01 AM to 8:00 PM ET) is retrieved.
 *
 * @param date The date for which to retrieve bars (must be a weekday: Monday to Friday).
 * @param first The start time of the range (must be between 6:01 AM and 8:00 PM ET).
 * @param last The end time of the range (must be between 6:01 AM and 8:00 PM ET, and >= first).
 *
 * @return A QFuture containing a std::unique_ptr to a QVector<Bar> with the requested bars.
 *
 * @pre date is a weekday (Monday to Friday).
 * @pre first and last are within trading hours (6:01 AM to 8:00 PM ET).
 * @pre date is not in the future; for current day, last <= current time.
 * @pre first <= last.
 * @pre first and last have seconds and milliseconds set to 0.
 *
 * @note The function may perform asynchronous operations to fetch data from the API.
 * @note Holes in the data (periods with no trading activity) are filled with null bars.
 * @warning Currently returns the complete day, not just the requested range (bug noted in code).
 */
BarCache::GetBarsResult_t BarCache::getBars(const QDate &date, const QTime &first, const QTime &last) const
{
    const QDateTime now = MainApp::getCurrentAppTime();
    const bool isCurrentDay = (date == now.date());

    // The smaller increment in bar time is 1 minute, so we can safely assume seconds and milliseconds are zero
    OBJ_ASSUME_EQUAL(first.second(), 0);
    OBJ_ASSUME_EQUAL(last.second(), 0);
    
    OBJ_ASSUME_EQUAL(first.msec(), 0);
    OBJ_ASSUME_EQUAL(last.msec(), 0);

    // Assume monday-friday and between 6:01am-8:00pm
    OBJ_ASSUME_GTE(date.dayOfWeek(), Qt::Monday);
    OBJ_ASSUME_LTE(date.dayOfWeek(), Qt::Friday);
    OBJ_ASSUME_GTE(first, TRADING_START_TIME);
    OBJ_ASSUME_LTE(last, TRADING_END_TIME);

    // Assume we are not requesting future dates/times
    OBJ_ASSUME_LTE(date, now.date()); // Can't request bars for future dates
    if (isCurrentDay) {
        OBJ_ASSUME_LTE(last, now.time()); // Can't request bars for later today than now
    }

    DEBUG << "getBarsInDay() called for day" << date << "and time range" << first << "to" << last;

    // With the new "complete day or nothing" rule:
    // If the day exists in cache, we have ALL bars for that day (or all bars up to now for current day)
    if (std::optional<std::unique_ptr<QVector<Bar>>> barsFromCache = getBarsFromCache(date, first, last);
        barsFromCache.has_value())
    {
        DEBUG << "Returning bars from in-memory cache for day" << date;

        return std::move(barsFromCache.value());
    }
    
    DEBUG << " m_barCacheByDay map doesn't contain bars  for day " << date << "checking database";
    
    // For database, we check if we have the complete day (6:01am to 8:00pm)
    // Use shared_ptr so the promise can be accessed by both .then() and .onFailed() handlers
    // in nested async operations without moving/invalidating it
    auto promisePtr = std::make_shared<QPromise<std::expected<std::unique_ptr<QVector<Bar>>, TSClient::Error>>>();
    promisePtr->start(); // Must start the promise before it can be used
    QFuture<std::expected<std::unique_ptr<QVector<Bar>>, TSClient::Error>> future = promisePtr->future();

    // Explicitely ignore return value of QtConcurrent::run to avoid compiler warning about unused return value
    // The reason why we ignore the returned QFuture is because we, as the side invoking this function, do not need
    // to track the completion of this asynchronous task ourselves - we return our own QFuture we created above to
    // the caller instead since we moved over our QPromise to the lambda for signaling later.
    (void) QtConcurrent::run(
        [this, date, first, last, promisePtr]()
        {
            std::optional<std::unique_ptr<QVector<Bar>>> dbBars = getBarsFromDatabase(date, first, last);
            
            // Check if we got the complete day from database
            if (dbBars.has_value())
            {
                std::unique_ptr<QVector<Bar>>& dbBarsPtr = dbBars.value();
                const size_t expectedBarCount = first.secsTo(last) / 60 + 1;

                // Happy path, got bars from database. Make absolutely sure that the number of bars is correct
                // When using the getBarsFromDatabase(), if we get the vector back, it means we have the complete set of bars for the requested range
                OBJ_ASSUME_EQUAL(dbBarsPtr->size(), expectedBarCount);

                DEBUG << "Loaded complete day from database:" << dbBarsPtr->size() << "bars";
        
                // Store the complete day in cache
                storeBarsInCache(date, dbBarsPtr);

                promisePtr->addResult(std::move(dbBarsPtr));
                promisePtr->finish();

                return;
            }

            // Database doesn't have complete day - fetch from API
            DEBUG << " fetching from database failed";

            // Fetch the complete day (or up to now for current day) from API
            QDateTime startDateTime = QDateTime(date, TRADING_START_TIME, QTimeZone("America/New_York"));

            const QDateTime now = MainApp::getCurrentAppTime();
            const bool isCurrentDay = (date == now.date());

            QDateTime endDayTime = QDateTime(date,
                                             isCurrentDay ? now.time() : TRADING_END_TIME,
                                             QTimeZone("America/New_York"));

            DEBUG << "Fetching complete day from API:" << startDateTime << "to" << endDayTime;

            TSClient::getInstance()->getBars(
                m_symbol,
                1,
                Bar::BarUnit::Minute,
                0,
                Bar::BarSessionTemplate::USEQ24Hour,
                startDateTime,
                endDayTime
            ).then(const_cast<BarCache *>(this),
                [this, date, startDateTime, endDayTime, promisePtr]
                (std::expected<std::unique_ptr<QVector<Bar>>, TSClient::Error> bars)
                {
                    if (!bars.has_value()) {
                        CRITICAL << "getBars() from API returned error for" << m_symbol
                                 << "- Error:" << static_cast<int>(bars.error());
                        promisePtr->addResult(std::unexpected(bars.error()));
                    } else {
                        DEBUG << "Asynchronous getBars() from API completed for complete day" << date
                              << "with" << bars.value()->size() << "bars received";

                        std::unique_ptr<QVector<Bar>> barsFromApiHolesFilled = std::make_unique<QVector<Bar>>(fillHolesOfReceivedRequest(startDateTime, endDayTime, *bars.value()));

                        // Store the complete day in both cache and database
                        storeBarsInCache(date, barsFromApiHolesFilled);
                        storeBarsInDatabase(date, barsFromApiHolesFilled);

                        // #warning bug here, we must return only the requested range, not the complete day
                        //  For now we return the complete day - later we can slice to requested range only
                        promisePtr->addResult(std::move(barsFromApiHolesFilled));
                    }
                    promisePtr->finish();

                });
        });

    return future;
}


std::optional<std::unique_ptr<QVector<Bar>>>
BarCache::getBarsFromCache(const QDate &date, const QTime &start, const QTime &end) const
{
    QReadLocker locker(&m_barCacheRwLock);

    if (!m_barCacheByDay.contains(date))
    {
        DEBUG << "m_barCacheByDay map doesn't contain day " << date;
        return std::nullopt;
    }    

    DEBUG << "m_barCacheByDay map contains day " << date;

    // Day exists in cache - extract the requested range
    const QVector<Bar> &dayVector = m_barCacheByDay[date];

    // a day vector is always pre-allocated to 840 bars
    OBJ_ASSUME_EQUAL(dayVector.size(), BARS_PER_DAY);

    std::unique_ptr<QVector<Bar>> result = std::make_unique<QVector<Bar>>();
    result->reserve((start.secsTo(end) / 60) + 1);

    for (size_t index = timeToIndex(start); index <= timeToIndex(end); ++index) {
        if (dayVector[index].getBarStatus() == Bar::BarStatus::Uninitialized) {
            // Missing bar in cache - treat as cache miss
            DEBUG << "Bar at index" << index << "for day" << date << "is uninitialized in cache";
            return std::nullopt;
        }
        result->append(dayVector[index]);
    }

    DEBUG << "Loaded complete day from memory cache:" << date << "with" << result->size() << "bars";
    return std::move(result);
}

void BarCache::storeBarInCache(const Bar& bar)
{
    QDateTime dateTime = bar.getTimeStamp();
    QDate date = dateTime.date();
    QTime time = dateTime.time();

    // Calculate the index for this bar in the day's vector
    size_t index = timeToIndex(time);

    QWriteLocker locker(&m_barCacheRwLock);

    
    QVector<Bar> &dayVector = getOrCreateDayVector(date);

    
    // Check if we're overwriting an existing bar (only if the index existed before resize)
    if (bar.getIsRealtime()) {
        if (bar.getBarStatus() == Bar::BarStatus::Closed){
            DEBUG << "Real time cache insertion; received the closing bar";
        }
    } else {
        OBJ_ASSUME_TRUE(dayVector[index].getBarStatus() == Bar::BarStatus::Uninitialized);
    }

    // Store the bar at the appropriate index
    dayVector[index] = bar;

    DEBUG << "Inserted bar in cache at index" << index << "for timestamp:" << bar.getTimeStamp();
}


void BarCache::storeBarsInCache(const QDate& date, const std::unique_ptr<QVector<Bar>>& bars) const
{
    OBJ_ASSUME_FALSE(bars->isEmpty());
    OBJ_ASSUME_TRUE(bars->count() <= BARS_PER_DAY); // Max bars per day
    
    OBJ_ASSUME_EQUAL(bars->first().getTimeStamp().date(), bars->last().getTimeStamp().date());
    OBJ_ASSUME_EQUAL(bars->first().getTimeStamp().time(), TRADING_START_TIME);
    OBJ_ASSUME_LTE(bars->last().getTimeStamp().time(), TRADING_END_TIME);
    
    QWriteLocker locker(&m_barCacheRwLock);
            
    if (bars->size() == BARS_PER_DAY) {
        // Full day
        m_barCacheByDay.insert(date, *bars);

        DEBUG << "Inserted full day in cache for" << date;
    } else {

        OBJ_ASSUME_TRUE(m_barCacheByDay.contains(date));

        QVector<Bar> &dayVector = m_barCacheByDay[date];

        // Insert/overwrite bars into existing day vector
        for (const Bar &bar : *bars)
        {
            size_t index = timeToIndex(bar.getTimeStamp().time());
            dayVector[index] = bar;
        }

        DEBUG << "Inserted partial day in cache for" << date << "with" << bars->size() << "bars";
    }
    
}

void BarCache::onReceivedNewLiveBar(Bar newBar)
{
    storeBarInCache(newBar);

    emit receivedNewBar(m_symbol, newBar);
}

std::optional<std::unique_ptr<QVector<Bar>>>
BarCache::getBarsFromDatabase(QDate date, QTime start, QTime end) const
{
    OBJ_ASSUME_TRUE(m_db.isOpen());

    DEBUG << "Checking database cache for" << m_symbol << " at date " << date << "from" << start << " to " << end;
    
    size_t indexStart = timeToIndex(start);
    size_t indexEnd = timeToIndex(end);

    OBJ_ASSUME_LT(indexStart, indexEnd);

    QSqlQuery query(m_db);
    query.prepare("SELECT [index], open, high, low, close, volume, status FROM bars WHERE date = ? AND [index] >= ? AND [index] <= ? ORDER BY [index]");
    query.addBindValue(date.toString("yyyy-MM-dd"));
    query.addBindValue(static_cast<int>(indexStart));
    query.addBindValue(static_cast<int>(indexEnd));

    std::unique_ptr<QVector<Bar>> bars = std::make_unique<QVector<Bar>>();

    if (query.exec()) {
        while (query.next()) {
            int index = query.value(0).toInt();
            QTime time = indexToTime(static_cast<size_t>(index));
            QDateTime ts(date, time, QTimeZone("America/New_York"));
            double open = query.value(1).toDouble();
            double high = query.value(2).toDouble();
            double low = query.value(3).toDouble();
            double close = query.value(4).toDouble();
            qint64 volume = query.value(5).toLongLong();
            Bar::BarStatus status = static_cast<Bar::BarStatus>(query.value(6).toInt());
            
            Bar bar;
            // Use the stored status to determine if this is a null bar
            if (status == Bar::BarStatus::Null) {
                bar = Bar::nullBar(ts);
            } else {
                bar = Bar(ts, open, high, low, close, volume);
            }
            
            bars->append(bar);
        }
        INFO << "Loaded" << bars->size() << "bars from database for" << m_symbol;
    } else {
        WARNING << "Database query failed for" << m_symbol << ":" << query.lastError().text();
    }

    if (bars->size() != (indexEnd - indexStart + 1)) {
        DEBUG << "Database does not have complete set of bars for" << m_symbol
              << "on date" << date << "- expected"
              << (indexEnd - indexStart + 1) << "bars but got" << bars->size();
        return std::nullopt;
    }
    return std::move(bars);
}

void BarCache::storeBarsInDatabase(const QDate &date, const std::unique_ptr<QVector<Bar>> &bars) const
{
    OBJ_ASSUME_TRUE(m_db.isOpen());
    OBJ_ASSUME_FALSE(bars->isEmpty());

    DEBUG << "Storing" << bars->size() << "bars in database for" << m_symbol;
    
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO bars (date, [index], open, high, low, close, volume, status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

    int storedCount = 0;
    for (const Bar& bar : *bars) {
        QString dateStr = date.toString("yyyy-MM-dd");
        size_t index = timeToIndex(bar.getTimeStamp().time());
        query.addBindValue(dateStr);
        query.addBindValue(static_cast<int>(index));
        query.addBindValue(bar.getOpen());
        query.addBindValue(bar.getHigh());
        query.addBindValue(bar.getLow());
        query.addBindValue(bar.getClose());
        query.addBindValue(bar.getTotalVolume());
        query.addBindValue(static_cast<int>(bar.getBarStatus()));
        if (query.exec()) {
            storedCount++;
        } else {
            WARNING << "Failed to store bar in database for" << m_symbol
                    << "at" << bar.getTimeStamp().toString() << ":"
                    << query.lastError().text();
        }
    }

    INFO << "Successfully stored" << storedCount << "bars in database for" << m_symbol;
}

void BarCache::clearDatabase()
{
    OBJ_ASSUME_TRUE(m_db.isOpen());

    INFO << "Clearing all bars from database for" << m_symbol;
    
    QSqlQuery query(m_db);
    if (query.exec("DELETE FROM bars")) {
        INFO << "Successfully cleared database for" << m_symbol;
    } else {
        WARNING << "Failed to clear database for" << m_symbol << ":" << query.lastError().text();
    }
}

constexpr QVector<std::tuple<QDate, QTime, QTime>> BarCache::splitIntoTradingDayRanges(const QDateTime &first, const QDateTime &last) noexcept
{
    Q_ASSERT(first.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(last.timeZone() == QTimeZone("America/New_York"));
    Q_ASSERT(first < last);

    // Ensure range is within trading hours
    Q_ASSERT(first.time() >= TRADING_START_TIME);
    Q_ASSERT(last.time() <= TRADING_END_TIME);


    QVector<std::tuple<QDate, QTime, QTime>> ranges;

    for (QDate date = first.date(); date <= last.date(); date = date.addDays(1)) {

        // Skip weekends
        if (date.dayOfWeek() > Qt::Friday) {
            continue;
        }

        QTime dayTimeStart = (date == first.date()) ? first.time() : TRADING_START_TIME;
        QTime dayTimeEnd   = (date == last.date())  ? last.time()  : TRADING_END_TIME;

        std::tuple<QDate, QTime, QTime> range = std::make_tuple(date, dayTimeStart, dayTimeEnd);

        ranges.append(range);
    }

    return ranges;
}
