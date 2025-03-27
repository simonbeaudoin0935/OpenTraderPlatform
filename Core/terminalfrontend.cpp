#include "terminalfrontend.h"
#include <QDebug>

TerminalFrontend::TerminalFrontend(QObject* parent) : AppFrontend(parent) {

}

void TerminalFrontend::onPriceUpdated(const QJsonObject& priceData)
{
    Q_UNUSED(priceData);
}

void TerminalFrontend::onPricesFetched() {
}

void TerminalFrontend::onFMPClientDataUsageUpdate(qsizetype newDataUsage)
{
    Q_UNUSED(newDataUsage);
}

void TerminalFrontend::onMemoryUsageUpdate(qint64 newDataUsage)
{
    Q_UNUSED(newDataUsage);
}
