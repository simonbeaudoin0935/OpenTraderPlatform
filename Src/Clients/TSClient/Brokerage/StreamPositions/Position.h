#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>

class Position
{
  public:
    // Default constructor
    Position() = default;

    // Constructor taking a QJsonObject
    Position(const QJsonObject& jsonObj, bool isUpdate = false);

    // Getters
    QString getAccountID() const
    {
        return accountID;
    }
    QString getAssetType() const
    {
        return assetType;
    }
    QString getAveragePrice() const
    {
        return averagePrice;
    }
    QString getBid() const
    {
        return bid;
    }
    QString getAsk() const
    {
        return ask;
    }
    QString getConversionRate() const
    {
        return conversionRate;
    }
    bool isDeleted() const
    {
        return deleted;
    }
    QString getDayTradeRequirement() const
    {
        return dayTradeRequirement;
    }
    QDateTime getExpirationDate() const
    {
        return expirationDate;
    }
    QDateTime getOpenedDateTime() const
    {
        return openedDateTime;
    }
    QDateTime getClosedDateTime() const
    {
        return closedDateTime;
    }
    QString getInitialRequirement() const
    {
        return initialRequirement;
    }
    QString getMaintenanceMargin() const
    {
        return maintenanceMargin;
    }
    QString getLast() const
    {
        return last;
    }
    QString getLongShort() const
    {
        return longShort;
    }
    QString getMarkToMarketPrice() const
    {
        return markToMarketPrice;
    }
    QString getMarketValue() const
    {
        return marketValue;
    }
    QString getPositionID() const
    {
        return positionID;
    }
    QString getQuantity() const
    {
        return quantity;
    }
    QString getSymbol() const
    {
        return symbol;
    }
    QDateTime getTimestamp() const
    {
        return timestamp;
    }
    QString getTodaysProfitLoss() const
    {
        return todaysProfitLoss;
    }
    QString getTotalCost() const
    {
        return totalCost;
    }
    QString getUnrealizedProfitLoss() const
    {
        return unrealizedProfitLoss;
    }
    QString getUnrealizedProfitLossPercent() const
    {
        return unrealizedProfitLossPercent;
    }
    QString getUnrealizedProfitLossQty() const
    {
        return unrealizedProfitLossQty;
    }
    bool isPositionUpdate() const
    {
        return isUpdate;
    } // Returns whether this position is an update

    // Validation
    bool isValid() const;

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

  private:
    QString accountID;                   // Required
    QString assetType;                   // Required
    QString averagePrice;                // Required
    QString bid;                         // Required
    QString ask;                         // Required
    QString conversionRate;              // Required
    bool deleted = false;                // Optional, defaults to false
    QString dayTradeRequirement;         // Required
    QDateTime expirationDate;            // Optional
    QDateTime openedDateTime;            // Optional
    QDateTime closedDateTime;            // Optional
    QString initialRequirement;          // Required
    QString maintenanceMargin;           // Required
    QString last;                        // Required
    QString longShort;                   // Required
    QString markToMarketPrice;           // Required
    QString marketValue;                 // Required
    QString positionID;                  // Required
    QString quantity;                    // Required
    QString symbol;                      // Required
    QDateTime timestamp;                 // Required
    QString todaysProfitLoss;            // Required
    QString totalCost;                   // Required
    QString unrealizedProfitLoss;        // Required
    QString unrealizedProfitLossPercent; // Required
    QString unrealizedProfitLossQty;     // Required
    bool isUpdate = false;               // Whether this position is an update
};

Q_DECLARE_METATYPE(Position)
