#include <QTimeZone>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QPromise>
#include <memory>

#include "MainApp.h"
#include "BarCache.h"
#include "DatabaseThread.h"
#include "TSClient.h"
#include "Settings.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY BarCacheLog
Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")

BarCache::BarCache(const QString& symbol, bool isStreaming, QObject* parent)
    : QObject(parent), m_symbol(symbol), m_isStreaming(isStreaming)
{
    OBJ_ASSUME_DIFF(parent, nullptr);

    this->setObjectName("BarCache::" + symbol);

    // Set up database path - one database file per symbol in Bars subdirectory
    QString cacheLocation = getCacheLocation();
    QString barsDir = cacheLocation + "/" + FileSystemConstants::BARS_CACHE_SUBDIR;

    // Create Bars directory if it doesn't exist
    QDir dir;
    if (!dir.exists(barsDir))
    {
        bool dirCreated = dir.mkpath(barsDir);
        ASSUME_TRUE(dirCreated);
        DEBUG << "Created Bars directory:" << barsDir;
    }

    m_dbPath = barsDir + "/" + symbol + ".db";

    DEBUG << "Cache location:" << cacheLocation;
    DEBUG << "Bars directory:" << barsDir;
    DEBUG << "Using database file:" << m_dbPath;

    // Open database via DatabaseThread (async, thread-safe)
    // We don't wait for the result here - operations will queue until ready
    DatabaseThread::getInstance()
        ->openDatabase(m_symbol, m_dbPath)
        .then(this,
              [this](bool success)
              {
                  if (success)
                  {
                      INFO << "Database opened successfully for" << m_symbol;
                  }
                  else
                  {
                      CRITICAL << "Failed to open database for" << m_symbol;
                  }
              });

    if (isStreaming)
    {
        startStream();
    }
}


void BarCache::startStream()
{
    DEBUG << "Starting bars stream for symbol " << m_symbol;

    m_stream = TSClient::getInstance()->openStreamBars(m_symbol,
                                                       1, /* Interval: 1 bar */
                                                       Bar::BarUnit::Minute,
                                                       0, /* Barsback: 0 bars */
                                                       Bar::BarSessionTemplate::USEQ24Hour);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamBars::newBarReceived, this, &BarCache::onReceivedNewLiveBar);

    m_stream->future().then(this,
                            [this](std::optional<QString> error)
                            {
                                if (error.has_value())
                                {
                                    CRITICAL << "Bars cache bar future failed for" << m_symbol
                                             << "- Exception:" << error.value();

                                    CRITICAL << "Restarting bars stream for symbol " << m_symbol;

                                    // Turns out closeStream() is already done implicitely when the TSClient handles the stream error
                                    // TSClient::getInstance()->closeStream(m_stream);

                                    startStream();
                                }
                                else
                                {
                                    CRITICAL << "Bars cache bar future finished, which should not happen";
                                    Q_UNREACHABLE();
                                }
                            });
}

BarCache::~BarCache()
{
    // Note: Stream cleanup is handled by TSClient. Calling closeStream() from destructor
    // can cause race conditions with pending .then() callbacks when using deleteLater().

    // Close database connection via DatabaseThread
    DatabaseThread::getInstance()->closeDatabase(m_symbol);

    DEBUG << "Destroyed";
}


// Get or create a day vector for a given date
QVector<Bar>& BarCache::getOrCreateDayVector(const QDate& date)
{
    ASSUME_GTE(date.dayOfWeek(), Qt::Monday);
    ASSUME_LTE(date.dayOfWeek(), Qt::Friday);

    // NOTE: Caller must already hold m_barCacheRwLock write lock

    if (!m_barCacheByDay.contains(date))
    {
        DEBUG << "m_barCacheByDay map doesn't contain day " << date << ", creating new vector.";

        // Pre-allocate to full capacity with uninitialized bars by default ctor of Bar
        m_barCacheByDay[date] = QVector<Bar>(BarsConstants::MINUTE_BARS_PER_DAY);
    }

    return m_barCacheByDay[date];
}

