#include "RiskManager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <cmath>

#include "Assume.h"
#include "CONSTANTS.h"
#include "LedgerPaths.h"
#include "Logging.h"
#include "SQL/RiskStateStoreQueries.h"
#include "Settings.h"

#define LOGGING_CATEGORY RiskManagerLog
Q_LOGGING_CATEGORY(RiskManagerLog, "RiskManager")

namespace
{
    [[nodiscard]] QString settingsKey(const char* p_pattern, const QString& p_accountId)
    {
        return QString::fromLatin1(p_pattern).arg(p_accountId);
    }

    [[nodiscard]] QString toIsoDateTime(const QDateTime& p_dt)
    {
        return p_dt.toString(Qt::ISODate);
    }

    [[nodiscard]] QDateTime fromIsoDateTime(const QVariant& p_value)
    {
        if (!p_value.isValid() || p_value.isNull())
        {
            return {};
        }

        return QDateTime::fromString(p_value.toString(), Qt::ISODate);
    }

    [[nodiscard]] QDateTime currentMarketDateTime()
    {
        return QDateTime::currentDateTimeUtc().toTimeZone(TradingHours::MARKET_TIMEZONE);
    }

    [[nodiscard]] QString drawdownBasisLabel(const RiskDrawdownBasis p_basis)
    {
        switch (p_basis)
        {
        case RiskDrawdownBasis::EquityPeak:
            return QStringLiteral("Equity Peak");
        case RiskDrawdownBasis::TodaysProfitLoss:
            return QStringLiteral("Today's PnL");
        case RiskDrawdownBasis::RealizedProfitLoss:
            return QStringLiteral("Realized PnL");
        case RiskDrawdownBasis::TodaysProfitLossFromBaseline:
            return QStringLiteral("Today's PnL (Baseline Loss)");
        }

        return QStringLiteral("Equity Peak");
    }

    [[nodiscard]] bool usesBaselineReference(const RiskDrawdownBasis p_basis)
    {
        return p_basis == RiskDrawdownBasis::TodaysProfitLossFromBaseline;
    }

    [[nodiscard]] double drawdownReferenceValue(const RiskRuntimeState& p_runtime)
    {
        return usesBaselineReference(p_runtime.drawdownBasis) ? p_runtime.basisBaseline : p_runtime.basisPeak;
    }

    [[nodiscard]] QString drawdownReferenceLabel(const RiskRuntimeState& p_runtime)
    {
        return usesBaselineReference(p_runtime.drawdownBasis) ? QStringLiteral("baseline") : QStringLiteral("peak");
    }

    [[nodiscard]] double drawdownAmountForState(const RiskRuntimeState& p_runtime)
    {
        return std::max(0.0, drawdownReferenceValue(p_runtime) - p_runtime.currentMetric);
    }
} // namespace

class RiskManager::Store
{
  public:
    Store() : m_connectionName("RiskStateStoreDB") {}

