#include "TUIFrontend.h"
#include <QDebug>

TUIFrontend::TUIFrontend(MainAlgo* mainAlgo, QObject* parent) : FrontEnd(parent), mainAlgo(mainAlgo) {}

void TUIFrontend::onTSClientDataUsageUpdate(qsizetype newDataUsage)
{
    Q_UNUSED(newDataUsage);
}

void TUIFrontend::onMemoryUsageUpdate(qsizetype newDataUsage)
{
    Q_UNUSED(newDataUsage);
}

void TUIFrontend::onStreamCountUpdate(int count)
{
    Q_UNUSED(count);
}

void TUIFrontend::onTradeStationAccountsReceived(QVector<Account> results)
{
    Q_UNUSED(results);
}

void TUIFrontend::onNewPositionReceived(QString account, Position position)
{
    Q_UNUSED(account);
    Q_UNUSED(position);
}

void TUIFrontend::onPositionDeleted(QString account, QString positionID)
{
    Q_UNUSED(account);
    Q_UNUSED(positionID);
}

void TUIFrontend::onNewOrderReceived(QString account, Order order)
{
    Q_UNUSED(account);
    Q_UNUSED(order);
}

void TUIFrontend::onBalanceUpdated(Balance balance)
{
    Q_UNUSED(balance);
}

void TUIFrontend::onCurrentHighlightedStockBarReceived(QString symbol, Bar bar)
{
    Q_UNUSED(symbol);
    Q_UNUSED(bar);
}

void TUIFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol,
                                                                  MarketDepthQuote quote,
                                                                  double bidAskImbalance,
                                                                  double bidDWP,
                                                                  double askDWP)
{
    Q_UNUSED(symbol);
    Q_UNUSED(quote);
    Q_UNUSED(bidAskImbalance);
    Q_UNUSED(bidDWP);
    Q_UNUSED(askDWP);
}

void TUIFrontend::onRequestedMissingBarsDisplayedStockReceived(QVector<Bar> bars)
{
    Q_UNUSED(bars);
}