QVector<Bar> BarCache::fillHolesOfReceivedRequest(const QDateTime& first,
                                                  const QDateTime& last,
                                                  const QVector<Bar>& barsFromAPI) const
{
    OBJ_ASSUME_EQUAL(first.date(), last.date());
    OBJ_ASSUME_GTE(first.date().dayOfWeek(), Qt::Monday);
    OBJ_ASSUME_LTE(first.date().dayOfWeek(), Qt::Friday);
    OBJ_ASSUME_GTE(first.time(), TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(last.time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

    // Account for bar holes where no activity happened
    QVector<Bar> resultBars;
    int voidBarsCreated = 0;

    qsizetype i = 0;

    for (QDateTime expectedTime = first; expectedTime <= last; expectedTime = expectedTime.addSecs(60))
    {
        Bar bar;

        if (i >= barsFromAPI.size())
        {
            bar = Bar::nullBar(expectedTime);
            voidBarsCreated++;
        }
        else
        {
            if (expectedTime == barsFromAPI[i].getTimeStamp())
            {
                bar = barsFromAPI[i];
                i++;
            }
            else
            {
                bar = Bar::nullBar(expectedTime);
                voidBarsCreated++;
            }
        }

        resultBars.append(bar);
    }

    if (voidBarsCreated > 0)
    {
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
BarCache::GetBarsResult_t BarCache::getBars(const QDate& date, const QTime& first, const QTime& last)
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
    OBJ_ASSUME_GTE(first, TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(last, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

    // Assume we are not requesting future dates/times
    OBJ_ASSUME_LTE(date, now.date()); // Can't request bars for future dates

    if (isCurrentDay)
    {
        // Round now.time() up to the next minute boundary because TradeStation
        // timestamps bars with their closing time (e.g., at 11:17:33, the current
        // bar covering 11:17:00-11:17:59 will be timestamped 11:18:00 when it closes)
        QTime nowRoundedUp;
        if (now.time().second() == 0 && now.time().msec() == 0)
        {
            nowRoundedUp = now.time();
        }
        else
        {
            // Use QDateTime to properly handle day boundary crossings
            QDateTime nowRoundedUpDateTime =
                QDateTime(now.date(), QTime(now.time().hour(), now.time().minute(), 0, 0), now.timeZone()).addSecs(60);
            nowRoundedUp = nowRoundedUpDateTime.time();

            // If we wrapped to the next day, that means current time is very late (like 23:59)
            // In this case, we should cap it at the trading end time since we can't have bars beyond trading hours
            if (nowRoundedUpDateTime.date() > now.date())
            {
                nowRoundedUp = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;
            }
        }
        OBJ_ASSUME_LTE(last, nowRoundedUp); // Can't request bars for later today than now
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

    // Create a shared promise for the final result.
    // Using shared_ptr because the promise needs to survive across multiple nested async
    // continuations. Moving a QPromise into nested lambdas causes undefined behavior when
    // Qt's continuation machinery accesses the moved-from promise in the outer lambda.
    QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> promise;
    auto future = promise.future();
    promise.start();

    // Query database via DatabaseThread (async, thread-safe)
    DatabaseThread::getInstance()
        ->getBarsFromDatabase(m_symbol,
                              date,
                              first,
                              last)
        .then(
            this, // Execute in the thread of this BarCache object, aka the MainAlgo thread
            [this, date, first, last, isCurrentDay, now, promise = std::move(promise)](
                std::optional<std::unique_ptr<QVector<Bar>>>&& dbBars) mutable
            {
                // Check if we got the complete day from database
                if (dbBars.has_value())
                {
                    std::shared_ptr<QVector<Bar>> sp = std::move(dbBars.value());

                    const qsizetype expectedBarCount = first.secsTo(last) / 60 + 1;

                    // Happy path, got bars from database
                    OBJ_ASSUME_EQUAL(sp->size(), expectedBarCount);

                    DEBUG << "Loaded complete day from database:" << sp->size() << "bars";

                    // Store the complete day in memory cache
                    storeBarsInCache(date, sp);

                    promise.addResult(sp);
                    promise.finish();
                    return;
                }

                // Database doesn't have complete day - fetch from API
                DEBUG << "Database miss, fetching from API";

                QDateTime startDateTime =
                    QDateTime(date, TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION, TradingHours::MARKET_TIMEZONE);
                QDateTime endDayTime = QDateTime(
                    date,
                    [now, isCurrentDay]() -> QTime
                    {
                        if (isCurrentDay)
                        {
                            if (now.time() > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
                            {
                                return TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;
                            }
                            else
                            {
                                return now.time();
                            }
                        }
                        else
                        {
                            return TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;
                        }
                    }(),
                    TradingHours::MARKET_TIMEZONE);


                DEBUG << "Fetching complete day from API:" << startDateTime << "to" << endDayTime;

                // Call the API and chain the result processing

                TSClient::getInstance()
                    ->getBars(m_symbol,
                              1,
                              Bar::BarUnit::Minute,
                              0,
                              Bar::BarSessionTemplate::USEQ24Hour,
                              startDateTime,
                              endDayTime)
                    .then(this,
                          [this, date, startDateTime, endDayTime, promise = std::move(promise)](
                              std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>&& bars) mutable
                          {
                              if (!bars.has_value())
                              {
                                  CRITICAL << "getBars() from API returned error for" << m_symbol
                                           << "- Error:" << static_cast<int>(bars.error());
                                  promise.addResult(std::unexpected(bars.error()));
                              }
                              else
                              {
                                  INFO << "Asynchronous getBars() from API completed for complete day" << date << "with"
                                       << bars.value()->size() << "bars received";

                                  // Shared_ptr is already provided by TSClient, so we can use it directly
                                  std::shared_ptr<QVector<Bar>> barsFromApiHolesFilled = std::make_shared<QVector<Bar>>(
                                      fillHolesOfReceivedRequest(startDateTime, endDayTime, *bars.value()));

                                  // Store the complete day in memory cache
                                  storeBarsInCache(date, barsFromApiHolesFilled);

                                  // Store in database via DatabaseThread (async, fire-and-forget)
                                  // Note: We intentionally don't wait for the result or attach continuations
                                  // to avoid lifetime/threading issues. The shared_ptr ensures the bars stay alive,
                                  // and destroyed then the DatabaseThread processes them.
                                  [[maybe_unused]] auto dbFuture =
                                      DatabaseThread::getInstance()->storeBarsInDatabase(m_symbol,
                                                                                         date,
                                                                                         barsFromApiHolesFilled);

                                  promise.addResult(barsFromApiHolesFilled);
                              }
                              promise.finish();

                              INFO << "Completed fetching bars from API for day" << date;
                          });
            });

    return future;
}


std::optional<std::unique_ptr<QVector<Bar>>>
BarCache::getBarsFromCache(const QDate& date, const QTime& start, const QTime& end) const
{
    QReadLocker locker(&m_barCacheRwLock);

    if (!m_barCacheByDay.contains(date))
    {
        DEBUG << "m_barCacheByDay map doesn't contain day " << date;
        return std::nullopt;
    }

    DEBUG << "m_barCacheByDay map contains day " << date;

    // Day exists in cache - extract the requested range
    const QVector<Bar>& dayVector = m_barCacheByDay[date];

    // a day vector is always pre-allocated to 840 bars
    OBJ_ASSUME_EQUAL(dayVector.size(), BarsConstants::MINUTE_BARS_PER_DAY);

    std::unique_ptr<QVector<Bar>> result = std::make_unique<QVector<Bar>>();
    result->reserve((start.secsTo(end) / 60) + 1);

    for (size_t index = timeToIndex(start); index <= timeToIndex(end); ++index)
    {
        if (dayVector[index].getBarStatus() == Bar::BarStatus::Uninitialized)
        {
            // Missing bar in cache - treat as cache miss
            DEBUG << "Bar at index" << index << "for day" << date << "is uninitialized in cache";
            return std::nullopt;
        }
        result->append(dayVector[index]);
    }

    DEBUG << "Loaded complete day from memory cache:" << date << "with" << result->size() << "bars";
    return result;
}

void BarCache::storeBarInCache(const Bar& bar)
{
    QDateTime dateTime = bar.getTimeStamp();
    QDate date = dateTime.date();
    QTime time = dateTime.time();

    // Calculate the index for this bar in the day's vector
    size_t index = timeToIndex(time);

    QWriteLocker locker(&m_barCacheRwLock);


    QVector<Bar>& dayVector = getOrCreateDayVector(date);


    // Check if we're overwriting an existing bar (only if the index existed before resize)
    if (bar.getIsRealtime())
    {
        if (bar.getBarStatus() == Bar::BarStatus::Closed)
        {
            DEBUG << "Real time cache insertion; received the closing bar";
        }
    }
    else
    {
        if (dayVector[index].getBarStatus() != Bar::BarStatus::Uninitialized)
        {
            if (timeToIndex(bar.getTimeStamp().time()) == 839)
            {
                WARNING << "We received a double of the last bar of the day for symbol" << m_symbol
                        << "at timestamp:" << bar.getTimeStamp()
                        << "- Experimentally, this has proven to be possible from the API."
                           " It seems to be a little glitch from their side when the app sits idle after hours.";
            }
            else
            {
                CRITICAL
                    << "Inserting historical bar into cache at index" << index << "for timestamp:" << bar.getTimeStamp()
                    << "but that slot was uninitialized. This should not happen as historical bars should be bulk inserted.";

                Q_UNREACHABLE();
            }
        }
    }

    // Store the bar at the appropriate index
    dayVector[index] = bar;

    DEBUG << "Inserted bar in cache at index" << index << "for timestamp:" << bar.getTimeStamp();
}

void BarCache::storeBarsInCache(const QDate& date, const std::shared_ptr<QVector<Bar>>& bars)
{
    OBJ_ASSUME_FALSE(bars->isEmpty());

    if (date < MainApp::getCurrentAppTime().date())
    {
        OBJ_ASSUME_EQUAL(bars->size(), BarsConstants::MINUTE_BARS_PER_DAY);
    }
    else
    {
        OBJ_ASSUME_LTE(bars->size(),
                       static_cast<qsizetype>(MainApp::getCurrentAppTime().time() >
                                                      TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION
                                                  ? BarsConstants::MINUTE_BARS_PER_DAY
                                                  : timeToIndex(MainApp::getCurrentAppTime().time()) + 1));
    }

    OBJ_ASSUME_EQUAL(bars->first().getTimeStamp().date(), bars->last().getTimeStamp().date());
    OBJ_ASSUME_EQUAL(bars->first().getTimeStamp().time(), TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(bars->last().getTimeStamp().time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

    QWriteLocker locker(&m_barCacheRwLock);

    if (bars->size() == BarsConstants::MINUTE_BARS_PER_DAY)
    {
        // Full day
        m_barCacheByDay.insert(date, *bars);

        DEBUG << "Inserted full day in cache for" << date;
    }
    else
    {
        // Partial day - need to create day vector if it doesn't exist
        if (!m_barCacheByDay.contains(date))
        {
            m_barCacheByDay[date] = QVector<Bar>(BarsConstants::MINUTE_BARS_PER_DAY);
        }

        QVector<Bar>& dayVector = m_barCacheByDay[date];

        // Insert/overwrite bars into existing day vector
        for (const Bar& bar: *bars)
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

void BarCache::clearDatabase()
{
    INFO << "Clearing all bars from database for" << m_symbol;

    DatabaseThread::getInstance()->clearDatabase(m_symbol).then(this,
                                                                [this](bool success)
                                                                {
                                                                    if (success)
                                                                    {
                                                                        INFO << "Successfully cleared database for"
                                                                             << m_symbol;
                                                                    }
                                                                    else
                                                                    {
                                                                        WARNING << "Failed to clear database for"
                                                                                << m_symbol;
                                                                    }
                                                                });
}

constexpr QVector<std::tuple<QDate, QTime, QTime>> BarCache::splitIntoTradingDayRanges(const QDateTime& first,
                                                                                       const QDateTime& last)
{
    ASSUME_EQUAL(first.timeZone(), QTimeZone("America/New_York"));
    ASSUME_EQUAL(last.timeZone(), QTimeZone("America/New_York"));
    ASSUME_LT(first, last);

    // Ensure range is within trading hours
    ASSUME_GTE(first.time(), TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION);
    ASSUME_LTE(last.time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);


    QVector<std::tuple<QDate, QTime, QTime>> ranges;

    for (QDate date = first.date(); date <= last.date(); date = date.addDays(1))
    {

        // Skip weekends
        if (date.dayOfWeek() > Qt::Friday)
        {
            continue;
        }

        QTime dayTimeStart = (date == first.date()) ? first.time() : TradingHours::TIME_FIRST_CANDLE_PRE_MARKET_SESSION;
        QTime dayTimeEnd = (date == last.date()) ? last.time() : TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;

        std::tuple<QDate, QTime, QTime> range = std::make_tuple(date, dayTimeStart, dayTimeEnd);

        ranges.append(range);
    }

    return ranges;
}
