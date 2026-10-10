#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "Balance.h"
#include "MainApp.h"
#include "PlaceOrder.h"
#include "Position.h"
#include "RiskManager.h"
#include "Settings.h"

namespace
{
    const QTimeZone kMarketTz("America/New_York");

    QDateTime marketNow(int p_hour = 10, int p_minute = 0, const QDate& p_date = QDate(2026, 10, 7))
    {
        return QDateTime(p_date, QTime(p_hour, p_minute), kMarketTz);
    }

    Balance balance(const QString& p_accountId, double p_equity, double p_todaysPnl)
    {
        return Balance(QJsonObject{{"AccountID", p_accountId},
                                   {"AccountType", "Margin"},
                                   {"BuyingPower", "0"},
                                   {"CashBalance", "0"},
                                   {"Comission", "0"},
                                   {"Equity", QString::number(p_equity)},
                                   {"MarketValue", "0"},
                                   {"TodaysProfitLoss", QString::number(p_todaysPnl)},
                                   {"UnclearedDeposit", "0"}});
    }

    Position closedPosition(const QString& p_accountId,
                            const QString& p_positionId,
                            double p_realizedPnl,
                            const QDateTime& p_closedDateTime)
    {
        return Position(QJsonObject{{"AccountID", p_accountId},
                                    {"PositionID", p_positionId},
                                    {"RealizedProfitLoss", p_realizedPnl},
                                    {"ClosedDateTime", p_closedDateTime.toString(Qt::ISODate)}});
    }

    PlaceOrderRequest entryOrder(const QString& p_accountId, TradeAction p_action, int p_quantity)
    {
        PlaceOrderRequest request;
        request.setAccountID(p_accountId);
        request.setSymbol("TEST");
        request.setTradeAction(p_action);
        request.setQuantity(p_quantity);
        return request;
    }
} // namespace