    ~Store()
    {
        if (m_db.isOpen())
        {
            m_db.close();
        }
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    [[nodiscard]] bool ensureOpen()
    {
        const QString targetDbPath = LedgerPaths::currentLedgerDatabasePath();
        if (targetDbPath.isEmpty())
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to resolve ledger path";
            return false;
        }

        if (m_db.isOpen() && m_dbPath == targetDbPath)
        {
            return true;
        }

        if (m_db.isValid())
        {
            if (m_db.isOpen())
            {
                m_db.close();
            }
            m_db = QSqlDatabase();
            QSqlDatabase::removeDatabase(m_connectionName);
        }

        QFileInfo fileInfo(targetDbPath);
        QDir dir = fileInfo.dir();
        if (!dir.exists() && !dir.mkpath("."))
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to create ledger directory:" << dir.path();
            return false;
        }

        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        m_db.setDatabaseName(targetDbPath);
        if (!m_db.open())
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to open database:" << m_db.lastError().text();
            return false;
        }

        QSqlQuery query(m_db);
        if (!query.exec(RiskStateStoreQueries::CREATE_RISK_STATE_TABLE))
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to create risk_state:" << query.lastError().text();
            return false;
        }
        if (!query.exec(RiskStateStoreQueries::CREATE_RISK_EVENTS_TABLE))
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to create risk_events:" << query.lastError().text();
            return false;
        }
        if (!query.exec(RiskStateStoreQueries::CREATE_RISK_CONFIG_SNAPSHOT_TABLE))
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to create risk_config_snapshot:"
                                      << query.lastError().text();
            return false;
        }

        m_dbPath = targetDbPath;
        return true;
    }

    [[nodiscard]] QString currentPath() const
    {
        return m_dbPath;
    }

    [[nodiscard]] std::optional<RiskRuntimeState> loadState(const QString& p_accountId)
    {
        if (!ensureOpen())
        {
            return std::nullopt;
        }

        QSqlQuery query(m_db);
        query.prepare(RiskStateStoreQueries::SELECT_RISK_STATE_FOR_ACCOUNT);
        query.addBindValue(p_accountId);

        if (!query.exec())
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to select risk_state for" << p_accountId << ":"
                                      << query.lastError().text();
            return std::nullopt;
        }

        if (!query.next())
        {
            return std::nullopt;
        }

        RiskRuntimeState runtime;
        runtime.riskDay = QDate::fromString(query.value(0).toString(), Qt::ISODate);
        runtime.drawdownBasis = riskDrawdownBasisFromInt(query.value(1).toInt());
        runtime.basisPeak = query.value(2).toDouble();
        runtime.basisBaseline = query.value(3).toDouble();
        runtime.currentMetric = query.value(4).toDouble();
        runtime.tradingLocked = query.value(5).toInt() != 0;
        runtime.lockReason = query.value(6).toString();
        runtime.entryTradesCount = query.value(7).toInt();
        runtime.openPositionsCount = query.value(8).toInt();
        runtime.cooldownUntil = fromIsoDateTime(query.value(9));
        runtime.lastEquity = query.value(10).toDouble();
        runtime.lastTodaysPnl = query.value(11).toDouble();
        runtime.lastRealizedPnl = query.value(12).toDouble();
        return runtime;
    }

    [[nodiscard]] bool saveState(const QString& p_accountId, const RiskRuntimeState& p_runtime)
    {
        if (!ensureOpen())
        {
            return false;
        }

        QSqlQuery query(m_db);
        query.prepare(RiskStateStoreQueries::UPSERT_RISK_STATE);
        query.addBindValue(p_accountId);
        query.addBindValue(p_runtime.riskDay.toString(Qt::ISODate));
        query.addBindValue(static_cast<int>(p_runtime.drawdownBasis));
        query.addBindValue(p_runtime.basisPeak);
        query.addBindValue(p_runtime.basisBaseline);
        query.addBindValue(p_runtime.currentMetric);
        query.addBindValue(p_runtime.tradingLocked ? 1 : 0);
        query.addBindValue(p_runtime.lockReason);
        query.addBindValue(p_runtime.entryTradesCount);
        query.addBindValue(p_runtime.openPositionsCount);
        if (p_runtime.cooldownUntil.isValid())
        {
            query.addBindValue(toIsoDateTime(p_runtime.cooldownUntil));
        }
        else
        {
            query.addBindValue(QVariant());
        }
        query.addBindValue(p_runtime.lastEquity);
        query.addBindValue(p_runtime.lastTodaysPnl);
        query.addBindValue(p_runtime.lastRealizedPnl);
        query.addBindValue(toIsoDateTime(QDateTime::currentDateTimeUtc()));

        if (!query.exec())
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to upsert risk_state for" << p_accountId << ":"
                                      << query.lastError().text();
            return false;
        }

        return true;
    }

    void appendEvent(const QString& p_accountId,
                     const QString& p_eventType,
                     const QString& p_reasonCode,
                     const QString& p_message,
                     const QString& p_dataJson)
    {
        if (!ensureOpen())
        {
            return;
        }

        QSqlQuery query(m_db);
        query.prepare(RiskStateStoreQueries::INSERT_RISK_EVENT);
        query.addBindValue(toIsoDateTime(QDateTime::currentDateTimeUtc()));
        query.addBindValue(p_accountId);
        query.addBindValue(p_eventType);
        query.addBindValue(p_reasonCode);
        query.addBindValue(p_message);
        if (!p_dataJson.isEmpty())
        {
            query.addBindValue(p_dataJson);
        }
        else
        {
            query.addBindValue(QVariant());
        }

        if (!query.exec())
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to append risk_event for" << p_accountId << ":"
                                      << query.lastError().text();
        }
    }

    void saveConfigSnapshot(const QString& p_accountId, const RiskConfig& p_config)
    {
        if (!ensureOpen())
        {
            return;
        }

        QJsonObject configJson;
        configJson["enabled"] = p_config.enabled;
        configJson["dailyDrawdownLimitUsd"] = p_config.dailyDrawdownLimitUsd;
        configJson["dailyDrawdownBasis"] = static_cast<int>(p_config.dailyDrawdownBasis);
        configJson["maxPlannedLossPerTradeUsd"] = p_config.maxPlannedLossPerTradeUsd;
        configJson["maxPositionShares"] = p_config.maxPositionShares;
        configJson["maxPositionNotionalUsd"] = p_config.maxPositionNotionalUsd;
        configJson["maxDailyEntryTrades"] = p_config.maxDailyEntryTrades;
        configJson["maxOpenPositions"] = p_config.maxOpenPositions;
        configJson["cooldownEnabled"] = p_config.cooldownEnabled;
        configJson["cooldownLossTriggerUsd"] = p_config.cooldownLossTriggerUsd;
        configJson["cooldownDurationSec"] = p_config.cooldownDurationSec;
        configJson["warningAmberUsedPercent"] = p_config.warningAmberUsedPercent;
        configJson["warningRedUsedPercent"] = p_config.warningRedUsedPercent;

        QSqlQuery query(m_db);
        query.prepare(RiskStateStoreQueries::UPSERT_RISK_CONFIG_SNAPSHOT);
        query.addBindValue(p_accountId);
        query.addBindValue(toIsoDateTime(QDateTime::currentDateTimeUtc()));
        query.addBindValue(QString::fromUtf8(QJsonDocument(configJson).toJson(QJsonDocument::Compact)));

        if (!query.exec())
        {
            qCWarning(RiskManagerLog) << "RiskManager store failed to save config snapshot for" << p_accountId << ":"
                                      << query.lastError().text();
        }
    }

  private:
    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_connectionName;
};

RiskManager::RiskManager() : m_store(std::make_unique<Store>()) {}

RiskManager::~RiskManager() = default;

QString RiskManager::normalizeAccountId(const QString& p_accountId)
{
    return p_accountId.trimmed().toUpper();
}

