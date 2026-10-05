#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

enum class RiskDrawdownBasis : quint8
{
    EquityPeak = 0,
    TodaysProfitLoss = 1,
    RealizedProfitLoss = 2,
    TodaysProfitLossFromBaseline = 3,
};

inline QString riskDrawdownBasisToString(const RiskDrawdownBasis p_basis)
{
    switch (p_basis)
    {
    case RiskDrawdownBasis::EquityPeak:
        return QStringLiteral("equity-peak");
    case RiskDrawdownBasis::TodaysProfitLoss:
        return QStringLiteral("todays-pnl");
    case RiskDrawdownBasis::RealizedProfitLoss:
        return QStringLiteral("realized-pnl");
    case RiskDrawdownBasis::TodaysProfitLossFromBaseline:
        return QStringLiteral("todays-pnl-baseline");
    }

    return QStringLiteral("equity-peak");
}

inline RiskDrawdownBasis riskDrawdownBasisFromInt(const int p_value)
{
    switch (p_value)
    {
    case static_cast<int>(RiskDrawdownBasis::TodaysProfitLoss):
        return RiskDrawdownBasis::TodaysProfitLoss;
    case static_cast<int>(RiskDrawdownBasis::RealizedProfitLoss):
        return RiskDrawdownBasis::RealizedProfitLoss;
    case static_cast<int>(RiskDrawdownBasis::TodaysProfitLossFromBaseline):
        return RiskDrawdownBasis::TodaysProfitLossFromBaseline;
    case static_cast<int>(RiskDrawdownBasis::EquityPeak):
    default:
        return RiskDrawdownBasis::EquityPeak;
    }
}

struct RiskConfig
{
    bool enabled = true;
    double dailyDrawdownLimitUsd = 1000.0;
    RiskDrawdownBasis dailyDrawdownBasis = RiskDrawdownBasis::EquityPeak;
    double maxPlannedLossPerTradeUsd = 200.0;
    int maxPositionShares = 5000;
    double maxPositionNotionalUsd = 100000.0;
    int maxDailyEntryTrades = 20;
    int maxOpenPositions = 10;
    bool cooldownEnabled = true;
    double cooldownLossTriggerUsd = 200.0;
    int cooldownDurationSec = 300;
    int warningAmberUsedPercent = 50;
    int warningRedUsedPercent = 80;
};

struct RiskRuntimeState
{
    QDate riskDay;
    RiskDrawdownBasis drawdownBasis = RiskDrawdownBasis::EquityPeak;
    double basisPeak = 0.0;
    double basisBaseline = 0.0;
    double currentMetric = 0.0;
    bool tradingLocked = false;
    QString lockReason;
    int entryTradesCount = 0;
    int openPositionsCount = 0;
    QDateTime cooldownUntil;
    double lastEquity = 0.0;
    double lastTodaysPnl = 0.0;
    double lastRealizedPnl = 0.0;
};

struct RiskStatusSnapshot
{
    QString accountId;
    RiskConfig config;
    RiskRuntimeState runtime;
    double drawdownAmountUsd = 0.0;
    double drawdownRemainingUsd = 0.0;
    double drawdownRemainingPercent = 100.0;
    bool cooldownActive = false;
    QString summaryText;
    QString detailsText;
};

struct RiskDecision
{
    bool allow = true;
    QString reasonCode;
    QString message;
    bool isEntry = false;
    int openingShares = 0;
    double plannedLossUsd = 0.0;
};
