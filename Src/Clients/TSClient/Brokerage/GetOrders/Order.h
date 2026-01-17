#pragma once

#include <QDateTime>
#include <QMetaType>
#include <optional>

#include "PlaceOrder.h"

class AdvancedOptions {

public:
    QString toString();

    AdvancedOptions(const QString &str);

    enum class Type {
        CND,    // Activation rule
        AON,    // All or None
        TRL,    // Trailing stop
        SHWQTY, // Show only
        DSCPR,  // Discretionary price
        NON,    // Non-display
        PEGVAL, // Peg value
        BKO,    // Book only
        PSO     // Add liquidity
    };

    Type type;
    std::optional<double> pegval;
    std::optional<double> shwqty;
    std::optional<double> dscpr;
};

// Enum for order status codes
enum class OrderStatus {
    ACK,  // Received
    BRO,  // Broken
    CAN,  // Canceled
    EXP,  // Expired
    FLL,  // Filled
    FLP,  // Partial Fill (UROut)
    FPR,  // Partial Fill (Alive)
    LAT,  // Too Late to Cancel
    OPN,  // Sent
    OUT,  // UROut
    REJ,  // Rejected
    UCH,  // Replaced
    UCN,  // Cancel Sent
    TSC,  // Trade Server Canceled
    RJC,  // Cancel Request Rejected
    DON,  // Queued
    RSN,  // Replace Sent
    CND,  // Condition Met
    OSO,  // OSO Order
    SUS   // Suspended
};

struct ConditionalOrder {
    QString orderID;
    QString relationship;
};

struct Leg {
    // TODO
};

class Order {

public:
    Order(const QJsonObject& jsonObj, bool isUpdate = false);
    bool isValid();
    
    // Getters for display
    QString getOrderID() const { return orderID; }
    QString getSymbol() const { return symbol; }
    QString getAccountID() const { return accountID; }
    QString getStatusDescription() const { return statusDescription; }
    OrderStatus getOrderStatus() const { return orderStatus; }
    QString getQuantity() const { return quantity; }
    QString getTradeAction() const { return tradeAction; }
    QString getDuration() const { return duration; }
    OrderType getOrderType() const { return orderType; }
    std::optional<double> getLimitPrice() const { return limitPrice; }
    std::optional<double> getStopPrice() const { return stopPrice; }
    double getFilledPrice() const { return filledPrice; }
    QDateTime getOpenedDateTime() const { return openedDateTime; }
    QString getRouting() const { return routing; }
    std::optional<QDateTime> getReceivedTime() const { return receivedTime; }
    std::optional<QDateTime> getFilledTime() const { return filledTime; }

    // Setters for tracking times
    void setReceivedTime(const QDateTime& p_time) { receivedTime = p_time; }
    void setFilledTime(const QDateTime& p_time) { filledTime = p_time; }

    QString accountID;
    std::optional<AdvancedOptions> advancedOptions;
    QDateTime closedDateTime;
    double commissionsFee;
    std::optional<QVector<ConditionalOrder>> conditionalOrders;
    double conversionRate;
    QString currency;
    QString duration;
    double filledPrice;
    std::optional<QDateTime> goodTillDate;
    std::optional<QString> groupName;
    std::optional<QVector<Leg>> legs;
    std::optional<QVector<MarketActivationRule>> marketActivationsRules;
    std::optional<QVector<TimeActivationRule>> timeActivationRules;
    std::optional<double> limitPrice;
    QDateTime openedDateTime;
    QString orderID;

    OrderType orderType;

    double priceUsedForBuyingPower;
    std::optional<QString> rejectReason;
    QString routing;
    double showOnlyQuantity;
    double spread;

    // Additional fields for display
    QString symbol;
    QString quantity;
    QString tradeAction;

    // status
    OrderStatus orderStatus;
    QString statusDescription;
    std::optional<double> stopPrice;
    TrailingStop trailingStop;
    double unbundledRouteFee;


    bool isUpdate = false;                // Whether this order is an update

    // Tracking times for order lifecycle
    std::optional<QDateTime> receivedTime;  // When we first received this order
    std::optional<QDateTime> filledTime;    // When this order was filled

};

Q_DECLARE_METATYPE(Order)
