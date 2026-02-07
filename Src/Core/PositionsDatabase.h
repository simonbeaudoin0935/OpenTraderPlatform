#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QLoggingCategory>
#include <optional>

#include "Position.h"

Q_DECLARE_LOGGING_CATEGORY(PositionsDatabaseLog)

/**
 * @brief Database for persisting position history (Singleton)
 *
 * This class manages a SQLite database that stores all positions received through
 * the positions stream. This allows the application to maintain position history
 * across restarts, including positions that have returned to 0 shares.
 *
 * This is a singleton class - use getInstance() to get the single instance.
 */
class PositionsDatabase : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Get the singleton instance of PositionsDatabase
     * @param p_parent Optional parent object (only used on first call)
     * @return Pointer to the singleton instance
     */
    static PositionsDatabase* getInstance(QObject* p_parent = nullptr);

    ~PositionsDatabase();

    // Delete copy constructor and assignment operator
    PositionsDatabase(const PositionsDatabase&) = delete;
    PositionsDatabase& operator=(const PositionsDatabase&) = delete;

    /**
     * @brief Insert a new position into the database
     * @param p_position The position to insert
     * @param p_openedDateTime The datetime when the position was opened (optional, defaults to current time)
     * @return true if successful, false otherwise
     */
    bool insertPosition(const Position& p_position, const QDateTime& p_openedDateTime = QDateTime::currentDateTime());

    /**
     * @brief Update an existing position in the database
     * @param p_position The position to update
     * @param p_closedDateTime The datetime when the position was closed to 0 shares (optional, only set when quantity is 0)
     * @return true if successful, false otherwise
     */
    bool updatePosition(const Position& p_position, std::optional<QDateTime> p_closedDateTime = std::nullopt);

    /**
     * @brief Check if a position exists in the database
     * @param p_positionID The position ID to check
     * @return true if the position exists, false otherwise
     */
    bool positionExists(const QString& p_positionID) const;

    /**
     * @brief Load all positions from the database
     * @return Map of position ID to Position object
     */
    QMap<QString, Position> loadAllPositions() const;

    /**
     * @brief Check if the database is open
     * @return true if open, false otherwise
     */
    bool isOpen() const;

    /**
     * @brief Get the total number of positions in the database
     * @return Total position count
     */
    int getPositionCount() const;

    /**
     * @brief Get the database file path
     * @return Database file path
     */
    QString getDatabasePath() const
    {
        return m_dbPath;
    }

    /**
     * @brief Clear all positions from the database
     * @return true if successful, false otherwise
     */
    bool clearAllPositions();

  private:
    explicit PositionsDatabase(const QString& p_dbPath, QObject* p_parent = nullptr);

    void createTable();

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_connectionName;

    static PositionsDatabase* s_instance; // Singleton instance
};
