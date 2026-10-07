#include "PositionsDatabase.h"
#include "PositionPnL.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include "Logging.h"
#include "Settings.h"
#include "SQL/PositionsDatabaseQueries.h"
#include "Assume.h"
#include "LedgerPaths.h"
#include "MainApp.h"
#include "TSClient.h"

#define LOGGING_CATEGORY PositionsDatabaseLog
Q_LOGGING_CATEGORY(PositionsDatabaseLog, "PositionsDatabase");

// Static singleton instance
PositionsDatabase* PositionsDatabase::s_instance = nullptr;

namespace
{
    QMutex s_instanceMutex;

    /**
 * @brief Determines the ledger database path for the current platform mode.
     *
     * Database structure:
 * - Live:       ~/.local/share/OpenTraderPlatform/Ledgers/Live/Ledger.db
 * - Simulation: ~/.local/share/OpenTraderPlatform/Ledgers/Simulation/Ledger.db
 * - Replay:     ~/.local/share/OpenTraderPlatform/Ledgers/Replay/Ledger_YYYY-MM-DD_HHMMSS.db
 * - Review:     Opens an existing replay ledger from the same Replay directory
     *
 * @return Full path to the combined ledger database file
     */
    QString determineDatabasePath()
    {
        return LedgerPaths::currentLedgerDatabasePath();
    }
} // anonymous namespace

PositionsDatabase* PositionsDatabase::getInstance(QObject* p_parent)
{
    QMutexLocker locker(&s_instanceMutex);
    if (s_instance == nullptr)
    {
        QString dbPath = determineDatabasePath();
        s_instance = new PositionsDatabase(dbPath, p_parent);
    }
    return s_instance;
}

