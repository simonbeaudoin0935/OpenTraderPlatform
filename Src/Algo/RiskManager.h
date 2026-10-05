#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QDateTime>
#include <optional>
#include <memory>

#include "Balance.h"
#include "PlaceOrder.h"
#include "Position.h"
#include "RiskTypes.h"

class RiskManager
{
  public:
    struct OrderEvalContext
    {
        int currentNetShares = 0;
        int openPositionsCount = 0;
        std::optional<double> referencePrice;
        std::optional<double> stopPrice;
    };

    RiskManager();
    ~RiskManager();

    [[nodiscard]] RiskConfig getConfig(const QString& p_accountId);
    void setConfig(const QString& p_accountId, const RiskConfig& p_config, const QDateTime& p_now);

    [[nodiscard]] RiskRuntimeState getRuntimeState(const QString& p_accountId, const QDateTime& p_now);
    [[nodiscard]] RiskStatusSnapshot getStatusSnapshot(const QString& p_accountId, const QDateTime& p_now);

    [[nodiscard]] RiskDecision
    evaluateOrder(const PlaceOrderRequest& p_orderRequest, const OrderEvalContext& p_context, const QDateTime& p_now);

    void onBalanceUpdate(const Balance& p_balance, const QDateTime& p_now);
    void onEntryOrderFirstFill(const QString& p_accountId, const QString& p_orderId, const QDateTime& p_now);
    void onPositionClosed(const Position& p_position, const QDateTime& p_now);
    void updateOpenPositionsCount(const QString& p_accountId, int p_openPositionsCount, const QDateTime& p_now);

    void resetRiskDay(const QString& p_accountId, const QDateTime& p_now, const QString& p_reason = QString());
    void unlockTrading(const QString& p_accountId, const QDateTime& p_now, const QString& p_reason = QString());

  private:
    class Store;

    struct AccountState
    {
        RiskConfig config;
        RiskRuntimeState runtime;
        bool configLoaded = false;
        bool runtimeLoaded = false;
        QSet<QString> countedEntryOrderIds;
    };

    [[nodiscard]] static QString normalizeAccountId(const QString& p_accountId);
    [[nodiscard]] static QDate resolveRiskDay(const QDateTime& p_now);
    [[nodiscard]] static bool isBuySideTradeAction(TradeAction p_tradeAction);
    [[nodiscard]] static int signedDeltaShares(TradeAction p_tradeAction, int p_quantity);
    [[nodiscard]] static int openingSharesForOrder(int p_currentNetShares, int p_signedDeltaShares);
    [[nodiscard]] static double currentMetricForBasis(const RiskRuntimeState& p_state, RiskDrawdownBasis p_basis);

    [[nodiscard]] AccountState& ensureAccountState(const QString& p_accountId, const QDateTime& p_now);
    [[nodiscard]] RiskConfig clampConfig(const RiskConfig& p_config) const;
    void rolloverIfNeeded(const QString& p_accountId, AccountState& p_state, const QDateTime& p_now);
    void resetDayInternal(const QString& p_accountId,
                          AccountState& p_state,
                          const QDateTime& p_now,
                          const QString& p_reason);
    void appendEvent(const QString& p_accountId,
                     const QString& p_eventType,
                     const QString& p_reasonCode,
                     const QString& p_message,
                     const QString& p_dataJson = QString());
    void saveConfigToSettings(const QString& p_accountId, const RiskConfig& p_config);
    [[nodiscard]] RiskConfig loadConfigFromSettings(const QString& p_accountId) const;
    void bindToCurrentLedgerIfNeeded();
    void persistState(const QString& p_accountId, const AccountState& p_state);

    QMap<QString, AccountState> m_accounts;
    QString m_boundLedgerPath;
    std::unique_ptr<Store> m_store;
};
