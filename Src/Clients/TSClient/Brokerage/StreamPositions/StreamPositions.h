#pragma once

#include "Stream.h"
#include "Position.h"

class StreamPositions final : public Stream
{
    Q_OBJECT
    
public:
    explicit StreamPositions(const QString &accountID, QNetworkReply * reply, QObject *parent = nullptr);
    ~StreamPositions() {};
    StreamPositions(const StreamPositions&) = delete;
    StreamPositions& operator=(const StreamPositions&) = delete;

    QString getAccountID() {return m_accountID; };
    
signals:
    void newPositionReceived(Position position);
    void endSnapshotReceived();
    void positionDeleted(QString positionID);

private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QString m_accountID;
    bool m_receivedEndSnapshot = false;  // Track if we've received the EndSnapshot status
};
