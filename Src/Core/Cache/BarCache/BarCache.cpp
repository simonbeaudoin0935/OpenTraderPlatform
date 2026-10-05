#include <QTimeZone>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QPointer>
#include <QPromise>
#include <QFutureWatcher>
#include <algorithm>
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
#include "BarHistoryBackfill.h"

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
    m_openDatabaseFuture = DatabaseThread::getInstance()->openDatabase(m_symbol, m_dbPath);
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

    if (bar.getBarStatus() == Bar::BarStatus::Closed)
    {
        auto bars = std::make_shared<QVector<Bar>>();
        bars->append(bar);
        [[maybe_unused]] auto dbFuture =
            DatabaseThread::getInstance()->storeBarsInDatabase(m_symbol, tf, bar.getTimeStamp().date(), bars);
    }
}

QTime BarCache::floorTimeToBarBoundary(const TimeFrame tf, const QTime& time)
{
    if (time <= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        return TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION;
    }
    if (time >= TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION)
    {
        return TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;
    }

    const int secondsFromOpen = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.secsTo(
        QTime(time.hour(), time.minute(), time.second()));
    const int barSeconds = BarUtils::secondsPerBar(tf);
    const int flooredSecondsFromOpen = (secondsFromOpen / barSeconds) * barSeconds;
    return TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION.addSecs(flooredSecondsFromOpen);
}

void BarCache::prefillNullBarsThrough(const TimeFrame tf, const QDate& date, const QTime& lastInclusive)
{
    QWriteLocker locker(&m_barCacheRwLock);
    QVector<Bar>& dayVector = getOrCreateDayVector(tf, date);
    const int lastIndex = BarUtils::barIndex(tf, lastInclusive);
    for (int index = 0; index <= lastIndex; ++index)
    {
        if (dayVector[index].getBarStatus() == Bar::BarStatus::Uninitialized)
        {
            dayVector[index] =
                Bar::nullBar(QDateTime(date, BarUtils::indexToBarTime(tf, index), TradingHours::MARKET_TIMEZONE));
        }
    }
}

void BarCache::warmCurrentDayCacheForLive(const QDateTime& p_now)
{
    const QDate date = p_now.date();
    if (date.dayOfWeek() < Qt::Monday || date.dayOfWeek() > Qt::Friday)
    {
        return;
    }

    if (!m_openDatabaseFuture.isFinished())
    {
        m_openDatabaseFuture.waitForFinished();
    }
    if (!m_openDatabaseFuture.result())
    {
        WARNING << "Cannot warm current-day cache because the database failed to open for" << m_symbol;
        return;
    }

    const auto loadCachedBars = [this, date, p_now](const TimeFrame tf)
    {
        QFuture<std::shared_ptr<QVector<Bar>>> future =
            DatabaseThread::getInstance()->getCachedBarsForDate(m_symbol, tf, date);
        future.waitForFinished();
        const std::shared_ptr<QVector<Bar>> bars = future.result();
        static_cast<void>(currentDayBackfillStart(tf, date, p_now, bars.get()));
        if (!bars->isEmpty())
        {
            storeBarsInCache(tf, date, bars);
        }
    };

    loadCachedBars(TimeFrame::ONE_MINUTE);
    loadCachedBars(TimeFrame::TEN_SECONDS);

    if (p_now.time() >= TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        prefillNullBarsThrough(TimeFrame::ONE_MINUTE,
                               date,
                               floorTimeToBarBoundary(TimeFrame::ONE_MINUTE, p_now.time()));
        prefillNullBarsThrough(TimeFrame::TEN_SECONDS,
                               date,
                               floorTimeToBarBoundary(TimeFrame::TEN_SECONDS, p_now.time()));
    }
}

std::optional<QDateTime> BarCache::currentDayBackfillStart(const TimeFrame tf,
                                                           const QDate& date,
                                                           const QDateTime& now,
                                                           const QVector<Bar>* savedBars)
{
    QWriteLocker locker(&m_barCacheRwLock);
    auto& starts = m_currentDayBackfillStartByTimeFrame[tf];
    if (starts.contains(date))
    {
        return starts.value(date);
    }
    const auto& verified = m_currentDayHistoryVerifiedThroughByTimeFrame[tf];
    const std::optional<QDateTime> verifiedThrough =
        verified.contains(date) ? std::optional<QDateTime>(verified.value(date)) : std::nullopt;
    const QDateTime dayStart(date,
                             TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                             TradingHours::MARKET_TIMEZONE);
    const QDateTime latestCompleted =
        QDateTime(date, floorTimeToBarBoundary(tf, now.time()), TradingHours::MARKET_TIMEZONE)
            .addSecs(-BarUtils::secondsPerBar(tf));
    const auto start =
        BarHistoryBackfill::start(savedBars != nullptr ? *savedBars : m_barCacheByTimeFrame.value(tf).value(date),
                                  dayStart,
                                  latestCompleted,
                                  verifiedThrough);
    if (start.has_value())
    {
        starts.insert(date, *start);
    }
    return start;
}

