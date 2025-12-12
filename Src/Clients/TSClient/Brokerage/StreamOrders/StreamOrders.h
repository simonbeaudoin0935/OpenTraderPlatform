#pragma once

#include "Stream.h"
#include "Order.h"

class StreamOrders final : public Stream
{
    Q_OBJECT

public:
    class StreamOrdersStatus {
    public:
        enum class Status {
            EndSnapshot,  // Initial snapshot is complete
            GoAway,      // Server is about to shut down
            Unknown      // Any other status value
        };

        // Default constructor
        StreamOrdersStatus() = default;

        // Constructor taking a QJsonObject
        StreamOrdersStatus(const QJsonObject& jsonObj);

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
    explicit StreamOrders(const QString &account, QNetworkReply * reply, QObject *parent = nullptr);
    ~StreamOrders() {};
    StreamOrders(const StreamOrders&) = delete;
    StreamOrders& operator=(const StreamOrders&) = delete;

    QString getAccountID() {return m_accountID; };
signals:
    void newOrderReceived(Order order);

private:
    void processJsonObject(const QJsonObject& jsonObj) override;

    QString m_accountID;
    
    bool receivedEndSnapshot = false;  // Track if we've received the EndSnapshot status
};
