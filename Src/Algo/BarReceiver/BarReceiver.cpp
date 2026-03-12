#include "BarReceiver.h"
#include "Logging.h"

#define LOGGING_CATEGORY BarReceiverLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "BarReceiver")

BarReceiver::BarReceiver(const QString& symbol, QObject* parent) : StreamReceiver(parent), m_symbol(symbol)
{
    setObjectName("BarReceiver::" + symbol);
    // Bars arrive via LiveBarAccumulator::barClosed → receivedNewBar (wired in SymbolContext)
    DEBUG << "BarReceiver created for" << m_symbol;
}