std::optional<QDateTime> BarCache::getLatestClosedBarTimestamp(const TimeFrame tf, const QDate& date) const
{
    QReadLocker locker(&m_barCacheRwLock);

    const auto tfIt = m_barCacheByTimeFrame.constFind(tf);
    if (tfIt == m_barCacheByTimeFrame.constEnd())
    {
        return std::nullopt;
    }
    const auto dayIt = tfIt->constFind(date);
    if (dayIt == tfIt->constEnd())
    {
        return std::nullopt;
    }

    const QVector<Bar>& dayVector = *dayIt;
    for (int index = dayVector.size() - 1; index >= 0; --index)
    {
        if (dayVector[index].getBarStatus() == Bar::BarStatus::Closed)
        {
            return dayVector[index].getTimeStamp();
        }
    }

    return std::nullopt;
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

        LTTnG_TP(opentraderplatform,
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

    LTTnG_TP(opentraderplatform,
           fillholes_run,
           "unknown", // symbol not in scope here; use tf+step as signal
           static_cast<int>(tf),
           static_cast<int>(stepSecs),
           static_cast<int>(barsFromAPI.size()),
           expectedSlots);

    for (QDateTime expectedTime = first; expectedTime <= last; expectedTime = expectedTime.addSecs(stepSecs))
    {
        Bar bar;
        while (i < barsFromAPI.size() && barsFromAPI[i].getTimeStamp() < expectedTime)
        {
            ++i;
        }

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

    LTTnG_TP(opentraderplatform,
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
 * @pre first and last have milliseconds set to 0.
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
    const QTime firstAligned = floorTimeToBarBoundary(tf, first);
    const QTime lastAligned = floorTimeToBarBoundary(tf, last);

    // We align second-level boundaries per timeframe but still expect millisecond precision to be clean.
    OBJ_ASSUME_EQUAL(first.msec(), 0);
    OBJ_ASSUME_EQUAL(last.msec(), 0);

    // Assume monday-friday and between 4:00am-6:59pm
    OBJ_ASSUME_GTE(date.dayOfWeek(), Qt::Monday);
    OBJ_ASSUME_LTE(date.dayOfWeek(), Qt::Friday);
    OBJ_ASSUME_GTE(firstAligned, TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(lastAligned, TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);
    OBJ_ASSUME_LTE(firstAligned, lastAligned);

    // Assume we are not requesting future dates/times.
    // In replay mode the replay clock can transiently lag the newly selected replay date
    // during reset/reload, so use the real market date as the upper bound instead.
    const QDate latestAllowedDate = MainApp::isInReplayMode()
                                        ? QDateTime::currentDateTime().toTimeZone(TradingHours::MARKET_TIMEZONE).date()
                                        : now.date();
    OBJ_ASSUME_LTE(date, latestAllowedDate);

    if (isCurrentDay)
    {
        // Can't request bars for later today than the active timeframe boundary.
        const QTime nowFloored = floorTimeToBarBoundary(tf, now.time());
        OBJ_ASSUME_LTE(lastAligned, nowFloored);
    }

    DEBUG << "getBarsInDay() called for day" << date << "and aligned time range" << firstAligned << "to" << lastAligned;

    if (isCurrentDay && tf >= TimeFrame::FIVE_MINUTES && tf <= TimeFrame::FOUR_HOURS)
    {
        using Result = std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>;
        const auto aggregate = [this, tf, date, firstAligned, lastAligned, now](const Result& source) -> Result
        {
            if (!source.has_value())
            {
                return std::unexpected(source.error());
            }
            const auto aggregated = BarUtils::aggregateBars(*source.value(), tf);
            auto bars = std::make_shared<QVector<Bar>>(fillHolesOfReceivedRequest(
                tf,
                QDateTime(date,
                          TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                          TradingHours::MARKET_TIMEZONE),
                QDateTime(date, floorTimeToBarBoundary(tf, now.time()), TradingHours::MARKET_TIMEZONE),
                aggregated));
            storeBarsInCache(tf, date, bars);
            auto filtered = std::make_shared<QVector<Bar>>();
            for (const Bar& bar: *bars)
            {
                if (bar.getTimeStamp().time() >= firstAligned && bar.getTimeStamp().time() <= lastAligned)
                {
                    filtered->append(bar);
                }
            }
            return filtered;
        };
        auto source = getBars(TimeFrame::ONE_MINUTE,
                              date,
                              TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                              floorTimeToBarBoundary(TimeFrame::ONE_MINUTE, now.time()));
        if (std::holds_alternative<std::shared_ptr<QVector<Bar>>>(source))
        {
            auto result = aggregate(std::get<std::shared_ptr<QVector<Bar>>>(source));
            OBJ_ASSUME_TRUE(result.has_value());
            return result.value();
        }
        auto promise = std::make_shared<QPromise<Result>>();
        promise->start();
        auto future = promise->future();
        auto* watcher = new QFutureWatcher<Result>(this);
        Q_CHECK_PTR(watcher);
        connect(watcher,
                &QFutureWatcher<Result>::finished,
                watcher,
                [watcher, promise, aggregate]()
                {
                    promise->addResult(aggregate(watcher->result()));
                    promise->finish();
                    watcher->deleteLater();
                });
        watcher->setFuture(std::get<QFuture<Result>>(source));
        return future;
    }

    if (isCurrentDay)
    {
        const auto pending = m_currentDayHistoryRequests.value(tf).value(date);
        if (pending.isValid() && !pending.isFinished())
        {
            using Result = std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>;
            auto promise = std::make_shared<QPromise<Result>>();
            promise->start();
            auto future = promise->future();
            auto* watcher = new QFutureWatcher<Result>(this);
            Q_CHECK_PTR(watcher);
            connect(watcher,
                    &QFutureWatcher<Result>::finished,
                    watcher,
                    [this, tf, date, watcher, promise, firstAligned, lastAligned]()
                    {
                        const auto result = watcher->result();
                        if (!result.has_value())
                        {
                            promise->addResult(std::unexpected(result.error()));
                        }
                        else
                        {
                            auto cached = getBarsFromCache(tf, date, firstAligned, lastAligned);
                            if (cached.has_value())
                            {
                                promise->addResult(std::shared_ptr<QVector<Bar>>(std::move(*cached)));
                            }
                            else
                            {
                                qCWarning(LOGGING_CATEGORY)
                                    << objectName() << "Backfill completed without the requested cache range";
                                promise->addResult(std::unexpected(TSClient::Error::Other));
                            }
                        }
                        promise->finish();
                        watcher->deleteLater();
                    });
            watcher->setFuture(pending);
            return future;
        }
    }

    if (isCurrentDay && (tf == TimeFrame::ONE_MINUTE || tf == TimeFrame::TEN_SECONDS))
    {
        prefillNullBarsThrough(tf, date, floorTimeToBarBoundary(tf, now.time()));
    }

    bool bypassDatabaseForCurrentDayTailBackfill = false;
    std::optional<QDateTime> backfillStart;

    // Past days are complete cache entries; today's saved tail needs independent coverage checking.
    if (std::optional<std::unique_ptr<QVector<Bar>>> barsFromCache =
            getBarsFromCache(tf, date, firstAligned, lastAligned);
        barsFromCache.has_value())
    {
        std::unique_ptr<QVector<Bar>> cachedBars = std::move(barsFromCache.value());

        const bool isLiveWarmableCurrentDay =
            isCurrentDay && (tf == TimeFrame::ONE_MINUTE || tf == TimeFrame::TEN_SECONDS);
        bool shouldForceLiveCurrentDayFetch = false;
        if (isLiveWarmableCurrentDay)
        {
            backfillStart = currentDayBackfillStart(tf, date, now);
            if (backfillStart.has_value())
            {
                shouldForceLiveCurrentDayFetch = true;
                bypassDatabaseForCurrentDayTailBackfill = getLatestClosedBarTimestamp(tf, date).has_value();
                DEBUG << "Backfilling current-day history for" << m_symbol << "from saved anchor" << *backfillStart;
            }
        }

        if (!shouldForceLiveCurrentDayFetch)
        {
            DEBUG << "Returning bars from in-memory cache for day" << date;
            return std::shared_ptr<QVector<Bar>>(std::move(cachedBars));
        }

        if (bypassDatabaseForCurrentDayTailBackfill)
        {
            DEBUG << "Ignoring stale current-day cache tail for" << date
                  << "and bypassing database cache to request API backfill";
        }
        else
        {
            DEBUG << "Ignoring all-null current-day cache range for" << date << "and requesting historical backfill";
        }
    }

    DEBUG << " m_barCacheByTimeFrame map doesn't contain bars for day " << date << "checking database";

    // Create a shared promise for the final result.
    // Using shared_ptr because the promise needs to survive across multiple nested async
    // continuations. Moving a QPromise into nested lambdas causes undefined behavior when
    // Qt's continuation machinery accesses the moved-from promise in the outer lambda.
    QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>> promise;
    auto future = promise.future();
    promise.start();
    auto sharedPromise =
        std::make_shared<QPromise<std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>>>(std::move(promise));
    if (isCurrentDay)
    {
        m_currentDayHistoryRequests[tf][date] = future;
    }

    // Always fetch the FULL day from database (to warm cache) even if only partial range requested
    // This ensures we cache complete days and avoid repeated database queries for the same day
    QTime fullDayStart = TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION; // 4:00 AM
    QTime fullDayEnd = TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION;        // 6:59 PM

    // Query database via DatabaseThread (async, thread-safe)
    // Request the FULL day, not just the requested range
    auto databaseBars =
        isCurrentDay ? DatabaseThread::getInstance()
                           ->getCachedBarsForDate(m_symbol, tf, date)
                           .then(
                               [](std::shared_ptr<QVector<Bar>> bars) -> std::optional<std::shared_ptr<QVector<Bar>>>
                               {
                                   if (bars->isEmpty())
                                   {
                                       return std::nullopt;
                                   }
                                   return bars;
                               })
                     : DatabaseThread::getInstance()->getBarsFromDatabase(m_symbol, tf, date, fullDayStart, fullDayEnd);
    databaseBars
        .then(
            [barCache = QPointer<BarCache>(this),
             symbol = m_symbol,
             tf,
             date,
             first = firstAligned,
             last = lastAligned,
             fullDayStart,
             fullDayEnd,
             isCurrentDay,
             now,
             bypassDatabaseForCurrentDayTailBackfill,
             backfillStart,
             sharedPromise](std::optional<std::shared_ptr<QVector<Bar>>>&& dbBars) mutable
            {
                if (barCache.isNull())
                {
                    sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                    sharedPromise->finish();
                    return;
                }

                QMetaObject::invokeMethod(
                    barCache,
                    [barCache,
                     symbol,
                     tf,
                     date,
                     first,
                     last,
                     fullDayStart,
                     fullDayEnd,
                     isCurrentDay,
                     now,
                     bypassDatabaseForCurrentDayTailBackfill,
                     backfillStart,
                     sharedPromise,
                     dbBars = std::move(dbBars)]() mutable
                    {
                        if (barCache.isNull())
                        {
                            sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                            sharedPromise->finish();
                            return;
                        }

                        BarCache* const self = barCache.data();

                        if (isCurrentDay && (tf == TimeFrame::ONE_MINUTE || tf == TimeFrame::TEN_SECONDS) &&
                            !bypassDatabaseForCurrentDayTailBackfill && dbBars.has_value())
                        {
                            self->m_currentDayBackfillStartByTimeFrame[tf].remove(date);
                            backfillStart = self->currentDayBackfillStart(tf, date, now, dbBars->get());
                            self->storeBarsInCache(tf, date, *dbBars);
                            if (backfillStart.has_value())
                            {
                                bypassDatabaseForCurrentDayTailBackfill = true;
                            }
                        }

                        // Check if we got bars from database
                        if (dbBars.has_value() && !bypassDatabaseForCurrentDayTailBackfill)
                        {
                            std::shared_ptr<QVector<Bar>> fullDayBars = std::move(dbBars.value());

                            qCDebug(LOGGING_CATEGORY)
                                << self->objectName() << "Loaded full day from database:" << fullDayBars->size()
                                << "bars";

                            // Store the complete day in memory cache
                            self->storeBarsInCache(tf, date, fullDayBars);

                            // Now filter to the requested range and return
                            auto filteredBars = std::make_shared<QVector<Bar>>();
                            for (const auto& bar: *fullDayBars)
                            {
                                if (bar.getTimeStamp().time() >= first && bar.getTimeStamp().time() <= last)
                                {
                                    filteredBars->append(bar);
                                }
                            }

                            qCDebug(LOGGING_CATEGORY)
                                << self->objectName() << "Returning filtered bars:" << filteredBars->size()
                                << "out of full day:" << fullDayBars->size();

                            sharedPromise->addResult(filteredBars);
                            sharedPromise->finish();
                            return;
                        }

                        if (dbBars.has_value() && bypassDatabaseForCurrentDayTailBackfill)
                        {
                            qCDebug(LOGGING_CATEGORY)
                                << self->objectName()
                                << "Bypassing current-day database cache hit to force API tail backfill";
                        }

                        // Database doesn't have complete day.
                        // Before going to the API, check if the source timescale is already in
                        // memory and aggregate from it — avoids a redundant Databento fetch.
                        // Example: switching 1m→5m when 1m bars are already cached in memory.
                        const TimeFrame sourceTf = BarUtils::aggregateSourceTimeFrame(tf);
                        if (sourceTf != tf)
                        {
                            auto sourceIt = self->m_barCacheByTimeFrame.constFind(sourceTf);
                            if (sourceIt != self->m_barCacheByTimeFrame.constEnd())
                            {
                                auto dayIt = sourceIt->constFind(date);
                                if (dayIt != sourceIt->constEnd() && !dayIt->isEmpty())
                                {
                                    qCDebug(LOGGING_CATEGORY)
                                        << self->objectName() << "Aggregating" << static_cast<int>(tf)
                                        << "m bars from in-memory" << static_cast<int>(sourceTf)
                                        << "m cache (no API fetch needed)";

                                    QVector<Bar> aggregated = BarUtils::aggregateBars(*dayIt, tf);

                                    auto fullDayBars = std::make_shared<QVector<Bar>>(self->fillHolesOfReceivedRequest(
                                        tf,
                                        QDateTime(date,
                                                  TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                                  TradingHours::MARKET_TIMEZONE),
                                        QDateTime(date,
                                                  TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                                  TradingHours::MARKET_TIMEZONE),
                                        aggregated));

                                    self->storeBarsInCache(tf, date, fullDayBars);

                                    // Persist to database (fire-and-forget)
                                    [[maybe_unused]] auto dbFuture =
                                        DatabaseThread::getInstance()->storeBarsInDatabase(symbol,
                                                                                           tf,
                                                                                           date,
                                                                                           fullDayBars);

                                    auto filteredBars = std::make_shared<QVector<Bar>>();
                                    for (const auto& bar: *fullDayBars)
                                    {
                                        if (bar.getTimeStamp().time() >= first && bar.getTimeStamp().time() <= last)
                                            filteredBars->append(bar);
                                    }

                                    sharedPromise->addResult(filteredBars);
                                    sharedPromise->finish();
                                    return;
                                }
                            }
                        }

                        if (isCurrentDay && tf != TimeFrame::ONE_MINUTE && tf != TimeFrame::TEN_SECONDS &&
                            tf != TimeFrame::ONE_SECOND)
                        {
                            auto oneMinuteIt = self->m_barCacheByTimeFrame.constFind(TimeFrame::ONE_MINUTE);
                            if (oneMinuteIt != self->m_barCacheByTimeFrame.constEnd())
                            {
                                auto dayIt = oneMinuteIt->constFind(date);
                                if (dayIt != oneMinuteIt->constEnd() && !dayIt->isEmpty())
                                {
                                    qCDebug(LOGGING_CATEGORY) << self->objectName() << "Aggregating current-day"
                                                              << static_cast<int>(tf) << "bars from in-memory 1m cache";

                                    QVector<Bar> aggregated = BarUtils::aggregateBars(*dayIt, tf);
                                    auto fullDayBars = std::make_shared<QVector<Bar>>(self->fillHolesOfReceivedRequest(
                                        tf,
                                        QDateTime(date,
                                                  TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                                  TradingHours::MARKET_TIMEZONE),
                                        QDateTime(date,
                                                  TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                                  TradingHours::MARKET_TIMEZONE),
                                        aggregated));

                                    self->storeBarsInCache(tf, date, fullDayBars);

                                    auto filteredBars = std::make_shared<QVector<Bar>>();
                                    for (const auto& bar: *fullDayBars)
                                    {
                                        if (bar.getTimeStamp().time() >= first && bar.getTimeStamp().time() <= last)
                                        {
                                            filteredBars->append(bar);
                                        }
                                    }

                                    sharedPromise->addResult(filteredBars);
                                    sharedPromise->finish();
                                    return;
                                }
                            }
                        }

                        auto fetchFromApi = [barCache,
                                             symbol,
                                             tf,
                                             date,
                                             first,
                                             last,
                                             isCurrentDay,
                                             now,
                                             backfillStart,
                                             sharedPromise]() mutable
                        {
                            if (barCache.isNull())
                            {
                                sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                                sharedPromise->finish();
                                return;
                            }

                            BarCache* const apiCache = barCache.data();

                            qCDebug(LOGGING_CATEGORY) << apiCache->objectName() << "No local replay warmup path for"
                                                      << static_cast<int>(tf) << "— fetching historical bars from API";

                            LTTnG_TP(opentraderplatform,
                                     barcache_api_fetch_start,
                                     symbol.toUtf8().constData(),
                                     static_cast<int>(tf),
                                     date.toString("yyyy-MM-dd").toUtf8().constData());

                            QDateTime startDateTime = backfillStart.value_or(
                                QDateTime(date,
                                          TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                          TradingHours::MARKET_TIMEZONE));

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

                            qCDebug(LOGGING_CATEGORY)
                                << apiCache->objectName() << "Fetching historical bars from API:" << startDateTime
                                << "to" << endDayTime;

                            TSClient::BarUnit barUnit = TSClient::BarUnit::Minute;
                            unsigned int interval = 1;
                            switch (tf)
                            {
                            case TimeFrame::ONE_SECOND:
                            case TimeFrame::TEN_SECONDS:
                                qCWarning(LOGGING_CATEGORY)
                                    << apiCache->objectName()
                                    << "TradeStation historical API does not support sub-minute bars for" << symbol
                                    << "tf" << static_cast<int>(tf);
                                sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                                sharedPromise->finish();
                                return;
                            case TimeFrame::ONE_MINUTE:
                                interval = 1;
                                barUnit = TSClient::BarUnit::Minute;
                                break;
                            case TimeFrame::FIVE_MINUTES:
                                interval = 5;
                                barUnit = TSClient::BarUnit::Minute;
                                break;
                            case TimeFrame::FIFTEEN_MINUTES:
                                interval = 15;
                                barUnit = TSClient::BarUnit::Minute;
                                break;
                            case TimeFrame::THIRTY_MINUTES:
                                interval = 30;
                                barUnit = TSClient::BarUnit::Minute;
                                break;
                            case TimeFrame::ONE_HOUR:
                                interval = 60;
                                barUnit = TSClient::BarUnit::Minute;
                                break;
                            case TimeFrame::FOUR_HOURS:
                                interval = 240;
                                barUnit = TSClient::BarUnit::Minute;
                                break;
                            case TimeFrame::ONE_DAY:
                                interval = 1;
                                barUnit = TSClient::BarUnit::Daily;
                                break;
                            case TimeFrame::ONE_WEEK:
                                interval = 1;
                                barUnit = TSClient::BarUnit::Weekly;
                                break;
                            case TimeFrame::ONE_MONTH:
                                interval = 1;
                                barUnit = TSClient::BarUnit::Monthly;
                                break;
                            }

                            TSClient::getInstance()
                                ->getBars(symbol,
                                          interval,
                                          barUnit,
                                          0,
                                          TSClient::BarSessionTemplate::USEQ24Hour,
                                          startDateTime,
                                          endDayTime)
                                .then(
                                    [barCache,
                                     sharedPromise,
                                     symbol,
                                     tf,
                                     date,
                                     first,
                                     last,
                                     isCurrentDay,
                                     startDateTime,
                                     endDayTime](
                                        std::expected<std::shared_ptr<QVector<Bar>>, TSClient::Error>&& barsResult)
                                    {
                                        if (barCache.isNull())
                                        {
                                            sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                                            sharedPromise->finish();
                                            return;
                                        }

                                        QMetaObject::invokeMethod(
                                            barCache,
                                            [barCache,
                                             sharedPromise,
                                             symbol,
                                             tf,
                                             date,
                                             first,
                                             last,
                                             isCurrentDay,
                                             startDateTime,
                                             endDayTime,
                                             barsResult = std::move(barsResult)]() mutable
                                            {
                                                if (barCache.isNull())
                                                {
                                                    sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                                                    sharedPromise->finish();
                                                    return;
                                                }

                                                if (!barsResult.has_value())
                                                {
                                                    qCCritical(LOGGING_CATEGORY)
                                                        << barCache->objectName()
                                                        << "TradeStation historical fetch failed for" << symbol
                                                        << "error" << static_cast<int>(barsResult.error());
                                                    sharedPromise->addResult(std::unexpected(barsResult.error()));
                                                    sharedPromise->finish();
                                                    return;
                                                }

                                                BarCache* const fetchedCache = barCache.data();
                                                const std::shared_ptr<QVector<Bar>> fullDayBars = barsResult.value();
                                                if (fullDayBars->isEmpty() && !isCurrentDay)
                                                {
                                                    qCDebug(LOGGING_CATEGORY)
                                                        << fetchedCache->objectName()
                                                        << "Historical fetch returned 0 bars for" << symbol;
                                                    sharedPromise->addResult(std::make_shared<QVector<Bar>>());
                                                    sharedPromise->finish();
                                                    return;
                                                }
                                                if (!fullDayBars->isEmpty())
                                                {
                                                    qCDebug(LOGGING_CATEGORY)
                                                        << fetchedCache->objectName()
                                                        << "TradeStation historical fetch returned"
                                                        << fullDayBars->size() << "bars for" << symbol << "— first:"
                                                        << fullDayBars->first().getTimeStamp().toString(Qt::ISODate)
                                                        << "last:"
                                                        << fullDayBars->last().getTimeStamp().toString(Qt::ISODate);
                                                }

                                                LTTnG_TP(opentraderplatform,
                                                         barcache_api_fetch_done,
                                                         symbol.toUtf8().constData(),
                                                         static_cast<int>(tf),
                                                         date.toString("yyyy-MM-dd").toUtf8().constData(),
                                                         static_cast<int>(fullDayBars->size()));

                                                const QDateTime fullDayFirst = startDateTime;
                                                const QDateTime fullDayLast(
                                                    date,
                                                    floorTimeToBarBoundary(tf, endDayTime.time()),
                                                    TradingHours::MARKET_TIMEZONE);
                                                auto filledBars = std::make_shared<QVector<Bar>>(
                                                    fetchedCache->fillHolesOfReceivedRequest(tf,
                                                                                             fullDayFirst,
                                                                                             fullDayLast,
                                                                                             *fullDayBars));

                                                fetchedCache->storeBarsInCache(tf, date, filledBars);
                                                if (isCurrentDay)
                                                {
                                                    fetchedCache
                                                        ->m_currentDayHistoryVerifiedThroughByTimeFrame[tf][date] =
                                                        fullDayLast.addSecs(-BarUtils::secondsPerBar(tf));
                                                    fetchedCache->m_currentDayBackfillStartByTimeFrame[tf].remove(date);
                                                    // Persist the merged request range, never null placeholders over live bars.
                                                    filledBars = std::shared_ptr<QVector<Bar>>(
                                                        std::move(fetchedCache
                                                                      ->getBarsFromCache(tf,
                                                                                         date,
                                                                                         fullDayFirst.time(),
                                                                                         fullDayLast.time())
                                                                      .value()));
                                                }

                                                [[maybe_unused]] auto dbFuture =
                                                    DatabaseThread::getInstance()->storeBarsInDatabase(symbol,
                                                                                                       tf,
                                                                                                       date,
                                                                                                       filledBars);

                                                auto filteredBars = std::make_shared<QVector<Bar>>();
                                                const auto resultBars =
                                                    fetchedCache->getBarsFromCache(tf, date, first, last);
                                                if (!resultBars.has_value())
                                                {
                                                    qCWarning(LOGGING_CATEGORY)
                                                        << fetchedCache->objectName()
                                                        << "Historical backfill did not populate the requested range";
                                                    sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                                                    sharedPromise->finish();
                                                    return;
                                                }
                                                *filteredBars = **resultBars;

                                                qCDebug(LOGGING_CATEGORY)
                                                    << fetchedCache->objectName()
                                                    << "Historical backfill complete:" << filteredBars->size()
                                                    << "bars (of" << filledBars->size() << "total)";

                                                sharedPromise->addResult(filteredBars);
                                                sharedPromise->finish();
                                            },
                                            Qt::QueuedConnection);
                                    });
                        };

                        const bool canWarmFromReplayTrades = MainApp::isInReplayMode() && tf != TimeFrame::ONE_SECOND &&
                                                             tf != TimeFrame::TEN_SECONDS &&
                                                             DBClient::hasReplayData(date, symbol);

                        if (!canWarmFromReplayTrades)
                        {
                            fetchFromApi();
                            return;
                        }

                        qCDebug(LOGGING_CATEGORY) << self->objectName() << "Reconstructing full trading day from local"
                                                  << "replay trades before API fallback for" << symbol << "on" << date
                                                  << "tf" << static_cast<int>(tf);

                        DBClient::loadReplayOneMinuteBarsFromTrades(symbol, date)
                            .then(
                                [barCache, symbol, tf, date, first, last, sharedPromise, fetchFromApi](
                                    std::optional<std::shared_ptr<QVector<Bar>>>&& replayBars) mutable
                                {
                                    if (barCache.isNull())
                                    {
                                        sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                                        sharedPromise->finish();
                                        return;
                                    }

                                    QMetaObject::invokeMethod(
                                        barCache,
                                        [barCache,
                                         symbol,
                                         tf,
                                         date,
                                         first,
                                         last,
                                         sharedPromise,
                                         fetchFromApi,
                                         replayBars = std::move(replayBars)]() mutable
                                        {
                                            if (barCache.isNull())
                                            {
                                                sharedPromise->addResult(std::unexpected(TSClient::Error::Other));
                                                sharedPromise->finish();
                                                return;
                                            }

                                            if (!replayBars.has_value())
                                            {
                                                qCDebug(LOGGING_CATEGORY)
                                                    << barCache->objectName()
                                                    << "Local replay trade reconstruction unavailable for" << symbol
                                                    << "on" << date << "- falling back to API";
                                                fetchFromApi();
                                                return;
                                            }

                                            BarCache* const replayCache = barCache.data();
                                            auto oneMinuteBars = std::move(replayBars.value());
                                            ASSUME_EQUAL(oneMinuteBars->size(),
                                                         BarUtils::barsPerDay(TimeFrame::ONE_MINUTE));

                                            replayCache->storeBarsInCache(TimeFrame::ONE_MINUTE, date, oneMinuteBars);
                                            [[maybe_unused]] auto minuteDbFuture =
                                                DatabaseThread::getInstance()->storeBarsInDatabase(
                                                    symbol,
                                                    TimeFrame::ONE_MINUTE,
                                                    date,
                                                    oneMinuteBars);

                                            std::shared_ptr<QVector<Bar>> requestedFullDayBars = oneMinuteBars;
                                            if (tf != TimeFrame::ONE_MINUTE)
                                            {
                                                const QDateTime fullDayFirst(
                                                    date,
                                                    TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION,
                                                    TradingHours::MARKET_TIMEZONE);
                                                const QDateTime fullDayLast(
                                                    date,
                                                    TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION,
                                                    TradingHours::MARKET_TIMEZONE);

                                                QVector<Bar> aggregated = BarUtils::aggregateBars(*oneMinuteBars, tf);
                                                requestedFullDayBars = std::make_shared<QVector<Bar>>(
                                                    replayCache->fillHolesOfReceivedRequest(tf,
                                                                                            fullDayFirst,
                                                                                            fullDayLast,
                                                                                            aggregated));

                                                replayCache->storeBarsInCache(tf, date, requestedFullDayBars);
                                                [[maybe_unused]] auto requestedDbFuture =
                                                    DatabaseThread::getInstance()
                                                        ->storeBarsInDatabase(symbol, tf, date, requestedFullDayBars);
                                            }

                                            auto filteredBars = std::make_shared<QVector<Bar>>();
                                            for (const auto& bar: *requestedFullDayBars)
                                            {
                                                if (bar.getTimeStamp().time() >= first &&
                                                    bar.getTimeStamp().time() <= last)
                                                {
                                                    filteredBars->append(bar);
                                                }
                                            }

                                            qCDebug(LOGGING_CATEGORY)
                                                << replayCache->objectName()
                                                << "Local replay warmup complete:" << filteredBars->size()
                                                << "bars returned from" << requestedFullDayBars->size()
                                                << "cached full-day bars";

                                            sharedPromise->addResult(filteredBars);
                                            sharedPromise->finish();
                                        },
                                        Qt::QueuedConnection);
                                });
                    },
                    Qt::QueuedConnection);
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
        LTTnG_TP(opentraderplatform,
               barcache_l1_miss,
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

    LTTnG_TP(opentraderplatform,
           barcache_l1_hit,
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
            else if (dayVector[index].getBarStatus() == Bar::BarStatus::Null ||
                     dayVector[index].getBarStatus() == Bar::BarStatus::Open)
            {
                DEBUG << "Replacing provisional bar at index" << index << "for" << bar.getTimeStamp();
            }
            else
            {
                WARNING << "Overwriting existing bar at index" << index << "for" << bar.getTimeStamp();
            }
        }
    }

    // Store the bar at the appropriate index
    dayVector[index] = bar;

    LTTnG_TP(opentraderplatform, barcache_store_bar_live, m_symbol.toUtf8().constData(), static_cast<int>(tf), index);

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
            // Current day may now be stored either as a partial prefix up to "now"
            // or as a full null-padded trading day for live backfill warmup.
            OBJ_ASSUME_TRUE(bars->size() == BarUtils::barsPerDay(tf) ||
                            bars->size() <= static_cast<qsizetype>(
                                                MainApp::getCurrentAppTime().time() >
                                                        TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION
                                                    ? BarUtils::barsPerDay(tf)
                                                    : BarUtils::barIndex(tf, MainApp::getCurrentAppTime().time()) + 1));
        }
    }

    OBJ_ASSUME_EQUAL(bars->first().getTimeStamp().date(), bars->last().getTimeStamp().date());
    OBJ_ASSUME_GTE(bars->first().getTimeStamp().time(), TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION);
    OBJ_ASSUME_LTE(bars->last().getTimeStamp().time(), TradingHours::TIME_LAST_CANDLE_AFTER_MARKET_SESSION);

    QWriteLocker locker(&m_barCacheRwLock);

    const bool mergeCurrentDay = !MainApp::isInReplayMode() && date == MainApp::getCurrentAppTime().date();
    if (bars->size() == BarUtils::barsPerDay(tf) && !mergeCurrentDay)
    {
        // Full day
        m_barCacheByTimeFrame[tf].insert(date, *bars);

        DEBUG << "Inserted full day in cache for" << date;

        LTTnG_TP(opentraderplatform,
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
            if (!mergeCurrentDay || BarHistoryBackfill::shouldReplace(dayVector[index], bar))
            {
                dayVector[index] = bar;
            }
        }

        DEBUG << "Inserted partial day in cache for" << date << "with" << bars->size() << "bars";

        LTTnG_TP(opentraderplatform,
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
    DatabaseThread::getInstance()->clearDatabase(m_symbol).then(
        [barCache = QPointer<BarCache>(this), symbol = m_symbol](bool success)
        {
            if (barCache.isNull())
            {
                return;
            }

            QMetaObject::invokeMethod(
                barCache,
                [barCache, symbol, success]()
                {
                    if (barCache.isNull())
                    {
                        return;
                    }

                    if (success)
                    {
                        qCInfo(LOGGING_CATEGORY)
                            << barCache->objectName() << "Successfully cleared database for" << symbol;
                    }
                    else
                    {
                        qCWarning(LOGGING_CATEGORY)
                            << barCache->objectName() << "Failed to clear database for" << symbol;
                    }
                },
                Qt::QueuedConnection);
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
