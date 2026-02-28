#include "Level1Receiver.h"

#include "Logging.h"

#define LOGGING_CATEGORY Level1ReceiverLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "Level1Receiver")

Level1Receiver::Level1Receiver(const QString& symbol, QObject* parent) : StreamReceiver(parent), m_symbol(symbol)
{
    setObjectName("Level1Receiver::" + symbol);
}

void Level1Receiver::openStream()
{
    // Subscriptions are managed by StockInstruments via DBClient::subscribeLive()
    DEBUG << "Level1Receiver ready for" << m_symbol << "(subscription via StockInstruments)";
}

void Level1Receiver::onReceivedNewLevel1(Level1 level1)
{
    emit receivedNewLevel1(m_symbol, level1);
}
