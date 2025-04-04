#pragma once

#include "../../Clients/TSClient/Stream/Stream.h"
#include "Position.h"

class StreamPositions : public Stream
{
    Q_OBJECT
public:
    class StreamPositionStatus {
    public:
        enum class Status {
            EndSnapshot,  // Initial snapshot is complete
            GoAway,      // Server is about to shut down
            Unknown      // Any other status value
        };

        // Default constructor
        StreamPositionStatus() = default;
        
        // Constructor taking a QJsonObject
        StreamPositionStatus(const QJsonObject& jsonObj);

        // Getters
        Status getStatus() const { return status; }
        QString getStatusString() const { return statusString; }

        // Validation
        bool isValid() const;
        bool isStatusValid() const;

        // Convert to JSON string for debugging/logging
        QString toJsonString() const;

    private:
        Status status = Status::Unknown;
        QString statusString;  // Original status string from JSON
    };

    // TODO make it multiple accounts
    explicit StreamPositions(QString &account, QObject *parent = nullptr);

    ~StreamPositions();
    StreamPositions(const StreamPositions&) = delete;
    StreamPositions& operator=(const StreamPositions&) = delete;

signals:
    void receivedNewPosition(QString account, Position position);

private:
    QString account;
    bool receivedEndSnapshot = false;  // Track if we've received the EndSnapshot status
    virtual bool processJsonObject(const QJsonObject& jsonObj);
};
