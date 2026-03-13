#include "Level2Receiver.h"

#include "Logging.h"

#define LOGGING_CATEGORY Level2ReceiverLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "Level2Receiver")

Level2Receiver::Level2Receiver(const QString& symbol, QObject* parent) : StreamReceiver(parent), m_symbol(symbol)
{
    setObjectName("Level2Receiver::" + symbol);
}

void Level2Receiver::openStream()
{
    // Subscriptions are managed by SymbolContext via DBClient::subscribeLive()
    DEBUG << "Level2Receiver ready for" << m_symbol << "(subscription via SymbolContext)";
}

void Level2Receiver::onReceivedNewLevel2(Level2 level2)
{
    emit receivedNewLevel2(m_symbol, level2);
}
