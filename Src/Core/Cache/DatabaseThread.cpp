#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QDir>
#include <QPromise>
#include <QTimeZone>

#include "DatabaseThread.h"
#include "Logging.h"
#include "Assume.h"
#include "SQL/DatabaseThreadQueries.h"

#define LOGGING_CATEGORY DatabaseThreadLog

Q_LOGGING_CATEGORY(DatabaseThreadLog, "DatabaseThread")

// Initialize static member
DatabaseThread* DatabaseThread::m_instance = nullptr;

DatabaseThread* DatabaseThread::getInstance()
{
    if (m_instance == nullptr)
    {
        qCDebug(DatabaseThreadLog) << "Singleton instance created";
        m_instance = new DatabaseThread();
    }
    return m_instance;
}

DatabaseThread::DatabaseThread() : QObject()
{
    m_thread.setObjectName("DatabaseThread");
    this->moveToThread(&m_thread);

    connect(&m_thread, &QThread::started, this, &DatabaseThread::onThreadStarted);
}

DatabaseThread::~DatabaseThread()
{
    // Close all database connections
    for (const QString& connectionName: m_databases.keys())
    {
        m_databases[connectionName].close();
    }
    m_databases.clear();

    m_thread.quit();
    m_thread.wait();
}

void DatabaseThread::start()
{
    m_thread.start();
}

void DatabaseThread::onThreadStarted()
{
    INFO << "Database thread started";
}

// ============================================================================
// Public async methods
// ============================================================================

QFuture<bool> DatabaseThread::openDatabase(const QString& symbol, const QString& dbPath)
{
    QPromise<bool> promise;
    QFuture<bool> future = promise.future();
    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, symbol, dbPath, promise = std::move(promise)]() mutable
        {
            bool result = openDatabaseInternal(symbol, dbPath);
            promise.addResult(result);
            promise.finish();
        },
        Qt::QueuedConnection);

    return future;
}

void DatabaseThread::closeDatabase(const QString& symbol)
{
    QMetaObject::invokeMethod(this, [this, symbol]() { closeDatabaseInternal(symbol); }, Qt::QueuedConnection);
}

QFuture<std::optional<std::unique_ptr<QVector<Bar>>>>
DatabaseThread::getBarsFromDatabase(const QString& symbol, QDate date, QTime start, QTime end)
{
    // Make sure we're not called from the database thread itself, that
    // would be illogical.
    OBJ_ASSUME_FALSE(this->thread() == QThread::currentThread());

    QPromise<std::optional<std::unique_ptr<QVector<Bar>>>> promise;
    QFuture<std::optional<std::unique_ptr<QVector<Bar>>>> future = promise.future();
    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, symbol, date, start, end, promise = std::move(promise)]() mutable
        {
            auto result = getBarsFromDatabaseInternal(symbol, date, start, end);
            promise.addResult(std::move(result));
            promise.finish();
        },
        Qt::QueuedConnection);

    return future;
}

[[nodiscard]]
QFuture<int>
DatabaseThread::storeBarsInDatabase(const QString& symbol, const QDate& date, const std::shared_ptr<QVector<Bar>> bars)
{
    QPromise<int> promise;
    QFuture<int> future = promise.future();
    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, symbol, date, barsPtr = std::move(bars), promise = std::move(promise)]() mutable
        {
            int result = storeBarsInDatabaseInternal(symbol, date, *barsPtr);
            promise.addResult(result);
            promise.finish();
        },
        Qt::QueuedConnection);

    return future;
}

QFuture<bool> DatabaseThread::clearDatabase(const QString& symbol)
{
    QPromise<bool> promise;
    QFuture<bool> future = promise.future();
    promise.start();

    QMetaObject::invokeMethod(
        this,
        [this, symbol, promise = std::move(promise)]() mutable
        {
            bool result = clearDatabaseInternal(symbol);
            promise.addResult(result);
            promise.finish();
        },
        Qt::QueuedConnection);

    return future;
}

// ============================================================================
// Internal implementations (run on database thread)
// ============================================================================

bool DatabaseThread::openDatabaseInternal(const QString& symbol, const QString& dbPath)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &m_thread);

    QString connectionName = "BarCache_" + symbol;

    // Check if already open
    if (m_databases.contains(symbol))
    {
        DEBUG << "Database already open for symbol" << symbol;
        return true;
    }

    // Create directory if needed
    QFileInfo dbInfo(dbPath);
    if (!dbInfo.dir().exists())
    {
        if (!dbInfo.dir().mkpath("."))
        {
            CRITICAL << "Failed to create cache directory:" << dbInfo.absolutePath();
            return false;
        }
        INFO << "Created cache directory:" << dbInfo.absolutePath();
    }

    bool dbFileExisted = QFileInfo::exists(dbPath);

    DEBUG << "Opening database:" << dbPath;
    DEBUG << "Connection name:" << connectionName;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);

    if (!db.open())
    {
        CRITICAL << "Failed to open database for" << symbol << ":" << db.lastError().text();
        return false;
    }

    // Create table if not exists
    QSqlQuery query(db);
    bool success = query.exec(DatabaseThreadQueries::CREATE_BAR_CACHE_TABLE);

    if (!success)
    {
        CRITICAL << "Failed to create table:" << query.lastError().text();
        db.close();
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    m_databases.insert(symbol, db);

    if (dbFileExisted)
    {
        INFO << "Opened existing database for symbol" << symbol << "at" << dbPath;
    }
    else
    {
        INFO << "Created new database for symbol" << symbol << "at" << dbPath;
    }

    return true;
}

