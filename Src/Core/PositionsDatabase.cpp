#include "PositionsDatabase.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>

#include "Logging.h"
#include "Settings.h"
#include "SQL/PositionsDatabaseQueries.h"
#include "Assume.h"
#include "MainApp.h"
#include "TSClient.h"

#define LOGGING_CATEGORY PositionsDatabaseLog
Q_LOGGING_CATEGORY(PositionsDatabaseLog, "PositionsDatabase");

// Static singleton instance
PositionsDatabase* PositionsDatabase::s_instance = nullptr;

namespace
{
    /**
 * @brief Determines the database path based on current trading mode
 *
 * Database structure:
 * - Live:       ~/.cache/L2Trader/Positions/Live/Positions.db
 * - Simulation: ~/.cache/L2Trader/Positions/Simulation/Positions.db
 * - Replay:     ~/.cache/L2Trader/Positions/Replay/Positions_YYYY-MM-DD_HHMMSS.db
 *
 * @return Full path to the positions database file
 */
    QString determineDatabasePath()
    {
        QString baseDir = getCacheLocation();
        baseDir += "/Positions/";

        // Check TSClient mode first - if Replay, use replay path
        TSClient* client = TSClient::getInstance();
        if (client && client->getMode() == TSClient::Mode::Replay)
        {
            QString timestamp = client->getReplaySessionTimestamp();
            ASSUME_TRUE(!timestamp.isEmpty());

            QString replayDir = baseDir + "Replay/";
            QDir().mkpath(replayDir);
            return replayDir + "Positions_" + timestamp + ".db";
        }

        // Otherwise check TradingMode (Live vs Sim)
        TradingMode tradingMode = MainApp::getTradingMode();
        if (tradingMode == TradingMode::Sim)
        {
            QString simDir = baseDir + "Simulation/";
            QDir().mkpath(simDir);
            return simDir + "Positions.db";
        }

        // Default to Live
        QString liveDir = baseDir + "Live/";
        QDir().mkpath(liveDir);
        return liveDir + "Positions.db";
    }
} // anonymous namespace

PositionsDatabase* PositionsDatabase::getInstance(QObject* p_parent)
{
    if (s_instance == nullptr)
    {
        QString dbPath = determineDatabasePath();
        s_instance = new PositionsDatabase(dbPath, p_parent);
    }
    return s_instance;
}

PositionsDatabase::PositionsDatabase(const QString& p_dbPath, QObject* p_parent)
    : QObject(p_parent), m_dbPath(p_dbPath), m_connectionName("PositionsDB") // Use fixed connection name for singleton
{
    setObjectName("PositionsDatabase");

    // Ensure the directory exists
    QFileInfo fileInfo(p_dbPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            CRITICAL << "Failed to create directory for positions database:" << dir.path();
            return;
        }
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(p_dbPath);

    if (!m_db.open())
    {
        CRITICAL << "Failed to open positions database:" << m_db.lastError().text();
        return;
    }

    createTable();
    INFO << "Positions database opened at" << p_dbPath;
}