QDate RiskManager::resolveRiskDay(const QDateTime& p_now)
{
    QDateTime marketNow = p_now;
    if (!marketNow.isValid())
    {
        marketNow = currentMarketDateTime();
    }
    else
    {
        marketNow = marketNow.toTimeZone(TradingHours::MARKET_TIMEZONE);
    }

    QDate day = marketNow.date();
    if (marketNow.time() < TradingHours::TIME_FIRST_CANDLE_EARLY_PRE_MARKET_SESSION)
    {
        day = day.addDays(-1);
    }
    return day;
}

bool RiskManager::isBuySideTradeAction(const TradeAction p_tradeAction)
{
    return p_tradeAction == TradeAction::Buy || p_tradeAction == TradeAction::BuyToCover ||
           p_tradeAction == TradeAction::BuyToOpen || p_tradeAction == TradeAction::BuyToClose;
}

int RiskManager::signedDeltaShares(const TradeAction p_tradeAction, const int p_quantity)
{
    const int quantity = std::max(0, p_quantity);
    return isBuySideTradeAction(p_tradeAction) ? quantity : -quantity;
}

int RiskManager::openingSharesForOrder(const int p_currentNetShares, const int p_signedDeltaShares)
{
    if (p_signedDeltaShares == 0)
    {
        return 0;
    }

    if (p_currentNetShares == 0)
    {
        return std::abs(p_signedDeltaShares);
    }

    const bool sameDirection =
        (p_currentNetShares > 0 && p_signedDeltaShares > 0) || (p_currentNetShares < 0 && p_signedDeltaShares < 0);
    if (sameDirection)
    {
        return std::abs(p_signedDeltaShares);
    }

    const int reduceOnlyShares = std::min(std::abs(p_currentNetShares), std::abs(p_signedDeltaShares));
    const int openingShares = std::abs(p_signedDeltaShares) - reduceOnlyShares;
    return std::max(0, openingShares);
}

double RiskManager::currentMetricForBasis(const RiskRuntimeState& p_state, const RiskDrawdownBasis p_basis)
{
    switch (p_basis)
    {
    case RiskDrawdownBasis::EquityPeak:
        return p_state.lastEquity;
    case RiskDrawdownBasis::TodaysProfitLoss:
    case RiskDrawdownBasis::TodaysProfitLossFromBaseline:
        return p_state.lastTodaysPnl;
    case RiskDrawdownBasis::RealizedProfitLoss:
        // Current balance payload does not expose a dedicated realized PnL field.
        return p_state.lastRealizedPnl;
    }

    return p_state.lastEquity;
}

void RiskManager::bindToCurrentLedgerIfNeeded()
{
    const QString currentLedgerPath = LedgerPaths::currentLedgerDatabasePath();
    if (m_boundLedgerPath != currentLedgerPath)
    {
        m_boundLedgerPath = currentLedgerPath;
        m_accounts.clear();
    }

    if (m_store != nullptr)
    {
        if (!m_store->ensureOpen())
        {
            qCWarning(RiskManagerLog) << "RiskManager failed to bind to ledger store for path:" << m_boundLedgerPath;
        }
    }
}

RiskConfig RiskManager::clampConfig(const RiskConfig& p_config) const
{
    RiskConfig config = p_config;

    config.dailyDrawdownLimitUsd = std::clamp(config.dailyDrawdownLimitUsd,
                                              RiskManagementConstants::MIN_DAILY_DRAWDOWN_LIMIT_USD,
                                              RiskManagementConstants::MAX_DAILY_DRAWDOWN_LIMIT_USD);

    config.maxPlannedLossPerTradeUsd = std::clamp(config.maxPlannedLossPerTradeUsd,
                                                  RiskManagementConstants::MIN_MAX_PLANNED_LOSS_PER_TRADE_USD,
                                                  RiskManagementConstants::MAX_MAX_PLANNED_LOSS_PER_TRADE_USD);

    config.maxPositionShares = std::clamp(config.maxPositionShares,
                                          RiskManagementConstants::MIN_MAX_POSITION_SHARES,
                                          RiskManagementConstants::MAX_MAX_POSITION_SHARES);

    config.maxPositionNotionalUsd = std::clamp(config.maxPositionNotionalUsd,
                                               RiskManagementConstants::MIN_MAX_POSITION_NOTIONAL_USD,
                                               RiskManagementConstants::MAX_MAX_POSITION_NOTIONAL_USD);

    config.maxDailyEntryTrades = std::clamp(config.maxDailyEntryTrades,
                                            RiskManagementConstants::MIN_MAX_DAILY_ENTRY_TRADES,
                                            RiskManagementConstants::MAX_MAX_DAILY_ENTRY_TRADES);

    config.maxOpenPositions = std::clamp(config.maxOpenPositions,
                                         RiskManagementConstants::MIN_MAX_OPEN_POSITIONS,
                                         RiskManagementConstants::MAX_MAX_OPEN_POSITIONS);

    config.cooldownLossTriggerUsd = std::clamp(config.cooldownLossTriggerUsd,
                                               RiskManagementConstants::MIN_COOLDOWN_LOSS_TRIGGER_USD,
                                               RiskManagementConstants::MAX_COOLDOWN_LOSS_TRIGGER_USD);

    config.cooldownDurationSec = std::clamp(config.cooldownDurationSec,
                                            RiskManagementConstants::MIN_COOLDOWN_DURATION_SEC,
                                            RiskManagementConstants::MAX_COOLDOWN_DURATION_SEC);

    config.warningAmberUsedPercent = std::clamp(config.warningAmberUsedPercent,
                                                RiskManagementConstants::MIN_WARNING_USED_PERCENT,
                                                RiskManagementConstants::MAX_WARNING_USED_PERCENT);
    config.warningRedUsedPercent = std::clamp(config.warningRedUsedPercent,
                                              RiskManagementConstants::MIN_WARNING_USED_PERCENT,
                                              RiskManagementConstants::MAX_WARNING_USED_PERCENT);
    if (config.warningRedUsedPercent < config.warningAmberUsedPercent)
    {
        config.warningRedUsedPercent = config.warningAmberUsedPercent;
    }

    return config;
}

