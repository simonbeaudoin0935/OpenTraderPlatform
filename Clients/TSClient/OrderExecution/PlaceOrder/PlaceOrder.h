#ifndef PLACE_ORDER_H
#define PLACE_ORDER_H

#include <optional>
#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QVector>
#include <QMetaType>

// Enum for order types
enum class OrderType {
    Market,
    Limit,
    StopMarket,
    StopLimit
};

// Enum for trade actions
enum class TradeAction {
    // Equities and futures
    Buy,
    Sell,
    
    // Equities only
    BuyToCover,
    SellShort,
    BuyToOpen,
    BuyToClose,
    SellToOpen,
    SellToClose
};

// Enum for order duration
enum class OrderDuration {
    // Regular trading session
    Day,           // Valid until the end of the regular trading session
    DayPlus,       // Valid until the end of the extended trading session
    
    // Good till canceled (90 days max)
    GTC,           // Good till canceled
    GTCPlus,       // Good till canceled plus
    
    // Good through date (90 days max)
    GTD,           // Good through date
    GTDPlus,       // Good through date plus
    
    // Session specific
    Opening,       // At the opening; only valid for listed stocks at the opening session Price
    OnClose,       // On Close; orders that target the closing session of an exchange
    
    // Immediate execution
    IOC,           // Immediate or Cancel; filled immediately or canceled, partial fills are accepted
    FOK,           // Fill or Kill; orders are filled entirely or canceled, partial fills are not accepted
    
    // Time-based (equity orders only)
    OneMinute,     // 1 minute; expires after the 1 minute
    ThreeMinutes,  // 3 minutes; expires after the 3 minutes
    FiveMinutes    // 5 minutes; expires after the 5 minutes
};

// Enum for market activation rule predicates
enum class MarketActivationRulePredicate {
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual
};

// Enum for market activation rule trigger keys
enum class MarketActivationRuleTriggerKey {
    STT,    // Single Tick Trigger
    STTN,   // Single Tick Trigger Next
    SBA,    // Single Bid Ask
    SAB,    // Single Ask Bid
    DTT,    // Double Tick Trigger
    DTTN,   // Double Tick Trigger Next
    DBA,    // Double Bid Ask
    DAB,    // Double Ask Bid
    TTT,    // Triple Tick Trigger
    TTTN,   // Triple Tick Trigger Next
    TBA,    // Triple Bid Ask
    TAB     // Triple Ask Bid
};

// Enum for market activation rule logic operators
enum class MarketActivationRuleLogicOperator {
    And,
    Or
};

// Enum for peg values
enum class PegValue {
    Best,
    Mid
};

class TimeInForce {
public:
    // Constructor
    TimeInForce(OrderDuration duration);
    
    // Setters
    void setDuration(OrderDuration value);
    void setExpiration(const std::optional<QString>& value);
    
    // Getters
    OrderDuration getDuration() const;
    std::optional<QString> getExpiration() const;
    
    // Convert to JSON
    QJsonObject toJson() const;
    
    // Static helper function to validate expiration date
    static bool isValidExpiration(const QString& expiration);

private:
    OrderDuration duration;
    std::optional<QString> expiration;
};

class MarketActivationRule {
public:
    void setRuleType(const QString& value) { 
        Q_ASSERT_X(value == "Price", "MarketActivationRule", "Currently only Price is supported for RuleType");
        ruleType = value; 
    }
    void setSymbol(const QString& value) { symbol = value; }
    void setPredicate(MarketActivationRulePredicate value) { predicate = value; }
    void setTriggerKey(MarketActivationRuleTriggerKey value) { triggerKey = value; }
    void setPrice(const QString& value) { price = value; }
    void setLogicOperator(MarketActivationRuleLogicOperator value) { logicOperator = value; }

    QString getRuleType() const { return ruleType; }
    QString getSymbol() const { return symbol; }
    MarketActivationRulePredicate getPredicate() const { return predicate; }
    MarketActivationRuleTriggerKey getTriggerKey() const { return triggerKey; }
    QString getPrice() const { return price; }
    MarketActivationRuleLogicOperator getLogicOperator() const { return logicOperator; }

    QJsonObject toJson() const;

private:
    QString ruleType;
    QString symbol;
    MarketActivationRulePredicate predicate;
    MarketActivationRuleTriggerKey triggerKey;
    QString price;
    MarketActivationRuleLogicOperator logicOperator;
};

class TimeActivationRule {
public:
    void setTimeUtc(const QString& value) { timeUtc = value; }

    QString getTimeUtc() const { return timeUtc; }

    QJsonObject toJson() const;

private:
    QString timeUtc;  // RFC3339 formatted date
};

class TrailingStop {
public:
    void setAmount(const std::optional<QString>& value) { amount = value; }
    void setPercent(const std::optional<QString>& value) { percent = value; }

    std::optional<QString> getAmount() const { return amount; }
    std::optional<QString> getPercent() const { return percent; }

    QJsonObject toJson() const;

private:
    std::optional<QString> amount;    // Currency offset
    std::optional<QString> percent;   // Percentage offset
};

