#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QTimeZone>
#include <optional>

#include "PlaceOrder.h"
#include "Logging.h"
#include "CONSTANTS.h"
#include "Assume.h"


namespace OrderNS
{

    class AdvancedOptions
    {
        Q_GADGET

      public:
        QString toString();

        AdvancedOptions(const QString& str);

        enum class Type
        {
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
        Q_ENUM(Type)

        Type type;
        std::optional<double> pegval;
        std::optional<double> shwqty;
        std::optional<double> dscpr;
    };

    struct ConditionalOrder
    {
        QString orderID;
        QString relationship;
    };

    struct Leg
    {
        // TODO
    };

} // namespace OrderNS


class Order
{
    Q_GADGET

  public:
    // Enum for order status codes
    enum class Status : quint8
    {
        ACK, // Received
        BRO, // Broken
        CAN, // Canceled
        EXP, // Expired
        FLL, // Filled
        FLP, // Partial Fill (UROut)
        FPR, // Partial Fill (Alive)
        LAT, // Too Late to Cancel
        OPN, // Sent
        OUT, // UROut
        REJ, // Rejected
        UCH, // Replaced
        UCN, // Cancel Sent
        TSC, // Trade Server Canceled
        RJC, // Cancel Request Rejected
        DON, // Queued
        RSN, // Replace Sent
        CND, // Condition Met
        OSO, // OSO Order
        SUS  // Suspended
    };
    Q_ENUM(Status)

    Order(const QJsonObject& jsonObj, bool isUpdate = false);
    bool isValid();

    // Getters for display
    QString getOrderID() const
    {
        return m_orderID;
    }
    QString getSymbol() const
    {
        return m_symbol;
    }
    QString getAccountID() const
    {
        return m_accountID;
    }
    QString getStatusDescription() const
    {
        return m_statusDescription;
    }

    /**
     * @brief Get the official TradeStation API status description for a given status
     * @param status The Order::Status enum value
     * @return The official status description string from TradeStation API
     *
     * This is the single source of truth for status descriptions matching
     * TradeStation's API documentation exactly.
     */
    static QString getStatusDescriptionForStatus(Status status)
    {
        switch (status)
        {
        case Status::ACK:
            return "Received";
        case Status::BRO:
            return "Broken";
        case Status::CAN:
            return "Canceled";
        case Status::EXP:
            return "Expired";
        case Status::FLL:
            return "Filled";
        case Status::FLP:
            return "Partial Fill (UROut)";
        case Status::FPR:
            return "Partial Fill (Alive)";
        case Status::LAT:
            return "Too Late to Cancel";
        case Status::OPN:
            return "Sent";
        case Status::OUT:
            return "UROut";
        case Status::REJ:
            return "Rejected";
        case Status::UCH:
            return "Replaced";
        case Status::UCN:
            return "Cancel Sent";
        case Status::TSC:
            return "Trade Server Canceled";
        case Status::RJC:
            return "Cancel Request Rejected";
        case Status::DON:
            return "Queued";
        case Status::RSN:
            return "Replace Sent";
        case Status::CND:
            return "Condition Met";
        case Status::OSO:
            return "OSO Order";
        case Status::SUS:
            return "Suspended";
        }

        // Should never reach here - all enum values must be handled
        ASSUME_TRUE(false);
        return ""; // Unreachable
    }

    Status getOrderStatus() const
    {
        return m_orderStatus;
    }
    QString getQuantity() const
    {
        return m_quantity;
    }
    QString getTradeAction() const
    {
        return m_tradeAction;
    }
    QString getDuration() const
    {
        return m_duration;
    }
    OrderType getOrderType() const
    {
        return m_orderType;
    }
    std::optional<double> getLimitPrice() const
    {
        return m_limitPrice;
    }
    std::optional<double> getStopPrice() const
    {
        return m_stopPrice;
    }
    double getFilledPrice() const
    {
        return m_filledPrice;
    }
    QDateTime getOpenedDateTime() const
    {
        return m_openedDateTime;
    }
    QDateTime getClosedDateTime() const
    {
        return m_closedDateTime;
    }
    std::optional<QString> getRejectReason() const
    {
        return m_rejectReason;
    }
    std::optional<qint64> getLatencyMs() const
    {
        return m_latencyMs;
    }

    void setLatencyMs(qint64 p_latencyMs)
    {
        m_latencyMs = p_latencyMs;
    }

    std::optional<QString> getStrategyLog() const
    {
        return m_strategyLog;
    }

    void setStrategyLog(const QString& p_log)
    {
        m_strategyLog = p_log;
    }

    QString m_accountID;
    std::optional<OrderNS::AdvancedOptions> m_advancedOptions;
    QDateTime m_closedDateTime;
    double m_commissionsFee;
    std::optional<QVector<OrderNS::ConditionalOrder>> m_conditionalOrders;
    double m_conversionRate;
    QString m_currency;
    QString m_duration;
    double m_filledPrice;
    std::optional<QDateTime> m_goodTillDate;
    std::optional<QString> m_groupName;
    std::optional<QVector<OrderNS::Leg>> m_legs;
    std::optional<QVector<MarketActivationRule>> m_marketActivationsRules;
    std::optional<QVector<TimeActivationRule>> m_timeActivationRules;
    std::optional<double> m_limitPrice;
    QDateTime m_openedDateTime;
    QString m_orderID;

    OrderType m_orderType;

    double m_priceUsedForBuyingPower;
    std::optional<QString> m_rejectReason;
    QString m_routing;
    double m_showOnlyQuantity;
    double m_spread;

    // Additional fields for display
    QString m_symbol;
    QString m_quantity;
    QString m_tradeAction;

    // status
    Status m_orderStatus;
    QString m_statusDescription;
    std::optional<double> m_stopPrice;
    TrailingStop m_trailingStop;
    double m_unbundledRouteFee;


    bool m_isUpdate = false; // Whether this order is an update

    // Ack/fill latency in milliseconds, computed once at the moment we receive the ack or fill
    // and stored as-is in the database. This value never changes after initial computation.
    std::optional<qint64> m_latencyMs;

    // Optional log message attached by the strategy when the order was placed.
    std::optional<QString> m_strategyLog;
};

Q_DECLARE_METATYPE(Order)
