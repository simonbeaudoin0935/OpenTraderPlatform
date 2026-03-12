#include <QTimeZone>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QPromise>
#include <memory>
#include <tuple>

#include "MainApp.h"
#include "BarCache.h"
#include "DatabaseThread.h"
#include "DBClient.h"
#include "TSClient.h"
#include "Settings.h"
#include "Logging.h"
#include "LTTng/LTTngTracepoints.h"
#include "Assume.h"
#include "BarUtils.h"

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

    // Open database via DatabaseThread (async, thread-safe).
    // We don't wait for the result here — operations queue until the DB is ready.
    // DatabaseThread logs success/failure internally; no .then() callback needed
    // (using .then(this, lambda) is unsafe here because a rapid symbol switch can
    // destroy this BarCache before the future resolves, making the QPointer null
    // and triggering a Q_ASSERT in Qt's continuation machinery).
    std::ignore = DatabaseThread::getInstance()->openDatabase(m_symbol, m_dbPath);
}

BarCache::~BarCache()
{
    // Close database connection via DatabaseThread
    DatabaseThread::getInstance()->closeDatabase(m_symbol);

    DEBUG << "Destroyed";
}

void BarCache::storeBar(TimeFrame tf, const Bar& bar)
{
    storeBarInCache(tf, bar);
}


// Get or create a day vector for a given timescale and date
QVector<Bar>& BarCache::getOrCreateDayVector(TimeFrame tf, const QDate& date)
{
    ASSUME_GTE(date.dayOfWeek(), Qt::Monday);
    ASSUME_LTE(date.dayOfWeek(), Qt::Friday);

    // NOTE: Caller must already hold m_barCacheRwLock write lock

    auto& dayMap = m_barCacheByTimeFrame[tf];
    if (!dayMap.contains(date))
    {
        DEBUG << "Creating new bar vector for timescale" << static_cast<int>(tf) << "day" << date;

        // Pre-allocate to full capacity with default-constructed (Uninitialized) bars
        dayMap[date] = QVector<Bar>(BarUtils::barsPerDay(tf));

        L2T_TP(l2trader,
               barcache_day_alloc,
               m_symbol.toUtf8().constData(),
               static_cast<int>(tf),
               date.toString("yyyy-MM-dd").toUtf8().constData(),
               BarUtils::barsPerDay(tf));
    }

    return dayMap[date];
}