class AdvancedOptions {
public:
    // Setters
    void setAddLiquidity(const std::optional<bool>& value) { addLiquidity = value; }
    void setAllOrNone(const std::optional<bool>& value) { allOrNone = value; }
    void setBookOnly(const std::optional<bool>& value) { bookOnly = value; }
    void setDiscretionaryPrice(const std::optional<QString>& value) { discretionaryPrice = value; }
    void setMarketActivationRules(const QVector<MarketActivationRule>& value) { marketActivationRules = value; }
    void setNonDisplay(const std::optional<bool>& value) { nonDisplay = value; }
    void setPegValue(const std::optional<PegValue>& value) { pegValue = value; }
    void setShowOnlyQuantity(const std::optional<QString>& value) { showOnlyQuantity = value; }
    void setTimeActivationRules(const QVector<TimeActivationRule>& value) { timeActivationRules = value; }
    void setTrailingStop(const std::optional<TrailingStop>& value) { trailingStop = value; }

    // Getters
    std::optional<bool> getAddLiquidity() const { return addLiquidity; }
    std::optional<bool> getAllOrNone() const { return allOrNone; }
    std::optional<bool> getBookOnly() const { return bookOnly; }
    std::optional<QString> getDiscretionaryPrice() const { return discretionaryPrice; }
    const QVector<MarketActivationRule>& getMarketActivationRules() const { return marketActivationRules; }
    std::optional<bool> getNonDisplay() const { return nonDisplay; }
    std::optional<PegValue> getPegValue() const { return pegValue; }
    std::optional<QString> getShowOnlyQuantity() const { return showOnlyQuantity; }
    const QVector<TimeActivationRule>& getTimeActivationRules() const { return timeActivationRules; }
    std::optional<TrailingStop> getTrailingStop() const { return trailingStop; }

    QJsonObject toJson() const;

private:
    std::optional<bool> addLiquidity;
    std::optional<bool> allOrNone;
    std::optional<bool> bookOnly;
    std::optional<QString> discretionaryPrice;
    QVector<MarketActivationRule> marketActivationRules;
    std::optional<bool> nonDisplay;
    std::optional<PegValue> pegValue;
    std::optional<QString> showOnlyQuantity;
    QVector<TimeActivationRule> timeActivationRules;
    std::optional<TrailingStop> trailingStop;
};

// TODO Add buying power warning here
// TODO Add legs here
// TODO Add OSO here    
class PlaceOrderRequest {
public:
    // Constructor
    PlaceOrderRequest();

    // Setters for required fields
    void setAccountID(const QString& value);
    void setOrderType(OrderType value);
    void setQuantity(int value);
    void setSymbol(const QString& value);
    void setTimeInForce(const TimeInForce& value);
    void setTradeAction(TradeAction value);

    // Setters for optional fields
    void setAdvancedOptions(const std::optional<AdvancedOptions>& value) { advancedOptions = value; }
    void setLimitPrice(const std::optional<double>& value);
    void setOrderConfirmID(const std::optional<QString>& value);
    void setRoute(const std::optional<QString>& value);
    void setStopPrice(const std::optional<double>& value);
    void setOcaGroupName(const std::optional<QString>& value);
    void setOcaGroupType(const std::optional<QString>& value);

    // Getters for all fields
    QString getAccountID() const;
    OrderType getOrderType() const;
    int getQuantity() const;
    QString getSymbol() const;
    TimeInForce getTimeInForce() const;
    TradeAction getTradeAction() const;
    std::optional<AdvancedOptions> getAdvancedOptions() const { return advancedOptions; }
    std::optional<double> getLimitPrice() const;
    std::optional<QString> getOrderConfirmID() const;
    std::optional<QString> getRoute() const;
    std::optional<double> getStopPrice() const;
    std::optional<QString> getOcaGroupName() const;
    std::optional<QString> getOcaGroupType() const;

    // Validation
    bool isValid() const;

    // Convert to JSON for API request
    QJsonObject toJson() const;
    
    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

private:
    // Required fields
    QString accountID;
    OrderType orderType;
    int quantity;
    QString symbol;
    TimeInForce timeInForce;
    TradeAction tradeAction;

    // Optional fields
    std::optional<AdvancedOptions> advancedOptions;
    // TODO Add buying power warning here
    // TODO Add legs here
    std::optional<double> limitPrice;
    // TODO Add OSO here
    std::optional<QString> orderConfirmID;
    std::optional<QString> route;  // Defaults to "Intelligent" for stocks and options
    std::optional<double> stopPrice;
};

class OrderResultItem {
public:
    // Default constructor
    OrderResultItem() = default;
    
    // Constructor taking a QJsonObject
    OrderResultItem(const QJsonObject& jsonObj);

    // Getters
    QString getOrderID() const { return orderID; }
    QString getMessage() const { return message; }
    std::optional<QString> getError() const { return error; }

    // Check if this is an error result
    bool isError() const { return error.has_value(); }

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

private:
    QString orderID;           // Required
    QString message;          // Required
    std::optional<QString> error;  // Optional, presence indicates error state
};

class PlaceOrderResult {
public:
    // Default constructor
    PlaceOrderResult() = default;
    
    // Constructor taking a QJsonObject
    PlaceOrderResult(const QJsonObject& jsonObj);

    // Getters
    const QVector<OrderResultItem>& getOrders() const { return orders; }
    const QVector<OrderResultItem>& getErrors() const { return errors; }

    // Helper methods
    bool hasErrors() const { return !errors.isEmpty(); }
    bool isAllSuccessful() const { return errors.isEmpty(); }

    // Convert to JSON string for debugging/logging
    QString toJsonString() const;

private:
    QVector<OrderResultItem> orders;  // Array of order results (both successful and failed)
    QVector<OrderResultItem> errors;  // Array of error results (from documented "Errors" array)
};

Q_DECLARE_METATYPE(OrderResultItem)
Q_DECLARE_METATYPE(PlaceOrderResult)

#endif // PLACE_ORDER_H