class RiskManagerTests : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        QCoreApplication::setOrganizationName("OpenTraderPlatform");
        QCoreApplication::setApplicationName("RiskManagerTests");
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_directory.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_directory.path());
        m_settings = std::make_unique<QSettings>(m_directory.path() + "/state.ini", QSettings::IniFormat);
        appStateSettings = m_settings.get();
        m_oldCacheRoot = cacheRootDir;
        cacheRootDir = m_directory.path();
        // RiskManager's ledger database path comes from getDataLocation(), which resolves
        // QStandardPaths::AppDataLocation (XDG_DATA_HOME on Linux), not cacheRootDir. Redirect
        // it into the temp dir too so the SQLite ledger used by these tests is fully isolated
        // and never leaks state across test runs or into the real user data directory.
        m_hadOldXdgDataHome = qEnvironmentVariableIsSet("XDG_DATA_HOME");
        m_oldXdgDataHome = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", QFile::encodeName(m_directory.path() + "/xdg-data"));
        // Sim mode keeps the ledger under an isolated "Simulation" subdirectory of cacheRootDir.
        MainApp::setTradingMode(TradingMode::Sim);
    }

    void cleanupTestCase()
    {
        appStateSettings = nullptr;
        cacheRootDir = m_oldCacheRoot;
        if (m_hadOldXdgDataHome)
        {
            qputenv("XDG_DATA_HOME", m_oldXdgDataHome);
        }
        else
        {
            qunsetenv("XDG_DATA_HOME");
        }
    }

    void clampConfigEnforcesBounds()
    {
        RiskManager manager;
        const QString accountId = "RM-CLAMP";
        RiskConfig config;
        config.dailyDrawdownLimitUsd = -5.0;
        config.maxPlannedLossPerTradeUsd = 1e12;
        config.maxPositionShares = 0;
        config.maxPositionNotionalUsd = -1.0;
        config.maxDailyEntryTrades = 0;
        config.maxOpenPositions = 0;
        config.cooldownLossTriggerUsd = -10.0;
        config.cooldownDurationSec = -1;
        config.warningAmberUsedPercent = 90;
        config.warningRedUsedPercent = 10; // below amber, must be raised to match amber
        manager.setConfig(accountId, config, marketNow());

        const RiskConfig clamped = manager.getConfig(accountId);
        QCOMPARE(clamped.dailyDrawdownLimitUsd, RiskManagementConstants::MIN_DAILY_DRAWDOWN_LIMIT_USD);
        QCOMPARE(clamped.maxPlannedLossPerTradeUsd, RiskManagementConstants::MAX_MAX_PLANNED_LOSS_PER_TRADE_USD);
        QCOMPARE(clamped.maxPositionShares, RiskManagementConstants::MIN_MAX_POSITION_SHARES);
        QCOMPARE(clamped.maxPositionNotionalUsd, RiskManagementConstants::MIN_MAX_POSITION_NOTIONAL_USD);
        QCOMPARE(clamped.maxDailyEntryTrades, RiskManagementConstants::MIN_MAX_DAILY_ENTRY_TRADES);
        QCOMPARE(clamped.maxOpenPositions, RiskManagementConstants::MIN_MAX_OPEN_POSITIONS);
        QCOMPARE(clamped.cooldownLossTriggerUsd, RiskManagementConstants::MIN_COOLDOWN_LOSS_TRIGGER_USD);
        QCOMPARE(clamped.cooldownDurationSec, RiskManagementConstants::MIN_COOLDOWN_DURATION_SEC);
        QCOMPARE(clamped.warningRedUsedPercent, clamped.warningAmberUsedPercent);
    }

    void evaluateOrderAllowsEntryWithinLimits()
    {
        RiskManager manager;
        const QString accountId = "RM-ENTRY-OK";
        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.referencePrice = 50.0;
        context.stopPrice = 49.0;

        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 100), context, marketNow());
        QVERIFY(decision.allow);
        QVERIFY(decision.isEntry);
        QCOMPARE(decision.openingShares, 100);
        QCOMPARE(decision.plannedLossUsd, 100.0); // |50 - 49| * 100 shares
    }

    void evaluateOrderRejectsMissingReferencePrice()
    {
        RiskManager manager;
        const QString accountId = "RM-MISSING-REF";
        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.stopPrice = 49.0;

        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 100), context, marketNow());
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("missing_reference_price"));
    }

    void evaluateOrderRejectsMissingStopPrice()
    {
        RiskManager manager;
        const QString accountId = "RM-MISSING-STOP";
        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.referencePrice = 50.0;

        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 100), context, marketNow());
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("missing_stop_price"));
    }

    void evaluateOrderRejectsMaxPositionShares()
    {
        RiskManager manager;
        const QString accountId = "RM-MAX-SHARES";
        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.referencePrice = 10.0;
        context.stopPrice = 9.0;

        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 6000), context, marketNow());
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("max_position_shares_exceeded"));
    }

    void evaluateOrderRejectsMaxPositionNotional()
    {
        RiskManager manager;
        const QString accountId = "RM-MAX-NOTIONAL";
        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.referencePrice = 30.0;
        context.stopPrice = 29.0;

        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 4000), context, marketNow());
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("max_position_notional_exceeded"));
    }

    void evaluateOrderRejectsMaxPlannedLoss()
    {
        RiskManager manager;
        const QString accountId = "RM-MAX-LOSS";
        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.referencePrice = 50.0;
        context.stopPrice = 40.0; // |50-40| * 100 = 1000 > default 200 limit

        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 100), context, marketNow());
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("max_per_trade_loss_exceeded"));
    }

    void evaluateOrderRejectsMaxDailyEntries()
    {
        RiskManager manager;
        const QString accountId = "RM-MAX-DAILY";
        const QDateTime now = marketNow();
        for (int i = 0; i < RiskManagementConstants::DEFAULT_MAX_DAILY_ENTRY_TRADES; ++i)
        {
            manager.onEntryOrderFirstFill(accountId, QString("order-%1").arg(i), now);
        }

        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.referencePrice = 50.0;
        context.stopPrice = 49.0;

        const RiskDecision decision = manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 10), context, now);
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("max_daily_entries_reached"));
    }

    void evaluateOrderRejectsMaxOpenPositions()
    {
        RiskManager manager;
        const QString accountId = "RM-MAX-OPEN";
        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = RiskManagementConstants::DEFAULT_MAX_OPEN_POSITIONS;
        context.referencePrice = 50.0;
        context.stopPrice = 49.0;

        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 10), context, marketNow());
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("max_open_positions_reached"));
    }

    void evaluateOrderBypassesChecksForReduceOnlyOrders()
    {
        RiskManager manager;
        const QString accountId = "RM-REDUCE-BYPASS";
        const QDateTime now = marketNow();
        // Breach the daily drawdown limit so trading is locked.
        manager.onBalanceUpdate(balance(accountId, 10000.0, 0.0), now);
        manager.onBalanceUpdate(
            balance(accountId, 10000.0 - RiskManagementConstants::DEFAULT_DAILY_DRAWDOWN_LIMIT_USD, 0.0),
            now);
        QVERIFY(manager.getRuntimeState(accountId, now).tradingLocked);

        // A fully reduce-only order (closing an existing position) is not an entry and must bypass the lock.
        RiskManager::OrderEvalContext context;
        context.currentNetShares = 100;
        context.openPositionsCount = 1;

        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Sell, 100), context, now);
        QVERIFY(decision.allow);
        QVERIFY(!decision.isEntry);
        QCOMPARE(decision.openingShares, 0);
    }

    void evaluateOrderRejectsWhenLocked()
    {
        RiskManager manager;
        const QString accountId = "RM-LOCK";
        const QDateTime now = marketNow();
        manager.onBalanceUpdate(balance(accountId, 10000.0, 0.0), now);
        manager.onBalanceUpdate(
            balance(accountId, 10000.0 - RiskManagementConstants::DEFAULT_DAILY_DRAWDOWN_LIMIT_USD, 0.0),
            now);
        QVERIFY(manager.getRuntimeState(accountId, now).tradingLocked);

        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.referencePrice = 50.0;
        context.stopPrice = 49.0;

        const RiskDecision decision = manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 10), context, now);
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("risk_locked"));
    }

    void onBalanceUpdateLocksOnEquityPeakDrawdownBreach()
    {
        RiskManager manager;
        const QString accountId = "RM-BALANCE-LOCK";
        const QDateTime now = marketNow();

        manager.onBalanceUpdate(balance(accountId, 50000.0, 0.0), now);
        RiskRuntimeState afterFirst = manager.getRuntimeState(accountId, now);
        QVERIFY(!afterFirst.tradingLocked);
        QCOMPARE(afterFirst.basisPeak, 50000.0);

        // Equity rises; peak should ratchet upward.
        manager.onBalanceUpdate(balance(accountId, 52000.0, 0.0), now);
        RiskRuntimeState afterRise = manager.getRuntimeState(accountId, now);
        QCOMPARE(afterRise.basisPeak, 52000.0);
        QVERIFY(!afterRise.tradingLocked);

        // Equity falls below peak by exactly the configured daily drawdown limit (1000 USD default).
        manager.onBalanceUpdate(
            balance(accountId, 52000.0 - RiskManagementConstants::DEFAULT_DAILY_DRAWDOWN_LIMIT_USD, 0.0),
            now);
        RiskRuntimeState afterBreach = manager.getRuntimeState(accountId, now);
        QVERIFY(afterBreach.tradingLocked);
        QVERIFY(!afterBreach.lockReason.isEmpty());
    }

    void onEntryOrderFirstFillDeduplicatesSameOrderId()
    {
        RiskManager manager;
        const QString accountId = "RM-DEDUP";
        const QDateTime now = marketNow();
        manager.onEntryOrderFirstFill(accountId, "order-1", now);
        manager.onEntryOrderFirstFill(accountId, "order-1", now);
        manager.onEntryOrderFirstFill(accountId, "order-1", now);
        QCOMPARE(manager.getRuntimeState(accountId, now).entryTradesCount, 1);

        manager.onEntryOrderFirstFill(accountId, "order-2", now);
        QCOMPARE(manager.getRuntimeState(accountId, now).entryTradesCount, 2);
    }

    void onPositionClosedStartsCooldownOnLossAndBlocksEntries()
    {
        RiskManager manager;
        const QString accountId = "RM-COOLDOWN";
        const QDateTime closedAt = marketNow();
        const double lossPnl = -(RiskManagementConstants::DEFAULT_COOLDOWN_LOSS_TRIGGER_USD + 50.0);
        manager.onPositionClosed(closedPosition(accountId, "pos-1", lossPnl, closedAt), closedAt);

        RiskRuntimeState state = manager.getRuntimeState(accountId, closedAt);
        QVERIFY(state.cooldownUntil.isValid());
        QCOMPARE(state.cooldownUntil,
                 closedAt.toTimeZone(kMarketTz).addSecs(RiskManagementConstants::DEFAULT_COOLDOWN_DURATION_SEC));

        RiskManager::OrderEvalContext context;
        context.currentNetShares = 0;
        context.openPositionsCount = 0;
        context.referencePrice = 50.0;
        context.stopPrice = 49.0;
        const RiskDecision decision =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 10), context, closedAt.addSecs(1));
        QVERIFY(!decision.allow);
        QCOMPARE(decision.reasonCode, QStringLiteral("cooldown_active"));

        // Past the cooldown window, entries are allowed again.
        const RiskDecision decisionAfterCooldown =
            manager.evaluateOrder(entryOrder(accountId, TradeAction::Buy, 10),
                                  context,
                                  closedAt.addSecs(RiskManagementConstants::DEFAULT_COOLDOWN_DURATION_SEC + 1));
        QVERIFY(decisionAfterCooldown.allow);
    }

    void onPositionClosedDefersCooldownWhenRealizedPnlMissing()
    {
        RiskManager manager;
        const QString accountId = "RM-COOLDOWN-DEFERRED";
        const QDateTime closedAt = marketNow();
        Position withoutRealizedPnl(QJsonObject{{"AccountID", accountId},
                                                {"PositionID", "pos-2"},
                                                {"ClosedDateTime", closedAt.toString(Qt::ISODate)}});
        QVERIFY(!withoutRealizedPnl.getRealizedProfitLoss().has_value());

        manager.onPositionClosed(withoutRealizedPnl, closedAt);
        QVERIFY(!manager.getRuntimeState(accountId, closedAt).cooldownUntil.isValid());
    }

    void resetRiskDayClearsLockAndCounters()
    {
        RiskManager manager;
        const QString accountId = "RM-RESET-DAY";
        const QDateTime now = marketNow();
        manager.onEntryOrderFirstFill(accountId, "order-1", now);
        manager.onBalanceUpdate(balance(accountId, 10000.0, 0.0), now);
        manager.onBalanceUpdate(
            balance(accountId, 10000.0 - RiskManagementConstants::DEFAULT_DAILY_DRAWDOWN_LIMIT_USD, 0.0),
            now);
        QVERIFY(manager.getRuntimeState(accountId, now).tradingLocked);
        QCOMPARE(manager.getRuntimeState(accountId, now).entryTradesCount, 1);

        manager.resetRiskDay(accountId, now, "test reset");
        const RiskRuntimeState state = manager.getRuntimeState(accountId, now);
        QVERIFY(!state.tradingLocked);
        QVERIFY(state.lockReason.isEmpty());
        QCOMPARE(state.entryTradesCount, 0);
        QCOMPARE(state.basisPeak, state.basisBaseline);
        QCOMPARE(state.basisPeak, state.currentMetric);
    }

    void unlockTradingClearsLockButPreservesCounters()
    {
        RiskManager manager;
        const QString accountId = "RM-UNLOCK";
        const QDateTime now = marketNow();
        manager.onEntryOrderFirstFill(accountId, "order-1", now);
        manager.onBalanceUpdate(balance(accountId, 10000.0, 0.0), now);
        manager.onBalanceUpdate(
            balance(accountId, 10000.0 - RiskManagementConstants::DEFAULT_DAILY_DRAWDOWN_LIMIT_USD, 0.0),
            now);
        QVERIFY(manager.getRuntimeState(accountId, now).tradingLocked);

        manager.unlockTrading(accountId, now, "test unlock");
        const RiskRuntimeState state = manager.getRuntimeState(accountId, now);
        QVERIFY(!state.tradingLocked);
        QVERIFY(state.lockReason.isEmpty());
        QCOMPARE(state.entryTradesCount, 1); // unlike resetRiskDay, counters are preserved
    }

    void getStatusSnapshotReportsDrawdownHeadroomAndState()
    {
        RiskManager manager;
        const QString accountId = "RM-SNAPSHOT";
        const QDateTime now = marketNow();
        RiskConfig config;
        config.dailyDrawdownLimitUsd = 1000.0;
        manager.setConfig(accountId, config, now);
        manager.onBalanceUpdate(balance(accountId, 10000.0, 0.0), now);
        manager.onBalanceUpdate(balance(accountId, 9700.0, 0.0), now); // used 300 of 1000 => 30% used

        const RiskStatusSnapshot snapshot = manager.getStatusSnapshot(accountId, now);
        QCOMPARE(snapshot.drawdownAmountUsd, 300.0);
        QCOMPARE(snapshot.drawdownRemainingUsd, 700.0);
        QCOMPARE(snapshot.drawdownRemainingPercent, 70.0);
        QVERIFY(!snapshot.cooldownActive);
        QVERIFY(snapshot.detailsText.contains("Entries"));

        manager.onBalanceUpdate(balance(accountId, 9000.0, 0.0), now); // breaches the 1000 limit
        const RiskStatusSnapshot locked = manager.getStatusSnapshot(accountId, now);
        QVERIFY(locked.runtime.tradingLocked);
        QVERIFY(locked.detailsText.contains("LOCKED"));
    }

    void runtimeStatePersistsAcrossInstances()
    {
        const QString accountId = "RM-PERSIST";
        const QDateTime now = marketNow();
        {
            RiskManager first;
            first.onBalanceUpdate(balance(accountId, 10000.0, 0.0), now);
            first.onBalanceUpdate(
                balance(accountId, 10000.0 - RiskManagementConstants::DEFAULT_DAILY_DRAWDOWN_LIMIT_USD, 0.0),
                now);
            QVERIFY(first.getRuntimeState(accountId, now).tradingLocked);
        } // first's Store closes its SQLite connection before the next instance opens it.

        RiskManager second;
        const RiskRuntimeState reloaded = second.getRuntimeState(accountId, now);
        QVERIFY(reloaded.tradingLocked);
        QVERIFY(!reloaded.lockReason.isEmpty());
    }

  private:
    QTemporaryDir m_directory;
    std::unique_ptr<QSettings> m_settings;
    QString m_oldCacheRoot;
    QByteArray m_oldXdgDataHome;
    bool m_hadOldXdgDataHome = false;
};

QTEST_GUILESS_MAIN(RiskManagerTests)
#include "RiskManagerTests.moc"