void RiskManager::saveConfigToSettings(const QString& p_accountId, const RiskConfig& p_config)
{
    if (appStateSettings == nullptr)
    {
        return;
    }

    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_ENABLED, p_accountId),
                               p_config.enabled);
    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_DAILY_DRAWDOWN_LIMIT_USD, p_accountId),
                               p_config.dailyDrawdownLimitUsd);
    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_DAILY_DRAWDOWN_BASIS, p_accountId),
                               static_cast<int>(p_config.dailyDrawdownBasis));
    appStateSettings->setValue(
        settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_PLANNED_LOSS_PER_TRADE_USD, p_accountId),
        p_config.maxPlannedLossPerTradeUsd);
    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_POSITION_SHARES, p_accountId),
                               p_config.maxPositionShares);
    appStateSettings->setValue(
        settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_POSITION_NOTIONAL_USD, p_accountId),
        p_config.maxPositionNotionalUsd);
    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_DAILY_ENTRY_TRADES, p_accountId),
                               p_config.maxDailyEntryTrades);
    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_OPEN_POSITIONS, p_accountId),
                               p_config.maxOpenPositions);
    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_COOLDOWN_ENABLED, p_accountId),
                               p_config.cooldownEnabled);
    appStateSettings->setValue(
        settingsKey(RiskManagementConstants::SETTINGS_KEY_COOLDOWN_LOSS_TRIGGER_USD, p_accountId),
        p_config.cooldownLossTriggerUsd);
    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_COOLDOWN_DURATION_SEC, p_accountId),
                               p_config.cooldownDurationSec);
    appStateSettings->setValue(
        settingsKey(RiskManagementConstants::SETTINGS_KEY_WARNING_AMBER_USED_PERCENT, p_accountId),
        p_config.warningAmberUsedPercent);
    appStateSettings->setValue(settingsKey(RiskManagementConstants::SETTINGS_KEY_WARNING_RED_USED_PERCENT, p_accountId),
                               p_config.warningRedUsedPercent);
}

RiskConfig RiskManager::loadConfigFromSettings(const QString& p_accountId) const
{
    RiskConfig config;
    config.enabled = RiskManagementConstants::DEFAULT_ENABLED;
    config.dailyDrawdownLimitUsd = RiskManagementConstants::DEFAULT_DAILY_DRAWDOWN_LIMIT_USD;
    config.dailyDrawdownBasis = riskDrawdownBasisFromInt(RiskManagementConstants::DEFAULT_DAILY_DRAWDOWN_BASIS);
    config.maxPlannedLossPerTradeUsd = RiskManagementConstants::DEFAULT_MAX_PLANNED_LOSS_PER_TRADE_USD;
    config.maxPositionShares = RiskManagementConstants::DEFAULT_MAX_POSITION_SHARES;
    config.maxPositionNotionalUsd = RiskManagementConstants::DEFAULT_MAX_POSITION_NOTIONAL_USD;
    config.maxDailyEntryTrades = RiskManagementConstants::DEFAULT_MAX_DAILY_ENTRY_TRADES;
    config.maxOpenPositions = RiskManagementConstants::DEFAULT_MAX_OPEN_POSITIONS;
    config.cooldownEnabled = RiskManagementConstants::DEFAULT_COOLDOWN_ENABLED;
    config.cooldownLossTriggerUsd = RiskManagementConstants::DEFAULT_COOLDOWN_LOSS_TRIGGER_USD;
    config.cooldownDurationSec = RiskManagementConstants::DEFAULT_COOLDOWN_DURATION_SEC;
    config.warningAmberUsedPercent = RiskManagementConstants::DEFAULT_WARNING_AMBER_USED_PERCENT;
    config.warningRedUsedPercent = RiskManagementConstants::DEFAULT_WARNING_RED_USED_PERCENT;

    if (appStateSettings == nullptr)
    {
        return clampConfig(config);
    }

    config.enabled =
        appStateSettings->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_ENABLED, p_accountId), config.enabled)
            .toBool();
    config.dailyDrawdownLimitUsd =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_DAILY_DRAWDOWN_LIMIT_USD, p_accountId),
                    config.dailyDrawdownLimitUsd)
            .toDouble();
    config.dailyDrawdownBasis = riskDrawdownBasisFromInt(
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_DAILY_DRAWDOWN_BASIS, p_accountId),
                    static_cast<int>(config.dailyDrawdownBasis))
            .toInt());
    config.maxPlannedLossPerTradeUsd =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_PLANNED_LOSS_PER_TRADE_USD, p_accountId),
                    config.maxPlannedLossPerTradeUsd)
            .toDouble();
    config.maxPositionShares =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_POSITION_SHARES, p_accountId),
                    config.maxPositionShares)
            .toInt();
    config.maxPositionNotionalUsd =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_POSITION_NOTIONAL_USD, p_accountId),
                    config.maxPositionNotionalUsd)
            .toDouble();
    config.maxDailyEntryTrades =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_DAILY_ENTRY_TRADES, p_accountId),
                    config.maxDailyEntryTrades)
            .toInt();
    config.maxOpenPositions =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_MAX_OPEN_POSITIONS, p_accountId),
                    config.maxOpenPositions)
            .toInt();
    config.cooldownEnabled =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_COOLDOWN_ENABLED, p_accountId),
                    config.cooldownEnabled)
            .toBool();
    config.cooldownLossTriggerUsd =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_COOLDOWN_LOSS_TRIGGER_USD, p_accountId),
                    config.cooldownLossTriggerUsd)
            .toDouble();
    config.cooldownDurationSec =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_COOLDOWN_DURATION_SEC, p_accountId),
                    config.cooldownDurationSec)
            .toInt();
    config.warningAmberUsedPercent =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_WARNING_AMBER_USED_PERCENT, p_accountId),
                    config.warningAmberUsedPercent)
            .toInt();
    config.warningRedUsedPercent =
        appStateSettings
            ->value(settingsKey(RiskManagementConstants::SETTINGS_KEY_WARNING_RED_USED_PERCENT, p_accountId),
                    config.warningRedUsedPercent)
            .toInt();

    return clampConfig(config);
}

