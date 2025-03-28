#include "terminalfrontend.h"
#include <QDebug>

TerminalFrontend::TerminalFrontend(QObject* parent) : AppFrontend(parent) {

}

void TerminalFrontend::onFMPClientDataUsageUpdate(qsizetype newDataUsage)
{
    Q_UNUSED(newDataUsage);
}

void TerminalFrontend::onMemoryUsageUpdate(qint64 newDataUsage)
{
    Q_UNUSED(newDataUsage);
}
