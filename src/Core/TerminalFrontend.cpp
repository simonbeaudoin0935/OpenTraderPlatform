#include "TerminalFrontend.h"
#include <QDebug>

TerminalFrontend::TerminalFrontend(QObject* parent) : AppFrontend(parent) {

}

void TerminalFrontend::onFMPClientDataUsageUpdate(qsizetype newDataUsage)
{
    Q_UNUSED(newDataUsage);
}

void TerminalFrontend::onTSClientDataUsageUpdate(qsizetype newDataUsage)
{
    Q_UNUSED(newDataUsage);
}

void TerminalFrontend::onMemoryUsageUpdate(qsizetype newDataUsage)
{
    Q_UNUSED(newDataUsage);
}

void TerminalFrontend::onStreamCountUpdate(int count)
{
    Q_UNUSED(count);
}

void TerminalFrontend::onTradeStationAccountsReceived(QVector<Account> results)
{
    Q_UNUSED(results);
}

void TerminalFrontend::onNewPositionReceived(QString account, Position position)
{
    Q_UNUSED(account);
    Q_UNUSED(position);
}

void TerminalFrontend::onCurrentHighlightedStockBarReceived(QString symbol, Bar bar)
{
    Q_UNUSED(symbol);
    Q_UNUSED(bar);
}

void TerminalFrontend::onCurrentHighlightedReceivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote, double bidAskImbalance, double bidDWP, double askDWP)
{
    Q_UNUSED(symbol);
    Q_UNUSED(quote);
    Q_UNUSED(bidAskImbalance);
    Q_UNUSED(bidDWP);
    Q_UNUSED(askDWP);
}

void TerminalFrontend::onRequestedMissingBarsDisplayedStockReceived(QVector<Bar> bars)
{
    Q_UNUSED(bars);
}

void TerminalFrontend::onMarketDepthNotAvailable()
{

}
