#include "OrdersDatabase.h"

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
#include "SQL/OrdersDatabaseQueries.h"
#include "Assume.h"
#include "LedgerPaths.h"
#include "MainApp.h"
#include "TSClient.h"

#define LOGGING_CATEGORY OrdersDatabaseLog
Q_LOGGING_CATEGORY(OrdersDatabaseLog, "OrdersDatabase");

// Static singleton instance
OrdersDatabase* OrdersDatabase::s_instance = nullptr;

namespace
{
    [[nodiscard]] QString bracketActionToString(const StrategyBracketOverlayEntry::Action p_action)
    {
        switch (p_action)
        {
        case StrategyBracketOverlayEntry::Action::Upsert:
            return QStringLiteral("upsert");
        case StrategyBracketOverlayEntry::Action::Clear:
            return QStringLiteral("clear");
        }

        return QStringLiteral("upsert");
    }

    [[nodiscard]] std::optional<StrategyBracketOverlayEntry::Action> stringToBracketAction(const QString& p_action)
    {
        if (p_action.compare(QStringLiteral("upsert"), Qt::CaseInsensitive) == 0)
        {
            return StrategyBracketOverlayEntry::Action::Upsert;
        }
        if (p_action.compare(QStringLiteral("clear"), Qt::CaseInsensitive) == 0)
        {
            return StrategyBracketOverlayEntry::Action::Clear;
        }
        return std::nullopt;
    }

    [[nodiscard]] QString bracketSideToString(const StrategyBracketOverlayEntry::Side p_side)
    {
        switch (p_side)
        {
        case StrategyBracketOverlayEntry::Side::Long:
            return QStringLiteral("long");
        case StrategyBracketOverlayEntry::Side::Short:
            return QStringLiteral("short");
        }

        return QStringLiteral("long");
    }