void RiskManager::appendEvent(const QString& p_accountId,
                              const QString& p_eventType,
                              const QString& p_reasonCode,
                              const QString& p_message,
                              const QString& p_dataJson)
{
    if (m_store != nullptr)
    {
        m_store->appendEvent(p_accountId, p_eventType, p_reasonCode, p_message, p_dataJson);
    }
}

void RiskManager::persistState(const QString& p_accountId, const AccountState& p_state)
{
    if (m_store != nullptr)
    {
        if (!m_store->saveState(p_accountId, p_state.runtime))
        {
            qCWarning(RiskManagerLog) << "RiskManager failed to persist state for account" << p_accountId;
        }
    }
}

void RiskManager::resetDayInternal(const QString& p_accountId,
                                   AccountState& p_state,
                                   const QDateTime& p_now,
                                   const QString& p_reason)
{
    p_state.runtime.riskDay = resolveRiskDay(p_now);
    p_state.runtime.drawdownBasis = p_state.config.dailyDrawdownBasis;
    const double metric = currentMetricForBasis(p_state.runtime, p_state.runtime.drawdownBasis);
    p_state.runtime.basisBaseline = metric;
    p_state.runtime.basisPeak = metric;
    p_state.runtime.currentMetric = metric;
    p_state.runtime.tradingLocked = false;
    p_state.runtime.lockReason.clear();
    p_state.runtime.entryTradesCount = 0;
    p_state.runtime.cooldownUntil = {};
    p_state.countedEntryOrderIds.clear();

    persistState(p_accountId, p_state);
    appendEvent(p_accountId,
                QStringLiteral("risk_day_reset"),
                QStringLiteral("day_reset"),
                p_reason.isEmpty() ? QStringLiteral("Risk day reset") : p_reason);
}

void RiskManager::rolloverIfNeeded(const QString& p_accountId, AccountState& p_state, const QDateTime& p_now)
{
    const QDate currentRiskDay = resolveRiskDay(p_now);
    if (!p_state.runtime.riskDay.isValid())
    {
        p_state.runtime.riskDay = currentRiskDay;
    }

    if (p_state.runtime.riskDay != currentRiskDay)
    {
        resetDayInternal(p_accountId, p_state, p_now, QStringLiteral("automatic daily rollover"));
    }
}

RiskManager::AccountState& RiskManager::ensureAccountState(const QString& p_accountId, const QDateTime& p_now)
{
    bindToCurrentLedgerIfNeeded();

    const QString accountId = normalizeAccountId(p_accountId);
    auto it = m_accounts.find(accountId);
    if (it == m_accounts.end())
    {
        it = m_accounts.insert(accountId, AccountState{});
    }
    AccountState& state = it.value();

    if (!state.configLoaded)
    {
        state.config = loadConfigFromSettings(accountId);
        state.configLoaded = true;
    }

    if (!state.runtimeLoaded)
    {
        std::optional<RiskRuntimeState> persisted;
        if (m_store != nullptr)
        {
            persisted = m_store->loadState(accountId);
        }

        if (persisted.has_value())
        {
            state.runtime = persisted.value();
        }
        else
        {
            state.runtime.riskDay = resolveRiskDay(p_now);
            state.runtime.drawdownBasis = state.config.dailyDrawdownBasis;
            state.runtime.basisBaseline = 0.0;
            state.runtime.basisPeak = 0.0;
            state.runtime.currentMetric = 0.0;
        }
        state.runtimeLoaded = true;
    }

    if (state.runtime.drawdownBasis != state.config.dailyDrawdownBasis)
    {
        state.runtime.drawdownBasis = state.config.dailyDrawdownBasis;
        const double metric = currentMetricForBasis(state.runtime, state.runtime.drawdownBasis);
        state.runtime.basisBaseline = metric;
        state.runtime.basisPeak = metric;
        state.runtime.currentMetric = metric;
        state.runtime.tradingLocked = false;
        state.runtime.lockReason.clear();
        persistState(accountId, state);
    }

    rolloverIfNeeded(accountId, state, p_now);
    return state;
}

