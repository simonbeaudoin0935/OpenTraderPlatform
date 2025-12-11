#include "PositionsReceiver.h"
#include "TSClient.h"

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
    QPair<QFuture<Position>, StreamPositions*> pair = TSClient::getInstance()->openStreamPositions(account);

    m_stream = pair.second;
    Q_CHECK_PTR(m_stream);

    pair.first.then(this, [this, account](Position position){
        CRITICAL << "Received new Position for account" << account;
        onReceivedNewPosition(account, position);
    }).onFailed(this, [this, account](Stream::TimeoutException ex){
        WARNING << "Positions Receiver future failed for" << account << "- Exception:" << ex.what();
    }).onFailed(this, [this, account](Stream::BadRequestException ex){
        WARNING << "Positions Receiver future failed for" << account << "- Exception:" << ex.what();
    }).onFailed(this, [this, account](Stream::DualLogonException ex){
        WARNING << "Positions Receiver future failed for" << account << "- Exception:" << ex.what();
    }).onFailed(this, [this, account](Stream::GoAwayException ex){
        WARNING << "Positions Receiver future failed for" << account << "- Exception:" << ex.what();
    }).onFailed(this, [this, account](Stream::InternalServerErrorException ex){
        WARNING << "Positions Receiver future failed for" << account << "- Exception:" << ex.what();
    }).onFailed(this, [this, account](Stream::InvalidSymbolException ex){
        WARNING << "Positions Receiver future failed for" << account << "- Exception:" << ex.what();
    }).onFinished();
}

void PositionsReceiver::onReceivedNewPosition(QString account, Position position)
{
    qCDebug(PositionsReceiverLog).noquote() << "New position for account (" << account << ") : " << position.toJsonString();
    emit receivedNewPosition(account, position);
}
