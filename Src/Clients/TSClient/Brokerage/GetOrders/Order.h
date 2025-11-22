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

    QString accountID;
    std::optional<AdvancedOptions> advancedOptions;
    QDateTime closedDateTime;
    double comissionsFee;
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
    double limitPrice;
    QDateTime openedDateTime;
    QString orderID;

    OrderType orderType;

    double priceUsedForBuyingPower;
    std::optional<QString> rejectReason;
    QString routing;
    double showOnlyQuantity;
    double spread;

    // status
    QString statusDescription;
    double stopPrice;
    TrailingStop trailingStop;
    double unbundledRouteFee;


    bool isUpdate = false;                // Whether this position is an update

};

Q_DECLARE_METATYPE(Order)