void DatabaseThread::closeDatabaseInternal(const QString& symbol)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &m_thread);

    if (!m_databases.contains(symbol))
    {
        WARNING << "No database connection for symbol" << symbol;
        return;
    }

    QString connectionName = m_databases[symbol].connectionName();
    m_databases[symbol].close();
    m_databases.remove(symbol);
    QSqlDatabase::removeDatabase(connectionName);

    INFO << "Closed database for symbol" << symbol;
}

std::optional<std::unique_ptr<QVector<Bar>>>
DatabaseThread::getBarsFromDatabaseInternal(const QString& symbol, QDate date, QTime start, QTime end)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &m_thread);

    if (!m_databases.contains(symbol))
    {
        CRITICAL << "No database connection for symbol" << symbol;
        return std::nullopt;
    }

    QSqlDatabase& db = m_databases[symbol];

    if (!db.isOpen())
    {
        CRITICAL << "Database not open for symbol" << symbol;
        return std::nullopt;
    }

    DEBUG << "Checking database cache for" << symbol << "at date" << date << "from" << start << "to" << end;

    size_t indexStart = timeToIndex(start);
    size_t indexEnd = timeToIndex(end);

    OBJ_ASSUME_LT(indexStart, indexEnd);

    QSqlQuery query(db);
    query.prepare(DatabaseThreadQueries::SELECT_BARS_BY_DATE_AND_INDEX);
    query.addBindValue(date.toString("yyyy-MM-dd"));
    query.addBindValue(static_cast<int>(indexStart));
    query.addBindValue(static_cast<int>(indexEnd));

    std::unique_ptr<QVector<Bar>> bars = std::make_unique<QVector<Bar>>();

    if (query.exec())
    {
        while (query.next())
        {
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
            if (status == Bar::BarStatus::Null)
            {
                bar = Bar::nullBar(ts);
            }
            else
            {
                bar = Bar(ts, open, high, low, close, volume);
            }

            bars->append(bar);
        }
        INFO << "Loaded" << bars->size() << "bars from database for" << symbol;
    }
    else
    {
        WARNING << "Database query failed for" << symbol << ":" << query.lastError().text();
        return std::nullopt;
    }

    size_t expectedCount = indexEnd - indexStart + 1;
    if (bars->size() != static_cast<qsizetype>(expectedCount))
    {
        DEBUG << "Database does not have complete set of bars for" << symbol << "on date" << date << "- expected"
              << expectedCount << "bars but got" << bars->size();
        return std::nullopt;
    }

    return bars;
}

int DatabaseThread::storeBarsInDatabaseInternal(const QString& symbol, const QDate& date, const QVector<Bar>& bars)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &m_thread);

    if (!m_databases.contains(symbol))
    {
        CRITICAL << "No database connection for symbol" << symbol;
        return 0;
    }

    QSqlDatabase& db = m_databases[symbol];

    if (!db.isOpen())
    {
        CRITICAL << "Database not open for symbol" << symbol;
        return 0;
    }

    if (bars.isEmpty())
    {
        WARNING << "Attempted to store empty bars vector for" << symbol;
        return 0;
    }

    DEBUG << "Storing" << bars.size() << "bars in database for" << symbol;

    QSqlQuery query(db);
    query.prepare(DatabaseThreadQueries::INSERT_OR_REPLACE_BAR);

    int storedCount = 0;
    for (const Bar& bar: bars)
    {
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

        if (query.exec())
        {
            storedCount++;
        }
        else
        {
            WARNING << "Failed to store bar in database for" << symbol << "at" << bar.getTimeStamp().toString() << ":"
                    << query.lastError().text();
        }
    }

    INFO << "Successfully stored" << storedCount << "bars in database for" << symbol;
    return storedCount;
}

bool DatabaseThread::clearDatabaseInternal(const QString& symbol)
{
    OBJ_ASSUME_EQUAL(QThread::currentThread(), &m_thread);

    if (!m_databases.contains(symbol))
    {
        CRITICAL << "No database connection for symbol" << symbol;
        return false;
    }

    QSqlDatabase& db = m_databases[symbol];

    if (!db.isOpen())
    {
        CRITICAL << "Database not open for symbol" << symbol;
        return false;
    }

    INFO << "Clearing all bars from database for" << symbol;

    QSqlQuery query(db);
    if (query.exec(DatabaseThreadQueries::DELETE_ALL_BARS))
    {
        INFO << "Successfully cleared database for" << symbol;
        return true;
    }
    else
    {
        WARNING << "Failed to clear database for" << symbol << ":" << query.lastError().text();
        return false;
    }
}

// ============================================================================
// Helper functions
// ============================================================================

size_t DatabaseThread::timeToIndex(const QTime& time)
{
    ASSUME_GTE(time, TradingHours::TRADING_START_TIME);
    ASSUME_LTE(time, TradingHours::TRADING_END_TIME);

    size_t minutesSince6AM = (time.hour() - TradingHours::TRADING_START_TIME.hour()) * 60 + time.minute();
    size_t index = minutesSince6AM - 1;

    ASSUME_LT(index, TradingHours::BARS_PER_DAY);
    return index;
}

QTime DatabaseThread::indexToTime(size_t index)
{
    ASSUME_LT(index, TradingHours::BARS_PER_DAY);

    size_t adjustedMinutes = index + 1;
    int hour = TradingHours::TRADING_START_TIME.hour() + (adjustedMinutes / 60);
    int minute = adjustedMinutes % 60;

    return QTime(hour, minute, 0);
}