QVector<Bar> BarCache::fillHolesOfReceivedRequest(TimeFrame tf,
                                                  const QDateTime& first,
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

    const qint64 stepSecs = static_cast<qint64>(BarUtils::secondsPerBar(tf));

    // Compute expected slot count so we can reserve and trace it
    const qint64 totalSecs = first.secsTo(last);
    [[maybe_unused]] const int expectedSlots = stepSecs > 0 ? static_cast<int>(totalSecs / stepSecs) + 1 : 0;

    L2T_TP(l2trader,
           fillholes_run,
           "unknown", // symbol not in scope here; use tf+step as signal
           static_cast<int>(tf),
           static_cast<int>(stepSecs),
           static_cast<int>(barsFromAPI.size()),
           expectedSlots);

    for (QDateTime expectedTime = first; expectedTime <= last; expectedTime = expectedTime.addSecs(stepSecs))
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

    L2T_TP(l2trader,
           fillholes_done,
           "unknown",
           static_cast<int>(tf),
           static_cast<int>(resultBars.size()),
           voidBarsCreated);

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
BarCache::GetBarsResult_t BarCache::getBars(TimeFrame tf, const QDate& date, const QTime& first, const QTime& last)
{
    const QDateTime now = MainApp::getCurrentAppTime();
    // In replay mode, the replay date is a historical date — treat it as a past day so we
    // always fetch the full day from the API (not capped at the replay start time).
    const bool isCurrentDay = !MainApp::isInReplayMode() && (date == now.date());

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
        // Floor now.time() to the current bar boundary for this timescale
        int minutesFromOpen = (now.time().hour() - 4) * 60 + now.time().minute();
        int barBoundaryMinutes = (minutesFromOpen / BarUtils::minutesPerBar(tf)) * BarUtils::minutesPerBar(tf);
        int boundaryHour = 4 + barBoundaryMinutes / 60;
        int boundaryMin = barBoundaryMinutes % 60;
        QTime nowFloored(boundaryHour, boundaryMin, 0, 0);
        OBJ_ASSUME_LTE(last, nowFloored); // Can't request bars for later today than now
    }

    DEBUG << "getBarsInDay() called for day" << date << "and time range" << first << "to" << last;

    // With the new "complete day or nothing" rule:
    // If the day exists in cache, we have ALL bars for that day (or all bars up to now for current day)
    if (std::optional<std::unique_ptr<QVector<Bar>>> barsFromCache = getBarsFromCache(tf, date, first, last);
        barsFromCache.has_value())
    {
        DEBUG << "Returning bars from in-memory cache for day" << date;

        return std::move(barsFromCache.value());
    }

    DEBUG << " m_barCacheByTimeFrame map doesn't contain bars for day " << date << "checking database";

    // Create a shared promise for the final result.
    // Using shared_ptr because the promise needs to survive across multiple nested async
    // continuations. Moving a QPromise into nested lambdas causes undefined behavior when
    // Qt's continuation machinery accesses the moved-from promise in the outer lambda.
    QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> promise;
    auto future = promise.future();
    promise.start();

    // Always fetch the FULL day from database (to warm cache) even if only partial range requested
    // This ensures we cache complete days and avoid repeated database queries for the same day
    QTime fullDayStart = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION; // 4:00 AM
    QTime fullDayEnd = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;        // 6:59 PM

    // Query database via DatabaseThread (async, thread-safe)
    // Request the FULL day, not just the requested range
    DatabaseThread::getInstance()
        ->getBarsFromDatabase(m_symbol,
                              tf,
                              date,
                              fullDayStart,
                              fullDayEnd)
        .then(this, // Execute in the thread of this BarCache object, aka the MainAlgo thread
              [this, tf, date, first, last, fullDayStart, fullDayEnd, isCurrentDay, now, promise = std::move(promise)](
                  std::optional<std::shared_ptr<QVector<Bar>>>&& dbBars) mutable
              {
                  // Check if we got bars from database
                  if (dbBars.has_value())
                  {
                      std::shared_ptr<QVector<Bar>> fullDayBars = std::move(dbBars.value());

                      DEBUG << "Loaded full day from database:" << fullDayBars->size() << "bars";

                      // Store the complete day in memory cache
                      storeBarsInCache(tf, date, fullDayBars);

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

                  // Database doesn't have complete day.
                  // Before going to the API, check if the source timescale is already in
                  // memory and aggregate from it — avoids a redundant Databento fetch.
                  // Example: switching 1m→5m when 1m bars are already cached in memory.
                  const TimeFrame sourceTf = BarUtils::aggregateSourceTimeFrame(tf);
                  if (sourceTf != tf)
                  {
                      auto sourceIt = m_barCacheByTimeFrame.constFind(sourceTf);
                      if (sourceIt != m_barCacheByTimeFrame.constEnd())
                      {
                          auto dayIt = sourceIt->constFind(date);
                          if (dayIt != sourceIt->constEnd() && !dayIt->isEmpty())
                          {
                              DEBUG << "Aggregating" << static_cast<int>(tf) << "m bars from in-memory"
                                    << static_cast<int>(sourceTf) << "m cache (no API fetch needed)";

                              QVector<Bar> aggregated = BarUtils::aggregateBars(*dayIt, tf);

                              auto fullDayBars = std::make_shared<QVector<Bar>>(fillHolesOfReceivedRequest(
                                  tf,
                                  QDateTime(date,
                                            TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                            TradingHours::MARKET_TIMEZONE),
                                  QDateTime(date,
                                            TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                            TradingHours::MARKET_TIMEZONE),
                                  aggregated));

                              storeBarsInCache(tf, date, fullDayBars);

                              // Persist to database (fire-and-forget)
                              [[maybe_unused]] auto dbFuture =
                                  DatabaseThread::getInstance()->storeBarsInDatabase(m_symbol, tf, date, fullDayBars);

                              auto filteredBars = std::make_shared<QVector<Bar>>();
                              for (const auto& bar: *fullDayBars)
                              {
                                  if (bar.getTimeStamp().time() >= first && bar.getTimeStamp().time() <= last)
                                      filteredBars->append(bar);
                              }

                              promise.addResult(filteredBars);
                              promise.finish();
                              return;
                          }
                      }
                  }

                  // No source-TF cache hit — fetch from Databento API
                  DEBUG << "No in-memory source cache for" << static_cast<int>(tf) << "m — fetching from API";

                  L2T_TP(l2trader,
                         barcache_api_fetch_start,
                         m_symbol.toUtf8().constData(),
                         static_cast<int>(tf),
                         date.toString("yyyy-MM-dd").toUtf8().constData());

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
                      [this, conn, sharedPromise, tf, date, first, last](const QString& sym, const QVector<Bar>& bars)
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

                          L2T_TP(l2trader,
                                 barcache_api_fetch_done,
                                 m_symbol.toUtf8().constData(),
                                 static_cast<int>(tf),
                                 date.toString("yyyy-MM-dd").toUtf8().constData(),
                                 static_cast<int>(fullDayBars->size()));

                          // Fill holes with Null bars so the day vector is contiguous
                          QDateTime fullDayFirst(date,
                                                 TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                                 TradingHours::MARKET_TIMEZONE);
                          QDateTime fullDayLast(date,
                                                TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                                TradingHours::MARKET_TIMEZONE);
                          auto filledBars = std::make_shared<QVector<Bar>>(
                              fillHolesOfReceivedRequest(tf, fullDayFirst, fullDayLast, *fullDayBars));

                          storeBarsInCache(tf, date, filledBars);

                          // Persist to database for next startup (fire-and-forget)
                          [[maybe_unused]] auto dbFuture =
                              DatabaseThread::getInstance()->storeBarsInDatabase(m_symbol, tf, date, filledBars);

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

                  dbClient->fetchHistoricalBars(m_symbol, startDateTime, endDayTime, tf);
              });

    return future;
}


std::optional<std::unique_ptr<QVector<Bar>>>
BarCache::getBarsFromCache(TimeFrame tf, const QDate& date, const QTime& start, const QTime& end) const
{
    QReadLocker locker(&m_barCacheRwLock);

    const auto tfIt = m_barCacheByTimeFrame.find(tf);
    if (tfIt == m_barCacheByTimeFrame.end() || !tfIt->contains(date))
    {
        DEBUG << "Cache miss for timescale" << static_cast<int>(tf) << "day" << date;
        L2T_TP(l2trader,
               barcache_cache_miss,
               m_symbol.toUtf8().constData(),
               static_cast<int>(tf),
               date.toString("yyyy-MM-dd").toUtf8().constData());
        return std::nullopt;
    }

    DEBUG << "Cache hit for timescale" << static_cast<int>(tf) << "day" << date;

    // Day exists in cache - extract the requested range
    const QVector<Bar>& dayVector = (*tfIt)[date];

    // A day vector is always pre-allocated to barsPerDay(tf) slots
    OBJ_ASSUME_EQUAL(dayVector.size(), BarUtils::barsPerDay(tf));

    std::unique_ptr<QVector<Bar>> result = std::make_unique<QVector<Bar>>();
    result->reserve(BarUtils::barIndex(tf, end) - BarUtils::barIndex(tf, start) + 1);

    for (int index = BarUtils::barIndex(tf, start); index <= BarUtils::barIndex(tf, end); ++index)
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

    L2T_TP(l2trader,
           barcache_cache_hit,
           m_symbol.toUtf8().constData(),
           static_cast<int>(tf),
           date.toString("yyyy-MM-dd").toUtf8().constData(),
           static_cast<int>(result->size()));

    return result;
}

void BarCache::storeBarInCache(TimeFrame tf, const Bar& bar)
{
    QDateTime dateTime = bar.getTimeStamp();

    OBJ_ASSUME_EQUAL(dateTime.timeZone(), TradingHours::MARKET_TIMEZONE);

    QDate date = dateTime.date();
    QTime time = dateTime.time();

    // Calculate the index for this bar in the day's vector
    int index = BarUtils::barIndex(tf, time);

    QWriteLocker locker(&m_barCacheRwLock);

    QVector<Bar>& dayVector = getOrCreateDayVector(tf, date);

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

    L2T_TP(l2trader, barcache_store_bar_live, m_symbol.toUtf8().constData(), static_cast<int>(tf), index);

    if (bar.getBarStatus() == Bar::BarStatus::Closed)
    {
        DEBUG << "Inserted bar in cache at index" << index << "for timestamp:" << bar.getTimeStamp();
    }
}

void BarCache::storeBarsInCache(TimeFrame tf, const QDate& date, const std::shared_ptr<QVector<Bar>>& bars)
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
            OBJ_ASSUME_LTE(bars->size(), BarUtils::barsPerDay(tf));
        }
        else
        {
            // Current day - can't have more bars than up to now
            OBJ_ASSUME_LTE(bars->size(),
                           static_cast<qsizetype>(
                               MainApp::getCurrentAppTime().time() > TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION
                                   ? BarUtils::barsPerDay(tf)
                                   : BarUtils::barIndex(tf, MainApp::getCurrentAppTime().time()) + 1));
        }
    }

    OBJ_ASSUME_EQUAL(bars->first().getTimeStamp().date(), bars->last().getTimeStamp().date());
    OBJ_ASSUME_GTE(bars->first().getTimeStamp().time(), TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(bars->last().getTimeStamp().time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

    QWriteLocker locker(&m_barCacheRwLock);

    if (bars->size() == BarUtils::barsPerDay(tf))
    {
        // Full day
        m_barCacheByTimeFrame[tf].insert(date, *bars);

        DEBUG << "Inserted full day in cache for" << date;

        L2T_TP(l2trader,
               barcache_store_full_day,
               m_symbol.toUtf8().constData(),
               static_cast<int>(tf),
               date.toString("yyyy-MM-dd").toUtf8().constData(),
               static_cast<int>(bars->size()));
    }
    else
    {
        // Partial day - slot bars into the pre-allocated day vector
        // getOrCreateDayVector is called inside the write lock (locker already held above)
        auto& dayMap = m_barCacheByTimeFrame[tf];
        if (!dayMap.contains(date))
        {
            dayMap[date] = QVector<Bar>(BarUtils::barsPerDay(tf));
        }
        QVector<Bar>& dayVector = dayMap[date];

        // Insert/overwrite bars into existing day vector
        for (const Bar& bar: *bars)
        {
            int index = BarUtils::barIndex(tf, bar.getTimeStamp().time());
            dayVector[index] = bar;
        }

        DEBUG << "Inserted partial day in cache for" << date << "with" << bars->size() << "bars";

        L2T_TP(l2trader,
               barcache_store_partial,
               m_symbol.toUtf8().constData(),
               static_cast<int>(tf),
               date.toString("yyyy-MM-dd").toUtf8().constData(),
               static_cast<int>(bars->size()),
               BarUtils::barsPerDay(tf));
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
