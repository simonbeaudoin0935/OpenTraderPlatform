#pragma once

#include <QString>
#include <QtGlobal>

struct StrategySymbolViewState
{
    QString symbol;
    int originalOrder = 0;
    bool hasActivity = false;
    bool hasTradeHistory = false;
    quint64 latestActivitySequence = 0;
    qint64 openQuantity = 0;
    double averagePrice = 0.0;
    double realizedPnl = 0.0;
    double unrealizedPnl = 0.0;
};