void PositionsDatabase::destroyInstance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (s_instance != nullptr)
    {
        delete s_instance;
        s_instance = nullptr;
    }
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
    m_db = QSqlDatabase(); // Release the copy before removal (Qt requirement)
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
    if (QThread::currentThread() != thread())
    {
        bool result = false;
        const bool invoked = QMetaObject::invokeMethod(
            this,
            [this, &result, p_position, p_openedDateTime]() { result = insertPosition(p_position, p_openedDateTime); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            WARNING << "Failed to marshal insertPosition to owner thread";
            return false;
        }
        return result;
    }

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

    query.addBindValue(p_position.getClosedDateTime().isValid()
                           ? QVariant(p_position.getClosedDateTime().toString(Qt::ISODate))
                           : QVariant());

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
    if (QThread::currentThread() != thread())
    {
        bool result = false;
        const bool invoked = QMetaObject::invokeMethod(
            this,
            [this, &result, p_position, p_closedDateTime]() { result = updatePosition(p_position, p_closedDateTime); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            WARNING << "Failed to marshal updatePosition to owner thread";
            return false;
        }
        return result;
    }

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
    if (QThread::currentThread() != thread())
    {
        bool result = false;
        const bool invoked = QMetaObject::invokeMethod(
            const_cast<PositionsDatabase*>(this),
            [this, &result, p_positionID]() { result = positionExists(p_positionID); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            WARNING << "Failed to marshal positionExists to owner thread";
            return false;
        }
        return result;
    }

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
    if (QThread::currentThread() != thread())
    {
        QMap<QString, Position> result;
        const bool invoked = QMetaObject::invokeMethod(
            const_cast<PositionsDatabase*>(this),
            [this, &result]() { result = loadAllPositions(); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            WARNING << "Failed to marshal loadAllPositions to owner thread";
            return {};
        }
        return result;
    }

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
        QString openedDateTimeStr = query.value(1).toString();
        QString closedDateTimeStr = query.value(2).toString();
        QString jsonDataStr = query.value(3).toString();

        // Reconstruct the Position object from JSON
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonDataStr.toUtf8());
        if (jsonDoc.isObject())
        {
            QJsonObject jsonObj = jsonDoc.object();
            if (!openedDateTimeStr.isEmpty())
            {
                jsonObj["OpenedDateTime"] = openedDateTimeStr;
            }
            if (!closedDateTimeStr.isEmpty())
            {
                jsonObj["ClosedDateTime"] = closedDateTimeStr;
            }
            Position position(jsonObj);
            if (position.isDeleted() || qFuzzyIsNull(position.getQuantity().toDouble()))
            {
                position = reconcileClosedPosition(position);
            }
            positions.insert(positionId, position);
        }
    }

    INFO << "Loaded" << positions.size() << "positions from database";
    return positions;
}

Position PositionsDatabase::reconcileClosedPosition(const Position& p_position) const
{
    if (QThread::currentThread() != thread())
    {
        Position result = p_position;
        const bool invoked = QMetaObject::invokeMethod(
            const_cast<PositionsDatabase*>(this),
            [this, &result, p_position]() { result = reconcileClosedPosition(p_position); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            WARNING << "Failed to marshal position P&L reconciliation to owner thread";
            result.setRealizedProfitLoss(std::nullopt);
            result.setPeakQuantity(std::nullopt);
        }
        return result;
    }

    Position result = p_position;
    result.setRealizedProfitLoss(std::nullopt);
    result.setPeakQuantity(std::nullopt);
    QSqlQuery query(m_db);
    query.prepare(PositionsDatabaseQueries::SELECT_POSITION_ORDERS);
    query.addBindValue(p_position.getAccountID());
    query.addBindValue(p_position.getSymbol());
    if (!query.exec())
    {
        WARNING << "Failed to load position fills:" << query.lastError().text();
        return result;
    }

    QVector<Order> orders;
    while (query.next())
    {
        const auto document = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
        if (!document.isObject())
        {
            WARNING << "Invalid order JSON while reconciling position" << p_position.getPositionID();
            return result;
        }
        auto json = document.object();
        json["Status"] = query.value(1).toString();
        json["FilledPrice"] = query.value(2).toDouble();
        json["OpenedDateTime"] = query.value(3).toString();
        json["ClosedDateTime"] = query.value(4).toString();
        orders.append(Order(json));
    }

    const auto calculation = PositionPnL::calculateClosedPosition(p_position, orders);
    result.setRealizedProfitLoss(calculation.grossProfit);
    result.setPeakQuantity(calculation.peakQuantity);
    if (!calculation.grossProfit.has_value())
    {
        WARNING << "Realized P&L unavailable for position" << p_position.getPositionID() << ":" << calculation.reason;
    }
    return result;
}

bool PositionsDatabase::isOpen() const
{
    if (QThread::currentThread() != thread())
    {
        bool result = false;
        const bool invoked = QMetaObject::invokeMethod(
            const_cast<PositionsDatabase*>(this),
            [this, &result]() { result = isOpen(); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            WARNING << "Failed to marshal isOpen to owner thread";
            return false;
        }
        return result;
    }

    return m_db.isOpen();
}

int PositionsDatabase::getPositionCount() const
{
    if (QThread::currentThread() != thread())
    {
        int result = 0;
        const bool invoked = QMetaObject::invokeMethod(
            const_cast<PositionsDatabase*>(this),
            [this, &result]() { result = getPositionCount(); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            WARNING << "Failed to marshal getPositionCount to owner thread";
            return 0;
        }
        return result;
    }

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
    if (QThread::currentThread() != thread())
    {
        bool result = false;
        const bool invoked = QMetaObject::invokeMethod(
            this,
            [this, &result]() { result = clearAllPositions(); },
            Qt::BlockingQueuedConnection);
        if (!invoked)
        {
            WARNING << "Failed to marshal clearAllPositions to owner thread";
            return false;
        }
        return result;
    }

    QSqlQuery query(m_db);
    if (!query.exec(PositionsDatabaseQueries::DELETE_ALL_POSITIONS))
    {
        WARNING << "Failed to clear positions:" << query.lastError().text();
        return false;
    }

    DEBUG << "Cleared all positions from database";
    return true;
}
