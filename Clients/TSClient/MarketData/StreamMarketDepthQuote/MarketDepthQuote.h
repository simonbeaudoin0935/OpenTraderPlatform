#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QVector>
#include <QMetaType>

class MarketDepthLevel {
public:
    // Default constructor
    MarketDepthLevel() = default;
    
    // Constructor taking a QJsonObject
    MarketDepthLevel(const QJsonObject& jsonObj);

    // Getters
    QDateTime getTimeStamp() const { return timeStamp; }
    QString getSide() const { return side; }
    QString getPrice() const { return price; }
    QString getSize() const { return size; }
    int getOrderCount() const { return orderCount; }
    QString getName() const { return name; }

    // Validation
    bool isValid() const;

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

private:
    QDateTime timeStamp;  // Required
    QString side;        // Required, "Bid" or "Ask"
    QString price;       // Required
    QString size;        // Required
    int orderCount;      // Required
    QString name;        // Required
};

class MarketDepthQuote {
public:
    // Default constructor
    MarketDepthQuote() = default;
    
    // Constructor taking a QJsonObject
    MarketDepthQuote(const QJsonObject& jsonObj);

    // Getters
    const QVector<MarketDepthLevel>& getBids() const { return bids; }
    const QVector<MarketDepthLevel>& getAsks() const { return asks; }

    // Helper methods
    bool isEmpty() const { return bids.isEmpty() && asks.isEmpty(); }
    bool isValid() const;

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

private:
    QVector<MarketDepthLevel> bids;  // Array of bid levels
    QVector<MarketDepthLevel> asks;  // Array of ask levels
};

Q_DECLARE_METATYPE(MarketDepthLevel)
Q_DECLARE_METATYPE(MarketDepthQuote)
