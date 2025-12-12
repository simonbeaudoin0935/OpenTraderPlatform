#include "PositionsReceiver.h"
#include "TSClient.h"

#include <QFutureWatcher>

Q_LOGGING_CATEGORY(PositionsReceiverLog, "PositionsReceiver");

#define DEBUG qCDebug(PositionsReceiverLog) << this->objectName()
#define INFO qCInfo(PositionsReceiverLog) << this->objectName()
#define WARNING qCWarning(PositionsReceiverLog) << this->objectName()
#define CRITICAL qCCritical(PositionsReceiverLog) << this->objectName()

PositionsReceiver::PositionsReceiver(const QString &account, QObject *parent) :
    QObject(parent),
    m_account(account)
{
    this->setObjectName("PositionReceiver");
    
    DEBUG <<  "Starting Positions stream for account " << account;
    
    m_stream = TSClient::getInstance()->openStreamPositions(account);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamPositions::newPositionReceived, this, [this](Position position) {
        CRITICAL << "Positions Receiver received new position for account" << m_account;
        onReceivedNewPosition(m_account, position);
    });

    m_stream->future().then(
        [this](){
            CRITICAL << "Positions Receiver future finished for account" << m_account;
        }
    ).onFailed(this, [this](Stream::TimeoutException ex){
        WARNING << "Positions Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::BadRequestException ex){
        WARNING << "Positions Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::DualLogonException ex){
        WARNING << "Positions Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::GoAwayException ex){
        WARNING << "Positions Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::InternalServerErrorException ex){
        WARNING << "Positions Receiver future failed for" << m_account << "- Exception:" << ex.what();
    }).onFailed(this, [this](Stream::InvalidSymbolException ex){
        WARNING << "Positions Receiver future failed for" << m_account << "- Exception:" << ex.what();
    });
}

void PositionsReceiver::onReceivedNewPosition(QString account, Position position)
{
    qCDebug(PositionsReceiverLog).noquote() << "New position for account (" << account << ") : " << position.toJsonString();
    emit receivedNewPosition(account, position);
}
