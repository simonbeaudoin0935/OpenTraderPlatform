#pragma once

#include <QObject>
#include <QThread>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QFuture>
#include <QMap>
#include <QMutex>

#include <optional>
#include <memory>

#include "Bar.h"
#include "CONSTANTS.h"
#include "TimeFrame.h"

Q_DECLARE_LOGGING_CATEGORY(DatabaseThreadLog)

/**
 * @brief Singleton thread dedicated to all database operations.
 *
 * This class ensures all QSqlDatabase operations happen on the same thread
 * that created the connection, as required by Qt's threading model.
 *
 * Usage pattern:
 * 1. Call getInstance() to get the singleton
 * 2. Call start() once at application startup
 * 3. Use openDatabase() to create a connection for a symbol
 * 4. Use getBarsFromDatabase() and storeBarsInDatabase() for data operations
 *
 * All public methods are thread-safe and can be called from any thread.
 * The actual work is executed on the dedicated database thread via
 * QMetaObject::invokeMethod with Qt::QueuedConnection.
 *
 * ## Schema versioning
 * On openDatabase(), the v2 multi-timescale schema is probed.
 * If the existing table is the old v1 schema (no timescale column), it is
 * dropped and recreated. Since the BarCache is a rebuildable cache (Databento
 * is the source of truth), no data migration is needed.
 */
class DatabaseThread final : public QObject
{
    Q_OBJECT

  public:
    // Singleton: Instance getter
    [[nodiscard]] static DatabaseThread* getInstance();

    // Singleton: Destroy instance (for cleanup)
    static void destroyInstance();

    // Delete copy/move constructors and assignment operators
    DatabaseThread(const DatabaseThread&) = delete;
    DatabaseThread(DatabaseThread&&) = delete;
    DatabaseThread& operator=(const DatabaseThread&) = delete;
    DatabaseThread& operator=(DatabaseThread&&) = delete;

    /**
     * @brief Start the database thread. Call once at application startup.
     */
    void start();

    /**
     * @brief Open or create a database for a symbol.
     *
     * On open, the schema version is checked. If the table is missing the
     * timescale column (v1 schema), it is dropped and recreated as v2.
     *
     * @param symbol The stock symbol (used as connection name)
     * @param dbPath Full path to the SQLite database file
     * @return QFuture that resolves to true on success, false on failure
     */
    [[nodiscard]] QFuture<bool> openDatabase(const QString& symbol, const QString& dbPath);

    /**
     * @brief Close a database connection for a symbol.
     * @param symbol The stock symbol
     */
    void closeDatabase(const QString& symbol);

    /**
     * @brief Retrieve bars from the database for a given timescale, date and time range.
     * @param symbol The stock symbol
     * @param tf     The timescale (determines bar index formula)
     * @param date   The date to query
     * @param start  Start time of the range
     * @param end    End time of the range
     * @return QFuture with optional vector of bars (nullopt if incomplete data)
     */
    [[nodiscard]] QFuture<std::optional<std::shared_ptr<QVector<Bar>>>>
    getBarsFromDatabase(const QString& symbol, TimeFrame tf, QDate date, QTime start, QTime end);

    /**
     * @brief Store bars in the database for a given timescale.
     * @param symbol The stock symbol
     * @param tf     The timescale of the bars
     * @param date   The date of the bars
     * @param bars   Vector of bars to store
     * @return QFuture that resolves to the number of bars successfully stored
     */
    [[nodiscard]] QFuture<int>
    storeBarsInDatabase(const QString& symbol, TimeFrame tf, const QDate& date, std::shared_ptr<QVector<Bar>> bars);

    /**
     * @brief Clear all bars from a symbol's database.
     * @param symbol The stock symbol
     * @return QFuture that resolves to true on success
     */
    [[nodiscard]] QFuture<bool> clearDatabase(const QString& symbol);

  private slots:
    void onThreadStarted();

  private:
    static DatabaseThread* m_instance;

    explicit DatabaseThread();
    ~DatabaseThread();

    // Internal implementations that run on the database thread
    bool openDatabaseInternal(const QString& symbol, const QString& dbPath);
    void closeDatabaseInternal(const QString& symbol);

    std::optional<std::shared_ptr<QVector<Bar>>>
    getBarsFromDatabaseInternal(const QString& symbol, TimeFrame tf, QDate date, QTime start, QTime end);

    int storeBarsInDatabaseInternal(const QString& symbol, TimeFrame tf, const QDate& date, const QVector<Bar>& bars);

    bool clearDatabaseInternal(const QString& symbol);

    QThread m_thread;

    // Map of symbol -> database connection
    // Only accessed from the database thread
    QMap<QString, QSqlDatabase> m_databases;
};
