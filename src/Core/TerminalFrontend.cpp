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

void TerminalFrontend::onMemoryUsageUpdate(qint64 newDataUsage)
{
    Q_UNUSED(newDataUsage);
}

void TerminalFrontend::onTradeStationAccountsReceived(QVector<AccountsResult> results)
{
    Q_UNUSED(results);
}

void TerminalFrontend::onMarketDepthNotAvailable()
{

}
