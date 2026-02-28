#include "BarReceiver.h"
#include "Logging.h"

#define LOGGING_CATEGORY BarReceiverLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "BarReceiver")

BarReceiver::BarReceiver(const QString& symbol, QObject* parent) : StreamReceiver(parent), m_symbol(symbol)
{
    setObjectName("BarReceiver::" + symbol);
    // TODO Phase 6: subscribe via DBClient (Schema::Ohlcv1M for historical, Schema::Trades for live bars)
    DEBUG << "BarReceiver created for" << m_symbol << "(stream subscription pending Phase 6)";
}
