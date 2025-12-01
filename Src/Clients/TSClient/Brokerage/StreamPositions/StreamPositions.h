#pragma once

#include "Stream.h"
#include "Position.h"


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


class StreamPositions final : public Stream
{
    Q_OBJECT
    
public:
    explicit StreamPositions(QString &accountID, QObject *parent = nullptr);

    ~StreamPositions();
    StreamPositions(const StreamPositions&) = delete;
    StreamPositions& operator=(const StreamPositions&) = delete;

signals:
    void receivedNewPosition(QString accountID, Position position);

private:
    QString accountID;
    bool receivedEndSnapshot = false;  // Track if we've received the EndSnapshot status
    bool processJsonObject(const QJsonObject& jsonObj) override;
};