PositionsDatabase::~PositionsDatabase()
{
    if (m_db.isOpen())
    {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    s_instance = nullptr;
}

void PositionsDatabase::createTable()
{
    QSqlQuery query(m_db);

    if (!query.exec(PositionsDatabaseQueries::CREATE_POSITIONS_TABLE))
    {
        CRITICAL << "Failed to create positions table:" << query.lastError().text();
        // Failed to create positions table
        Q_UNREACHABLE();
    }
}

bool PositionsDatabase::insertPosition(const Position& p_position, const QDateTime& p_openedDateTime)
{
    QSqlQuery query(m_db);

    query.prepare(PositionsDatabaseQueries::INSERT_POSITION);
    query.addBindValue(p_position.getPositionID());
    query.addBindValue(p_position.getAccountID());
    query.addBindValue(p_position.getSymbol());
    query.addBindValue(p_position.getQuantity());
    query.addBindValue(p_position.getAveragePrice());
    query.addBindValue(p_position.getLast());
    query.addBindValue(p_position.getMarkToMarketPrice());
    query.addBindValue(p_position.getMarketValue());
    query.addBindValue(p_position.getTotalCost());
    query.addBindValue(p_position.getUnrealizedProfitLoss());
    query.addBindValue(p_position.getUnrealizedProfitLossPercent());
    query.addBindValue(p_position.getTodaysProfitLoss());
    query.addBindValue(p_position.getLongShort());
    query.addBindValue(p_position.getAssetType());
    query.addBindValue(p_position.getBid());
    query.addBindValue(p_position.getAsk());
    query.addBindValue(p_position.getConversionRate());
    query.addBindValue(p_position.getDayTradeRequirement());
    query.addBindValue(p_position.getInitialRequirement());
    query.addBindValue(p_position.getMaintenanceMargin());
    query.addBindValue(p_position.getUnrealizedProfitLossQty());
    query.addBindValue(p_position.isDeleted() ? 1 : 0);

    // Handle optional expiration date
    if (p_position.getExpirationDate().isValid())
    {
        query.addBindValue(p_position.getExpirationDate().toString(Qt::ISODate));
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    query.addBindValue(p_position.getTimestamp().toString(Qt::ISODate));

    // Add opened datetime
    if (p_openedDateTime.isValid())
    {
        query.addBindValue(p_openedDateTime.toString(Qt::ISODate));
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    // Closed datetime is NULL on insert (position just opened)
    query.addBindValue(QVariant());

    // Store the full position as JSON for easy reconstruction
    query.addBindValue(p_position.toJsonString());

    if (!query.exec())
    {
        WARNING << "Failed to insert position" << p_position.getPositionID() << ":" << query.lastError().text();
        return false;
    }

    DEBUG << "Inserted position" << p_position.getPositionID() << "into database";
    return true;
}

bool PositionsDatabase::updatePosition(const Position& p_position, std::optional<QDateTime> p_closedDateTime)
{
    QSqlQuery query(m_db);

    query.prepare(PositionsDatabaseQueries::UPDATE_POSITION);
    query.addBindValue(p_position.getQuantity());
    query.addBindValue(p_position.getAveragePrice());
    query.addBindValue(p_position.getLast());
    query.addBindValue(p_position.getMarkToMarketPrice());
    query.addBindValue(p_position.getMarketValue());
    query.addBindValue(p_position.getTotalCost());
    query.addBindValue(p_position.getUnrealizedProfitLoss());
    query.addBindValue(p_position.getUnrealizedProfitLossPercent());
    query.addBindValue(p_position.getTodaysProfitLoss());
    query.addBindValue(p_position.getBid());
    query.addBindValue(p_position.getAsk());
    query.addBindValue(p_position.isDeleted() ? 1 : 0);
    query.addBindValue(p_position.getTimestamp().toString(Qt::ISODate));

    // Add closed datetime if provided
    if (p_closedDateTime.has_value() && p_closedDateTime.value().isValid())
    {
        query.addBindValue(p_closedDateTime.value().toString(Qt::ISODate));
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    // Store the full position as JSON
    query.addBindValue(p_position.toJsonString());

    query.addBindValue(p_position.getPositionID());

    if (!query.exec())
    {
        WARNING << "Failed to update position" << p_position.getPositionID() << ":" << query.lastError().text();
        return false;
    }

    DEBUG << "Updated position" << p_position.getPositionID() << "in database";
    return true;
}

bool PositionsDatabase::positionExists(const QString& p_positionID) const
{
    QSqlQuery query(m_db);
    query.prepare(PositionsDatabaseQueries::SELECT_POSITION_EXISTS);
    query.addBindValue(p_positionID);

    if (!query.exec())
    {
        WARNING << "Failed to check if position exists:" << query.lastError().text();
        return false;
    }

    if (query.next())
    {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QMap<QString, Position> PositionsDatabase::loadAllPositions() const
{
    QMap<QString, Position> positions;

    QSqlQuery query(m_db);
    if (!query.exec(PositionsDatabaseQueries::SELECT_ALL_POSITIONS))
    {
        WARNING << "Failed to load positions from database:" << query.lastError().text();
        return positions;
    }

    while (query.next())
    {
        QString positionId = query.value(0).toString();
        QString jsonDataStr = query.value(1).toString();

        // Reconstruct the Position object from JSON
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonDataStr.toUtf8());
        if (jsonDoc.isObject())
        {
            QJsonObject jsonObj = jsonDoc.object();
            Position position(jsonObj);
            positions.insert(positionId, position);
        }
    }

    INFO << "Loaded" << positions.size() << "positions from database";
    return positions;
}

bool PositionsDatabase::isOpen() const
{
    return m_db.isOpen();
}

int PositionsDatabase::getPositionCount() const
{
    QSqlQuery query(m_db);
    if (!query.exec(PositionsDatabaseQueries::SELECT_POSITION_COUNT))
    {
        WARNING << "Failed to get position count:" << query.lastError().text();
        return 0;
    }

    if (query.next())
    {
        return query.value(0).toInt();
    }

    return 0;
}

bool PositionsDatabase::clearAllPositions()
{
    QSqlQuery query(m_db);
    if (!query.exec(PositionsDatabaseQueries::DELETE_ALL_POSITIONS))
    {
        WARNING << "Failed to clear positions:" << query.lastError().text();
        return false;
    }

    INFO << "Cleared all positions from database";
    return true;
}