    [[nodiscard]] std::optional<StrategyBracketOverlayEntry::Side> stringToBracketSide(const QString& p_side)
    {
        if (p_side.compare(QStringLiteral("long"), Qt::CaseInsensitive) == 0)
        {
            return StrategyBracketOverlayEntry::Side::Long;
        }
        if (p_side.compare(QStringLiteral("short"), Qt::CaseInsensitive) == 0)
        {
            return StrategyBracketOverlayEntry::Side::Short;
        }
        return std::nullopt;
    }

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

OrdersDatabase* OrdersDatabase::getInstance(QObject* p_parent)
{
    if (s_instance == nullptr)
    {
        QString dbPath = determineDatabasePath();
        s_instance = new OrdersDatabase(dbPath, p_parent);
    }
    return s_instance;
}

void OrdersDatabase::destroyInstance()
{
    if (s_instance != nullptr)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

OrdersDatabase::OrdersDatabase(const QString& p_dbPath, QObject* p_parent)
    : QObject(p_parent), m_dbPath(p_dbPath), m_connectionName("OrdersDB") // Use fixed connection name for singleton
{
    setObjectName("OrdersDatabase");

    // Ensure the directory exists
    QFileInfo fileInfo(p_dbPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            CRITICAL << "Failed to create directory for orders database:" << dir.path();
            return;
        }
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(p_dbPath);

    if (!m_db.open())
    {
        CRITICAL << "Failed to open orders database:" << m_db.lastError().text();
        return;
    }

    createTable();
    INFO << "Orders database opened at" << p_dbPath;
}

OrdersDatabase::~OrdersDatabase()
{
    if (m_db.isOpen())
    {
        m_db.close();
    }
    m_db = QSqlDatabase(); // Release the copy before removal (Qt requirement)
    QSqlDatabase::removeDatabase(m_connectionName);
    s_instance = nullptr;
}

void OrdersDatabase::createTable()
{
    QSqlQuery query(m_db);

    if (!query.exec(OrdersDatabaseQueries::CREATE_ORDERS_TABLE))
    {
        CRITICAL << "Failed to create orders table:" << query.lastError().text();
        Q_UNREACHABLE();
    }

    if (!query.exec(OrdersDatabaseQueries::CREATE_STRATEGY_LOGS_TABLE))
    {
        CRITICAL << "Failed to create strategy_logs table:" << query.lastError().text();
        Q_UNREACHABLE();
    }

    if (!query.exec(OrdersDatabaseQueries::CREATE_STRATEGY_BRACKET_OVERLAYS_TABLE))
    {
        CRITICAL << "Failed to create strategy_bracket_overlays table:" << query.lastError().text();
        Q_UNREACHABLE();
    }

    // Backward-compatible migration: older ledgers may not have reference_entry_price.
    QSqlQuery tableInfoQuery(m_db);
    if (!tableInfoQuery.exec("PRAGMA table_info(strategy_bracket_overlays)"))
    {
        CRITICAL << "Failed to inspect strategy_bracket_overlays schema:" << tableInfoQuery.lastError().text();
        Q_UNREACHABLE();
    }

    bool hasReferenceEntryPriceColumn = false;
    while (tableInfoQuery.next())
    {
        if (tableInfoQuery.value(1).toString() == QStringLiteral("reference_entry_price"))
        {
            hasReferenceEntryPriceColumn = true;
            break;
        }
    }

    if (!hasReferenceEntryPriceColumn)
    {
        QSqlQuery alterQuery(m_db);
        if (!alterQuery.exec("ALTER TABLE strategy_bracket_overlays ADD COLUMN reference_entry_price REAL"))
        {
            CRITICAL << "Failed to migrate strategy_bracket_overlays with reference_entry_price column:"
                     << alterQuery.lastError().text();
            Q_UNREACHABLE();
        }
    }
}

bool OrdersDatabase::insertOrder(const Order& p_order, std::optional<qint64> p_latencyMs)
{
    QSqlQuery query(m_db);

    query.prepare(OrdersDatabaseQueries::INSERT_ORDER);
    query.addBindValue(p_order.getOrderID());
    query.addBindValue(p_order.getAccountID());
    query.addBindValue(p_order.getSymbol());
    query.addBindValue(p_order.getQuantity());
    query.addBindValue(p_order.getTradeAction());
    query.addBindValue(QtEnum::toString(p_order.getOrderType().type));
    query.addBindValue(QtEnum::toString(p_order.getOrderStatus()));
    query.addBindValue(p_order.getStatusDescription());

    // Handle optional limit and stop prices
    if (p_order.getLimitPrice().has_value())
    {
        query.addBindValue(p_order.getLimitPrice().value());
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    if (p_order.getStopPrice().has_value())
    {
        query.addBindValue(p_order.getStopPrice().value());
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    query.addBindValue(p_order.getFilledPrice());
    query.addBindValue(p_order.getOpenedDateTime().toString(Qt::ISODate));

    // Handle closed datetime (might not be set)
    if (p_order.getClosedDateTime().isValid())
    {
        query.addBindValue(p_order.getClosedDateTime().toString(Qt::ISODate));
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    // Latency in milliseconds
    if (p_latencyMs.has_value())
    {
        query.addBindValue(p_latencyMs.value());
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    // Optional strategy log message
    if (p_order.getStrategyLog().has_value())
    {
        query.addBindValue(p_order.getStrategyLog().value());
    }
    else
    {
        query.addBindValue(QVariant()); // NULL
    }

    // Store a minimal order representation in JSON
    QJsonObject jsonObj;
    jsonObj["OrderID"] = p_order.getOrderID();
    jsonObj["AccountID"] = p_order.getAccountID();
    jsonObj["Symbol"] = p_order.getSymbol();
    jsonObj["Quantity"] = p_order.getQuantity();
    jsonObj["TradeAction"] = p_order.getTradeAction();
    jsonObj["SyntheticFill"] = p_order.m_fillIsSynthetic;
    if (p_order.m_executedQuantity.has_value())
    {
        jsonObj["ExecQuantity"] = p_order.m_executedQuantity.value();
    }
    query.addBindValue(QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact)));

    if (!query.exec())
    {
        WARNING << "Failed to insert order" << p_order.getOrderID() << ":" << query.lastError().text();
        return false;
    }

    DEBUG << "Inserted order" << p_order.getOrderID() << "into database";
    return true;
}

bool OrdersDatabase::updateOrder(const Order& p_order, std::optional<qint64> p_latencyMs)
{
    QSqlQuery query(m_db);

    query.prepare(OrdersDatabaseQueries::UPDATE_ORDER);
    query.addBindValue(QtEnum::toString(p_order.getOrderStatus()));
    query.addBindValue(p_order.getStatusDescription());
    query.addBindValue(p_order.getFilledPrice());

    // Handle closed datetime (might not be set)
    if (p_order.getClosedDateTime().isValid())
    {
        query.addBindValue(p_order.getClosedDateTime().toString(Qt::ISODate));
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    if (p_latencyMs.has_value())
    {
        query.addBindValue(p_latencyMs.value());
    }
    else
    {
        query.addBindValue(QVariant()); // NULL value for SQL
    }

    // Update JSON data
    QJsonObject jsonObj;
    jsonObj["OrderID"] = p_order.getOrderID();
    jsonObj["AccountID"] = p_order.getAccountID();
    jsonObj["Symbol"] = p_order.getSymbol();
    jsonObj["Quantity"] = p_order.getQuantity();
    jsonObj["TradeAction"] = p_order.getTradeAction();
    jsonObj["SyntheticFill"] = p_order.m_fillIsSynthetic;
    if (p_order.m_executedQuantity.has_value())
    {
        jsonObj["ExecQuantity"] = p_order.m_executedQuantity.value();
    }
    query.addBindValue(QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact)));

    query.addBindValue(p_order.getOrderID());

    if (!query.exec())
    {
        WARNING << "Failed to update order" << p_order.getOrderID() << ":" << query.lastError().text();
        return false;
    }

    DEBUG << "Updated order" << p_order.getOrderID() << "in database";
    return true;
}

bool OrdersDatabase::orderExists(const QString& p_orderID) const
{
    QSqlQuery query(m_db);
    query.prepare(OrdersDatabaseQueries::SELECT_ORDER_EXISTS);
    query.addBindValue(p_orderID);

    if (!query.exec())
    {
        WARNING << "Failed to check if order exists:" << query.lastError().text();
        return false;
    }

    if (query.next())
    {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QMap<QString, std::tuple<Order, std::optional<qint64>>> OrdersDatabase::loadAllOrders() const
{
    QMap<QString, std::tuple<Order, std::optional<qint64>>> orders;

    QSqlQuery query(m_db);
    if (!query.exec(OrdersDatabaseQueries::SELECT_ALL_ORDERS))
    {
        WARNING << "Failed to load orders from database:" << query.lastError().text();
        return orders;
    }

    while (query.next())
    {
        QString orderId = query.value(0).toString();
        std::optional<qint64> latencyMs;
        if (!query.value(1).isNull())
        {
            latencyMs = query.value(1).toLongLong();
        }
        QString statusStr = query.value(2).toString();
        QString statusDescriptionStr = query.value(3).toString();
        QString orderTypeStr = query.value(4).toString();
        QString jsonDataStr = query.value(5).toString();

        // Additional columns
        double filledPrice = query.value(6).toDouble();
        QVariant limitPriceVar = query.value(7);
        QVariant stopPriceVar = query.value(8);
        QString openedDateTimeStr = query.value(9).toString();
        QString closedDateTimeStr = query.value(10).toString();
        QVariant strategyLogVar = query.value(11);

        // Reconstruct the Order object from JSON with additional fields
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonDataStr.toUtf8());
        if (jsonDoc.isObject())
        {
            QJsonObject jsonObj = jsonDoc.object();
            jsonObj["Status"] = statusStr;
            if (statusDescriptionStr.isEmpty())
            {
                statusDescriptionStr =
                    Order::getStatusDescriptionForStatus(QtEnum::fromString<Order::Status>(statusStr));
            }
            jsonObj["StatusDescription"] = statusDescriptionStr;
            jsonObj["OrderType"] = orderTypeStr;
            jsonObj["FilledPrice"] = filledPrice;

            if (!limitPriceVar.isNull())
            {
                jsonObj["LimitPrice"] = QString::number(limitPriceVar.toDouble(), 'f', 4);
            }
            if (!stopPriceVar.isNull())
            {
                jsonObj["StopPrice"] = QString::number(stopPriceVar.toDouble(), 'f', 4);
            }
            if (!openedDateTimeStr.isEmpty())
            {
                jsonObj["OpenedDateTime"] = openedDateTimeStr;
            }
            if (!closedDateTimeStr.isEmpty())
            {
                jsonObj["ClosedDateTime"] = closedDateTimeStr;
            }

            Order order(jsonObj);
            if (!strategyLogVar.isNull())
            {
                order.setStrategyLog(strategyLogVar.toString());
            }
            orders.insert(orderId, std::make_tuple(order, latencyMs));
        }
    }

    DEBUG << "Loaded" << orders.size() << "orders from database";
    return orders;
}

bool OrdersDatabase::isOpen() const
{
    return m_db.isOpen();
}

int OrdersDatabase::getOrderCount() const
{
    QSqlQuery query(m_db);
    if (!query.exec(OrdersDatabaseQueries::SELECT_ORDER_COUNT))
    {
        WARNING << "Failed to get order count:" << query.lastError().text();
        return 0;
    }

    if (query.next())
    {
        return query.value(0).toInt();
    }

    return 0;
}

bool OrdersDatabase::clearAllOrders()
{
    QSqlQuery query(m_db);
    if (!query.exec(OrdersDatabaseQueries::DELETE_ALL_ORDERS))
    {
        WARNING << "Failed to clear orders:" << query.lastError().text();
        return false;
    }

    INFO << "Cleared all orders from database";
    return true;
}

bool OrdersDatabase::updateOrderStrategyLog(const QString& p_orderID, const QString& p_log)
{
    QSqlQuery query(m_db);
    query.prepare(OrdersDatabaseQueries::UPDATE_ORDER_STRATEGY_LOG);
    query.addBindValue(p_log);
    query.addBindValue(p_orderID);

    if (!query.exec())
    {
        WARNING << "Failed to update strategy_log for order" << p_orderID << ":" << query.lastError().text();
        return false;
    }

    DEBUG << "Updated strategy_log for order" << p_orderID;
    return true;
}

bool OrdersDatabase::insertStrategyLog(const StrategyLogEntry& p_entry)
{
    QSqlQuery query(m_db);
    query.prepare(OrdersDatabaseQueries::INSERT_STRATEGY_LOG);
    query.addBindValue(p_entry.strategyID);
    query.addBindValue(p_entry.symbol);
    query.addBindValue(p_entry.timestamp.toString(Qt::ISODate));
    query.addBindValue(p_entry.message);

    if (!query.exec())
    {
        WARNING << "Failed to insert strategy log:" << query.lastError().text();
        return false;
    }

    DEBUG << "Inserted strategy log for" << p_entry.symbol << "from" << p_entry.strategyID;
    return true;
}

QVector<StrategyLogEntry> OrdersDatabase::loadStrategyLogs(const QString& p_symbol) const
{
    QVector<StrategyLogEntry> entries;

    QSqlQuery query(m_db);
    query.prepare(OrdersDatabaseQueries::SELECT_STRATEGY_LOGS_FOR_SYMBOL);
    query.addBindValue(p_symbol);

    if (!query.exec())
    {
        WARNING << "Failed to load strategy logs for" << p_symbol << ":" << query.lastError().text();
        return entries;
    }

    while (query.next())
    {
        StrategyLogEntry entry;
        entry.id = query.value(0).toInt();
        entry.strategyID = query.value(1).toString();
        entry.symbol = query.value(2).toString();
        entry.timestamp =
            QDateTime::fromString(query.value(3).toString(), Qt::ISODate).toTimeZone(TradingHours::MARKET_TIMEZONE);
        entry.message = query.value(4).toString();
        entries.append(entry);
    }

    DEBUG << "Loaded" << entries.size() << "strategy log entries for" << p_symbol;
    return entries;
}

bool OrdersDatabase::insertStrategyBracketOverlay(const StrategyBracketOverlayEntry& p_entry)
{
    QSqlQuery query(m_db);
    query.prepare(OrdersDatabaseQueries::INSERT_STRATEGY_BRACKET_OVERLAY);
    query.addBindValue(p_entry.strategyID);
    query.addBindValue(p_entry.symbol);
    query.addBindValue(p_entry.timestamp.toString(Qt::ISODate));
    query.addBindValue(bracketActionToString(p_entry.action));

    if (p_entry.action == StrategyBracketOverlayEntry::Action::Clear)
    {
        query.addBindValue(QVariant());
        query.addBindValue(QVariant());
        query.addBindValue(QVariant());
        query.addBindValue(QVariant());
        query.addBindValue(0);
        query.addBindValue(QVariant());
    }
    else
    {
        query.addBindValue(bracketSideToString(p_entry.side));
        query.addBindValue(p_entry.stopPrice);
        query.addBindValue(p_entry.takePrice);
        query.addBindValue(p_entry.referenceEntryPrice > 0.0 ? QVariant(p_entry.referenceEntryPrice) : QVariant());
        query.addBindValue(p_entry.triggered ? 1 : 0);
        query.addBindValue(p_entry.triggerReason);
    }

    if (!query.exec())
    {
        WARNING << "Failed to insert strategy bracket overlay event:" << query.lastError().text();
        return false;
    }

    DEBUG << "Inserted strategy bracket overlay event for" << p_entry.symbol << "from" << p_entry.strategyID << "action"
          << bracketActionToString(p_entry.action);
    return true;
}

QVector<StrategyBracketOverlayEntry> OrdersDatabase::loadStrategyBracketOverlays(const QString& p_symbol) const
{
    QVector<StrategyBracketOverlayEntry> entries;

    QSqlQuery query(m_db);
    query.prepare(OrdersDatabaseQueries::SELECT_STRATEGY_BRACKET_OVERLAYS_FOR_SYMBOL);
    query.addBindValue(p_symbol);

    if (!query.exec())
    {
        WARNING << "Failed to load strategy bracket overlays for" << p_symbol << ":" << query.lastError().text();
        return entries;
    }

    while (query.next())
    {
        StrategyBracketOverlayEntry entry;
        entry.id = query.value(0).toInt();
        entry.strategyID = query.value(1).toString();
        entry.symbol = query.value(2).toString();
        entry.timestamp =
            QDateTime::fromString(query.value(3).toString(), Qt::ISODate).toTimeZone(TradingHours::MARKET_TIMEZONE);

        const auto action = stringToBracketAction(query.value(4).toString());
        if (!action.has_value())
        {
            WARNING << "Skipping strategy bracket overlay row with unknown action for symbol" << p_symbol << "id"
                    << entry.id;
            continue;
        }
        entry.action = *action;

        if (entry.action == StrategyBracketOverlayEntry::Action::Clear)
        {
            entries.append(entry);
            continue;
        }

        const auto side = stringToBracketSide(query.value(5).toString());
        if (!side.has_value())
        {
            WARNING << "Skipping strategy bracket overlay row with unknown side for symbol" << p_symbol << "id"
                    << entry.id;
            continue;
        }

        entry.side = *side;
        entry.stopPrice = query.value(6).toDouble();
        entry.takePrice = query.value(7).toDouble();
        entry.referenceEntryPrice = query.value(8).toDouble();
        entry.triggered = query.value(9).toInt() != 0;
        entry.triggerReason = query.value(10).toString();
        entries.append(entry);
    }

    DEBUG << "Loaded" << entries.size() << "strategy bracket overlay entries for" << p_symbol;
    return entries;
}
