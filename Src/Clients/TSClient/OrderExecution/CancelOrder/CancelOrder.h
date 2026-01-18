#pragma once

#include <QJsonObject>

class CancelOrderResult
{
  public:
    // Default constructor
    CancelOrderResult() = default;

    // Constructor taking a QJsonObject
    CancelOrderResult(const QJsonObject& jsonObj);

    // Getters
    QString getOrderID() const
    {
        return orderID;
    }
    QString getMessage() const
    {
        return message;
    }
    std::optional<QString> getError() const
    {
        return error;
    }

    // Check if this is an error result
    bool isError() const
    {
        return error.has_value();
    }

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

  private:
    QString orderID;              // Required
    QString message;              // Required
    std::optional<QString> error; // Optional, presence indicates error state
};
