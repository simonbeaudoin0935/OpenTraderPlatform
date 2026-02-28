#include "Level1Receiver.h"

#include "Logging.h"

#define LOGGING_CATEGORY Level1ReceiverLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "Level1Receiver")

Level1Receiver::Level1Receiver(const QString& symbol, QObject* parent) : StreamReceiver(parent), m_symbol(symbol)
{
    setObjectName("Level1Receiver::" + symbol);
    openStream();
}

void Level1Receiver::openStream()
{
    // TODO Phase 6: subscribe via DBClient (Schema::Mbp1)
    DEBUG << "Level1Receiver: DBClient subscription not yet wired for" << m_symbol;
}

void Level1Receiver::onReceivedNewLevel1(Level1 level1)
{
    emit receivedNewLevel1(m_symbol, level1);
}