RiskConfig RiskManager::getConfig(const QString& p_accountId)
{
    const QString accountId = normalizeAccountId(p_accountId);
    if (accountId.isEmpty())
    {
        return clampConfig(loadConfigFromSettings(QStringLiteral("DEFAULT")));
    }

    bindToCurrentLedgerIfNeeded();
    auto it = m_accounts.find(accountId);
    if (it == m_accounts.end())
    {
        it = m_accounts.insert(accountId, AccountState{});
    }

    AccountState& state = it.value();
    if (!state.configLoaded)
    {
        state.config = loadConfigFromSettings(accountId);
        state.configLoaded = true;
    }

    return state.config;
}

void RiskManager::setConfig(const QString& p_accountId, const RiskConfig& p_config, const QDateTime& p_now)
{
    const QString accountId = normalizeAccountId(p_accountId);
    if (accountId.isEmpty())
    {
        return;
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    const RiskConfig config = clampConfig(p_config);
    const RiskDrawdownBasis previousBasis = state.config.dailyDrawdownBasis;

    state.config = config;
    state.configLoaded = true;

    saveConfigToSettings(accountId, state.config);
    if (m_store != nullptr)
    {
        m_store->saveConfigSnapshot(accountId, state.config);
    }

    if (previousBasis != state.config.dailyDrawdownBasis)
    {
        state.runtime.drawdownBasis = state.config.dailyDrawdownBasis;
        const double metric = currentMetricForBasis(state.runtime, state.runtime.drawdownBasis);
        state.runtime.basisBaseline = metric;
        state.runtime.basisPeak = metric;
        state.runtime.currentMetric = metric;
        state.runtime.tradingLocked = false;
        state.runtime.lockReason.clear();
        appendEvent(accountId,
                    QStringLiteral("drawdown_basis_changed"),
                    QStringLiteral("basis_changed"),
                    QStringLiteral("Drawdown basis changed and state rebaselined"));
    }

    persistState(accountId, state);
}

RiskRuntimeState RiskManager::getRuntimeState(const QString& p_accountId, const QDateTime& p_now)
{
    const QString accountId = normalizeAccountId(p_accountId);
    if (accountId.isEmpty())
    {
        return {};
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    return state.runtime;
}

RiskStatusSnapshot RiskManager::getStatusSnapshot(const QString& p_accountId, const QDateTime& p_now)
{
    RiskStatusSnapshot snapshot;
    snapshot.accountId = normalizeAccountId(p_accountId);
    if (snapshot.accountId.isEmpty())
    {
        snapshot.summaryText = QStringLiteral("No account selected");
        snapshot.detailsText = QStringLiteral("Select an account to view risk status.");
        return snapshot;
    }

    AccountState& state = ensureAccountState(snapshot.accountId, p_now);
    snapshot.config = state.config;
    snapshot.runtime = state.runtime;

    const double drawdownAmount = drawdownAmountForState(state.runtime);
    snapshot.drawdownAmountUsd = drawdownAmount;
    snapshot.drawdownRemainingUsd = std::max(0.0, state.config.dailyDrawdownLimitUsd - drawdownAmount);
    if (state.config.dailyDrawdownLimitUsd > 0.0)
    {
        snapshot.drawdownRemainingPercent =
            std::clamp((snapshot.drawdownRemainingUsd / state.config.dailyDrawdownLimitUsd) * 100.0, 0.0, 100.0);
    }
    else
    {
        snapshot.drawdownRemainingPercent = 100.0;
    }
    const double usedPercent = std::clamp(100.0 - snapshot.drawdownRemainingPercent, 0.0, 100.0);

    const QDateTime marketNow =
        p_now.isValid() ? p_now.toTimeZone(TradingHours::MARKET_TIMEZONE) : currentMarketDateTime();
    snapshot.cooldownActive = state.runtime.cooldownUntil.isValid() && state.runtime.cooldownUntil > marketNow;

    snapshot.summaryText = QString("Drawdown headroom: %1% (used %2%) | $%3 / $%4 remaining")
                               .arg(QString::number(snapshot.drawdownRemainingPercent, 'f', 1),
                                    QString::number(usedPercent, 'f', 1),
                                    QString::number(snapshot.drawdownRemainingUsd, 'f', 2),
                                    QString::number(state.config.dailyDrawdownLimitUsd, 'f', 2));

    const QString basisDetails = QString("Basis %1 | %2 $%3 -> now $%4 | drawdown used $%5")
                                     .arg(drawdownBasisLabel(state.runtime.drawdownBasis),
                                          drawdownReferenceLabel(state.runtime),
                                          QString::number(drawdownReferenceValue(state.runtime), 'f', 2),
                                          QString::number(state.runtime.currentMetric, 'f', 2),
                                          QString::number(snapshot.drawdownAmountUsd, 'f', 2));

    if (state.runtime.tradingLocked)
    {
        snapshot.detailsText = QString("LOCKED: %1 | %2").arg(state.runtime.lockReason, basisDetails);
    }
    else if (snapshot.cooldownActive)
    {
        snapshot.detailsText =
            QString("Cooldown until %1 | %2")
                .arg(snapshot.runtime.cooldownUntil.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")), basisDetails);
    }
    else
    {
        snapshot.detailsText = QString("Entries %1/%2 | Open Positions %3/%4 | %5")
                                   .arg(state.runtime.entryTradesCount)
                                   .arg(state.config.maxDailyEntryTrades)
                                   .arg(state.runtime.openPositionsCount)
                                   .arg(state.config.maxOpenPositions)
                                   .arg(basisDetails);
    }

    return snapshot;
}

void RiskManager::onBalanceUpdate(const Balance& p_balance, const QDateTime& p_now)
{
    const QString accountId = normalizeAccountId(p_balance.getAccountID());
    if (accountId.isEmpty())
    {
        return;
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    state.runtime.lastEquity = p_balance.getEquity();
    state.runtime.lastTodaysPnl = p_balance.getTodaysProfitLoss();
    state.runtime.lastRealizedPnl = p_balance.getTodaysProfitLoss();
    state.runtime.drawdownBasis = state.config.dailyDrawdownBasis;

    const double metric = currentMetricForBasis(state.runtime, state.runtime.drawdownBasis);
    if (state.runtime.basisBaseline == 0.0 && state.runtime.basisPeak == 0.0)
    {
        state.runtime.basisBaseline = metric;
        state.runtime.basisPeak = metric;
    }
    state.runtime.currentMetric = metric;
    if (usesBaselineReference(state.runtime.drawdownBasis))
    {
        state.runtime.basisPeak = state.runtime.basisBaseline;
    }
    else
    {
        state.runtime.basisPeak = std::max(state.runtime.basisPeak, metric);
    }

    const double drawdown = drawdownAmountForState(state.runtime);
    if (state.config.enabled && drawdown >= state.config.dailyDrawdownLimitUsd && !state.runtime.tradingLocked)
    {
        state.runtime.tradingLocked = true;
        state.runtime.lockReason =
            QString("Daily drawdown limit breached (%1 >= %2)")
                .arg(QString::number(drawdown, 'f', 2), QString::number(state.config.dailyDrawdownLimitUsd, 'f', 2));
        appendEvent(accountId,
                    QStringLiteral("lock"),
                    QStringLiteral("daily_drawdown_breach"),
                    state.runtime.lockReason);
    }

    persistState(accountId, state);
}

void RiskManager::onEntryOrderFirstFill(const QString& p_accountId, const QString& p_orderId, const QDateTime& p_now)
{
    const QString accountId = normalizeAccountId(p_accountId);
    const QString orderId = p_orderId.trimmed();
    if (accountId.isEmpty() || orderId.isEmpty())
    {
        return;
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    if (state.countedEntryOrderIds.contains(orderId))
    {
        return;
    }

    state.countedEntryOrderIds.insert(orderId);
    state.runtime.entryTradesCount += 1;
    persistState(accountId, state);
}

void RiskManager::onPositionClosed(const Position& p_position, const QDateTime& p_now)
{
    const QString accountId = normalizeAccountId(p_position.getAccountID());
    if (accountId.isEmpty())
    {
        return;
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    if (!state.config.cooldownEnabled || state.config.cooldownDurationSec <= 0 ||
        state.config.cooldownLossTriggerUsd <= 0.0)
    {
        return;
    }

    bool ok = false;
    const double positionPnl = p_position.getTodaysProfitLoss().trimmed().toDouble(&ok);
    if (!ok)
    {
        return;
    }

    if (positionPnl <= -state.config.cooldownLossTriggerUsd)
    {
        const QDateTime cooldownUntil =
            p_now.toTimeZone(TradingHours::MARKET_TIMEZONE).addSecs(state.config.cooldownDurationSec);
        if (!state.runtime.cooldownUntil.isValid() || cooldownUntil > state.runtime.cooldownUntil)
        {
            state.runtime.cooldownUntil = cooldownUntil;
            appendEvent(accountId,
                        QStringLiteral("cooldown_started"),
                        QStringLiteral("position_loss_trigger"),
                        QString("Cooldown started after losing closed trade (pnl=%1)")
                            .arg(QString::number(positionPnl, 'f', 2)));
            persistState(accountId, state);
        }
    }
}

void RiskManager::updateOpenPositionsCount(const QString& p_accountId, int p_openPositionsCount, const QDateTime& p_now)
{
    const QString accountId = normalizeAccountId(p_accountId);
    if (accountId.isEmpty())
    {
        return;
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    state.runtime.openPositionsCount = std::max(0, p_openPositionsCount);
    persistState(accountId, state);
}

RiskDecision RiskManager::evaluateOrder(const PlaceOrderRequest& p_orderRequest,
                                        const OrderEvalContext& p_context,
                                        const QDateTime& p_now)
{
    const QString accountId = normalizeAccountId(p_orderRequest.getAccountID());
    RiskDecision decision;

    if (accountId.isEmpty())
    {
        decision.allow = false;
        decision.reasonCode = QStringLiteral("invalid_account");
        decision.message = QStringLiteral("Order account is empty");
        return decision;
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    const int signedDelta = signedDeltaShares(p_orderRequest.getTradeAction(), p_orderRequest.getQuantity());
    const int openingShares = openingSharesForOrder(p_context.currentNetShares, signedDelta);
    decision.isEntry = openingShares > 0;
    decision.openingShares = openingShares;

    state.runtime.openPositionsCount = p_context.openPositionsCount;

    if (!state.config.enabled || !decision.isEntry)
    {
        return decision;
    }

    auto reject = [&](const QString& p_code, const QString& p_message) -> RiskDecision
    {
        RiskDecision rejected = decision;
        rejected.allow = false;
        rejected.reasonCode = p_code;
        rejected.message = p_message;
        appendEvent(accountId, QStringLiteral("order_rejected"), p_code, p_message);
        return rejected;
    };

    const QDateTime marketNow =
        p_now.isValid() ? p_now.toTimeZone(TradingHours::MARKET_TIMEZONE) : currentMarketDateTime();

    if (state.runtime.tradingLocked)
    {
        const QString message = state.runtime.lockReason.isEmpty() ? QStringLiteral("Trading is locked by risk manager")
                                                                   : state.runtime.lockReason;
        return reject(QStringLiteral("risk_locked"), message);
    }

    if (state.runtime.cooldownUntil.isValid() && state.runtime.cooldownUntil > marketNow)
    {
        return reject(QStringLiteral("cooldown_active"),
                      QString("Cooldown is active until %1")
                          .arg(state.runtime.cooldownUntil.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t"))));
    }

    if (state.runtime.entryTradesCount >= state.config.maxDailyEntryTrades)
    {
        return reject(QStringLiteral("max_daily_entries_reached"),
                      QString("Daily entry trade limit reached (%1/%2)")
                          .arg(state.runtime.entryTradesCount)
                          .arg(state.config.maxDailyEntryTrades));
    }

    if (p_context.currentNetShares == 0 && p_context.openPositionsCount >= state.config.maxOpenPositions)
    {
        return reject(QStringLiteral("max_open_positions_reached"),
                      QString("Max open positions reached (%1/%2)")
                          .arg(p_context.openPositionsCount)
                          .arg(state.config.maxOpenPositions));
    }

    const int predictedNetShares = p_context.currentNetShares + signedDelta;
    if (std::abs(predictedNetShares) > state.config.maxPositionShares)
    {
        return reject(QStringLiteral("max_position_shares_exceeded"),
                      QString("Position-size limit exceeded (%1 > %2)")
                          .arg(std::abs(predictedNetShares))
                          .arg(state.config.maxPositionShares));
    }

    std::optional<double> referencePrice = p_context.referencePrice;
    if (referencePrice.has_value() && referencePrice.value() <= 0.0)
    {
        referencePrice.reset();
    }

    if (!referencePrice.has_value())
    {
        return reject(QStringLiteral("missing_reference_price"),
                      QStringLiteral("Unable to determine reference price for risk checks"));
    }

    const double projectedNotional = std::abs(static_cast<double>(predictedNetShares) * referencePrice.value());
    if (projectedNotional > state.config.maxPositionNotionalUsd)
    {
        return reject(QStringLiteral("max_position_notional_exceeded"),
                      QString("Position-notional limit exceeded ($%1 > $%2)")
                          .arg(QString::number(projectedNotional, 'f', 2))
                          .arg(QString::number(state.config.maxPositionNotionalUsd, 'f', 2)));
    }

    std::optional<double> stopPrice = p_context.stopPrice;
    if (stopPrice.has_value() && stopPrice.value() <= 0.0)
    {
        stopPrice.reset();
    }
    if (!stopPrice.has_value())
    {
        return reject(QStringLiteral("missing_stop_price"),
                      QStringLiteral("Entry orders require a stop source for planned-risk checks"));
    }

    const double plannedLoss =
        std::abs(referencePrice.value() - stopPrice.value()) * static_cast<double>(openingShares);
    decision.plannedLossUsd = plannedLoss;
    if (plannedLoss > state.config.maxPlannedLossPerTradeUsd)
    {
        return reject(QStringLiteral("max_per_trade_loss_exceeded"),
                      QString("Planned per-trade loss exceeded ($%1 > $%2)")
                          .arg(QString::number(plannedLoss, 'f', 2))
                          .arg(QString::number(state.config.maxPlannedLossPerTradeUsd, 'f', 2)));
    }

    const double drawdown = drawdownAmountForState(state.runtime);
    if (drawdown >= state.config.dailyDrawdownLimitUsd)
    {
        state.runtime.tradingLocked = true;
        state.runtime.lockReason =
            QString("Daily drawdown limit breached (%1 >= %2)")
                .arg(QString::number(drawdown, 'f', 2), QString::number(state.config.dailyDrawdownLimitUsd, 'f', 2));
        persistState(accountId, state);
        return reject(QStringLiteral("daily_drawdown_breached"), state.runtime.lockReason);
    }

    return decision;
}

void RiskManager::resetRiskDay(const QString& p_accountId, const QDateTime& p_now, const QString& p_reason)
{
    const QString accountId = normalizeAccountId(p_accountId);
    if (accountId.isEmpty())
    {
        return;
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    resetDayInternal(accountId, state, p_now, p_reason.isEmpty() ? QStringLiteral("manual reset") : p_reason);
}

void RiskManager::unlockTrading(const QString& p_accountId, const QDateTime& p_now, const QString& p_reason)
{
    const QString accountId = normalizeAccountId(p_accountId);
    if (accountId.isEmpty())
    {
        return;
    }

    AccountState& state = ensureAccountState(accountId, p_now);
    state.runtime.tradingLocked = false;
    state.runtime.lockReason.clear();
    persistState(accountId, state);
    appendEvent(accountId,
                QStringLiteral("unlock"),
                QStringLiteral("manual_unlock"),
                p_reason.isEmpty() ? QStringLiteral("Manual trading unlock") : p_reason);
}
