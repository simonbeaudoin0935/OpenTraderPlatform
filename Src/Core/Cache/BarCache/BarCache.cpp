#include <QTimeZone>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QPromise>
#include <memory>

#include "MainApp.h"
#include "BarCache.h"
#include "DatabaseThread.h"
#include "DBClient.h"
#include "TSClient.h"
#include "Settings.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY BarCacheLog
Q_LOGGING_CATEGORY(BarCacheLog, "BarCache")

BarCache::BarCache(const QString& symbol, QObject* parent) : QObject(parent), m_symbol(symbol)
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
}

BarCache::~BarCache()
{
    // Close database connection via DatabaseThread
    DatabaseThread::getInstance()->closeDatabase(m_symbol);

    DEBUG << "Destroyed";
}

void BarCache::storeBar(const Bar& bar)
{
    storeBarInCache(bar);
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
    OBJ_ASSUME_GTE(first.time(), TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
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
 * (4:00 AM to 6:59 PM ET) is retrieved.
 *
 * @param date The date for which to retrieve bars (must be a weekday: Monday to Friday).
 * @param first The start time of the range (must be between 4:00 AM and 6:59 PM ET).
 * @param last The end time of the range (must be between 4:00 AM and 6:59 PM ET, and >= first).
 *
 * @return A QFuture containing a std::shared_ptr to a QVector<Bar> with the requested bars.
 *
 * @pre date is a weekday (Monday to Friday).
 * @pre first and last are within trading hours (4:00 AM to 6:59 PM ET).
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

    // Assume monday-friday and between 4:00am-6:59pm
    OBJ_ASSUME_GTE(date.dayOfWeek(), Qt::Monday);
    OBJ_ASSUME_LTE(date.dayOfWeek(), Qt::Friday);
    OBJ_ASSUME_GTE(first, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(last, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

    // Assume we are not requesting future dates/times
    OBJ_ASSUME_LTE(date, now.date()); // Can't request bars for future dates

    if (isCurrentDay)
    {
        // Floor now.time() to the current minute boundary because open-time convention
        // timestamps bars with their open time (e.g., at 11:17:33, the current
        // bar covering 11:17:00-11:17:59 is timestamped 11:17:00)
        QTime nowFloored(now.time().hour(), now.time().minute(), 0, 0);
        OBJ_ASSUME_LTE(last, nowFloored); // Can't request bars for later today than now
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

    // Always fetch the FULL day from database (to warm cache) even if only partial range requested
    // This ensures we cache complete days and avoid repeated database queries for the same day
    // Full day: 4:00 AM (early pre-market open) to 6:59 PM (after-market close) = 900 bars total
    QTime fullDayStart = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION; // 4:00 AM
    QTime fullDayEnd = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;        // 6:59 PM

    // Query database via DatabaseThread (async, thread-safe)
    // Request the FULL day, not just the requested range
    DatabaseThread::getInstance()
        ->getBarsFromDatabase(m_symbol,
                              date,
                              fullDayStart,
                              fullDayEnd)
        .then(this, // Execute in the thread of this BarCache object, aka the MainAlgo thread
              [this, date, first, last, fullDayStart, fullDayEnd, isCurrentDay, now, promise = std::move(promise)](
                  std::optional<std::shared_ptr<QVector<Bar>>>&& dbBars) mutable
              {
                  // Check if we got bars from database
                  if (dbBars.has_value())
                  {
                      std::shared_ptr<QVector<Bar>> fullDayBars = std::move(dbBars.value());

                      DEBUG << "Loaded full day from database:" << fullDayBars->size() << "bars";

                      // Store the complete day in memory cache
                      storeBarsInCache(date, fullDayBars);

                      // Now filter to the requested range and return
                      auto filteredBars = std::make_shared<QVector<Bar>>();
                      for (const auto& bar: *fullDayBars)
                      {
                          if (bar.getTimeStamp().time() >= first && bar.getTimeStamp().time() <= last)
                          {
                              filteredBars->append(bar);
                          }
                      }

                      DEBUG << "Returning filtered bars:" << filteredBars->size()
                            << "out of full day:" << fullDayBars->size();

                      promise.addResult(filteredBars);
                      promise.finish();
                      return;
                  }

                  // Database doesn't have complete day - fetch from API
                  DEBUG << "Database miss, fetching from API";

                  QDateTime startDateTime = QDateTime(date,
                                                      TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                                      TradingHours::MARKET_TIMEZONE);

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

                  // Fetch from Databento via DBClient
                  auto* dbClient = DBClient::getInstance();
                  if (!dbClient->hasApiKey())
                  {
                      WARNING << "No Databento API key — cannot fetch historical bars for" << m_symbol;
                      promise.addResult(std::unexpected(TSClient::Error::Other));
                      promise.finish();
                      return;
                  }

                  // Connect one-shot to historicalBarsReceived to resolve the promise
                  auto sharedPromise =
                      std::make_shared<QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(
                          std::move(promise));

                  auto conn = std::make_shared<QMetaObject::Connection>();
                  *conn = connect(
                      dbClient,
                      &DBClient::historicalBarsReceived,
                      this,
                      [this, conn, sharedPromise, date, first, last](const QString& sym, const QVector<Bar>& bars)
                      {
                          if (sym != m_symbol)
                              return; // Not our request

                          disconnect(*conn); // One-shot

                          if (bars.isEmpty())
                          {
                              DEBUG << "Historical fetch returned 0 bars for" << m_symbol;
                              sharedPromise->addResult(std::make_shared<QVector<Bar>>());
                              sharedPromise->finish();
                              return;
                          }

                          auto fullDayBars = std::make_shared<QVector<Bar>>(bars);
                          DEBUG << "Historical fetch returned" << fullDayBars->size() << "bars for" << m_symbol
                                << "— first:" << fullDayBars->first().getTimeStamp().toString(Qt::ISODate)
                                << "last:" << fullDayBars->last().getTimeStamp().toString(Qt::ISODate);

                          // Fill holes with Null bars so the day vector is contiguous
                          QDateTime fullDayFirst(date,
                                                 TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                                 TradingHours::MARKET_TIMEZONE);
                          QDateTime fullDayLast(date,
                                                TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                                TradingHours::MARKET_TIMEZONE);
                          auto filledBars = std::make_shared<QVector<Bar>>(
                              fillHolesOfReceivedRequest(fullDayFirst, fullDayLast, *fullDayBars));

                          storeBarsInCache(date, filledBars);

                          // Persist to database for next startup (fire-and-forget)
                          [[maybe_unused]] auto dbFuture =
                              DatabaseThread::getInstance()->storeBarsInDatabase(m_symbol, date, filledBars);

                          auto filteredBars = std::make_shared<QVector<Bar>>();
                          for (const auto& bar: *filledBars)
                          {
                              if (bar.getTimeStamp().time() >= first && bar.getTimeStamp().time() <= last)
                              {
                                  filteredBars->append(bar);
                              }
                          }

                          DEBUG << "Historical backfill complete:" << filteredBars->size() << "bars (of"
                                << filledBars->size() << "total)";

                          sharedPromise->addResult(filteredBars);
                          sharedPromise->finish();
                      });

                  dbClient->fetchHistoricalBars(m_symbol, startDateTime, endDayTime);
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

    // a day vector is always pre-allocated to 900 bars
    OBJ_ASSUME_EQUAL(dayVector.size(), BarsConstants::MINUTE_BARS_PER_DAY);

    std::unique_ptr<QVector<Bar>> result = std::make_unique<QVector<Bar>>();
    result->reserve((start.secsTo(end) / 60) + 1);

    for (size_t index = BarsConstants::timeToIndex(start); index <= BarsConstants::timeToIndex(end); ++index)
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

    OBJ_ASSUME_EQUAL(dateTime.timeZone(), TradingHours::MARKET_TIMEZONE);

    QDate date = dateTime.date();
    QTime time = dateTime.time();

    // Calculate the index for this bar in the day's vector
    size_t index = BarsConstants::timeToIndex(time);

    QWriteLocker locker(&m_barCacheRwLock);

    QVector<Bar>& dayVector = getOrCreateDayVector(date);

    // Log when overwriting existing bar slots
    if (bar.getBarStatus() == Bar::BarStatus::Closed)
    {
        if (dayVector[index].getBarStatus() != Bar::BarStatus::Uninitialized)
        {
            if (dayVector[index] == bar)
            {
                DEBUG << "Duplicate closed bar at index" << index << "for" << bar.getTimeStamp() << "- ignoring.";
            }
            else
            {
                WARNING << "Overwriting existing bar at index" << index << "for" << bar.getTimeStamp();
            }
        }
    }

    // Store the bar at the appropriate index
    dayVector[index] = bar;

    if (bar.getBarStatus() == Bar::BarStatus::Closed)
    {
        DEBUG << "Inserted bar in cache at index" << index << "for timestamp:" << bar.getTimeStamp();
    }
}

void BarCache::storeBarsInCache(const QDate& date, const std::shared_ptr<QVector<Bar>>& bars)
{
    OBJ_ASSUME_FALSE(bars->isEmpty());

    // In live mode, validate bar counts make sense relative to current time
    // In replay mode, skip validation since we're loading historical data that may have full days
    if (TSClient::getInstance()->getMode() == TSClient::Mode::Live)
    {
        if (date < MainApp::getCurrentAppTime().date())
        {
            // Past day - thinly traded stocks may have many empty minutes,
            // so only assert upper bound. Zero bars is caught by ASSUME_FALSE above.
            OBJ_ASSUME_LTE(bars->size(), BarsConstants::MINUTE_BARS_PER_DAY);
        }
        else
        {
            // Current day - can't have more bars than up to now
            OBJ_ASSUME_LTE(bars->size(),
                           static_cast<qsizetype>(
                               MainApp::getCurrentAppTime().time() > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION
                                   ? BarsConstants::MINUTE_BARS_PER_DAY
                                   : BarsConstants::timeToIndex(MainApp::getCurrentAppTime().time()) + 1));
        }
    }

    OBJ_ASSUME_EQUAL(bars->first().getTimeStamp().date(), bars->last().getTimeStamp().date());
    OBJ_ASSUME_GTE(bars->first().getTimeStamp().time(), TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
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
            size_t index = BarsConstants::timeToIndex(bar.getTimeStamp().time());
            dayVector[index] = bar;
        }

        DEBUG << "Inserted partial day in cache for" << date << "with" << bars->size() << "bars";
    }
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
    ASSUME_EQUAL(first.timeZone(), TradingHours::MARKET_TIMEZONE);
    ASSUME_EQUAL(last.timeZone(), TradingHours::MARKET_TIMEZONE);
    ASSUME_LT(first, last);

    // Ensure range is within trading hours
    ASSUME_GTE(first.time(), TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
    ASSUME_LTE(last.time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);


    QVector<std::tuple<QDate, QTime, QTime>> ranges;

    for (QDate date = first.date(); date <= last.date(); date = date.addDays(1))
    {

        // Skip weekends
        if (date.dayOfWeek() > Qt::Friday)
        {
            continue;
        }

        QTime dayTimeStart =
            (date == first.date()) ? first.time() : TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION;
        QTime dayTimeEnd = (date == last.date()) ? last.time() : TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;

        std::tuple<QDate, QTime, QTime> range = std::make_tuple(date, dayTimeStart, dayTimeEnd);

        ranges.append(range);
    }

    return ranges;
}
