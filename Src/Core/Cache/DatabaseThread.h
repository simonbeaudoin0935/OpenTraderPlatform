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
 */
class DatabaseThread final : public QObject
{
    Q_OBJECT

  public:
    // Singleton: Instance getter
    [[nodiscard]] static DatabaseThread* getInstance();

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
     * @brief Retrieve bars from the database for a given date and time range.
     * @param symbol The stock symbol
     * @param date The date to query
     * @param start Start time of the range
     * @param end End time of the range
     * @return QFuture with optional vector of bars (nullopt if incomplete data)
     */
    [[nodiscard]] QFuture<std::optional<std::unique_ptr<QVector<Bar>>>>
    getBarsFromDatabase(const QString& symbol, QDate date, QTime start, QTime end);

    /**
     * @brief Store bars in the database.
     * @param symbol The stock symbol
     * @param date The date of the bars
     * @param bars Vector of bars to store
     * @return QFuture that resolves to the number of bars successfully stored
     */
    [[nodiscard]] QFuture<int>
    storeBarsInDatabase(const QString& symbol, const QDate& date, const std::shared_ptr<QVector<Bar>> bars);

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

    std::optional<std::unique_ptr<QVector<Bar>>>
    getBarsFromDatabaseInternal(const QString& symbol, QDate date, QTime start, QTime end);

    int storeBarsInDatabaseInternal(const QString& symbol, const QDate& date, const QVector<Bar>& bars);

    bool clearDatabaseInternal(const QString& symbol);

    // Helper to convert time to index (same logic as BarCache)
    static size_t timeToIndex(const QTime& time);
    static QTime indexToTime(size_t index);

    QThread m_thread;

    // Map of symbol -> database connection
    // Only accessed from the database thread
    QMap<QString, QSqlDatabase> m_databases;

    // Trading hours constants (same as BarCache)
    static inline const QTime TRADING_START_TIME = QTime(6, 1);
    static inline const QTime TRADING_END_TIME = QTime(20, 0);
    static constexpr size_t BARS_PER_DAY = 840;
};
