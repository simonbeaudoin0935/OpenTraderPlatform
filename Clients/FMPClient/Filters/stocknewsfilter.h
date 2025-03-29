#ifndef STOCK_NEWS_FILTER_H
#define STOCK_NEWS_FILTER_H

#include <string>
#include <optional>
#include <QString>
#include <QJsonObject>
#include <QUrlQuery>
#include <QDate>

class StockNewsFilter {
public:
    // Constructor
    StockNewsFilter();

    // Setters for each filter criterion
    void setSymbol(const QString &symbol);  // Required parameter
    void setLimit(std::optional<int> limit);  // Optional parameter
    void setPage(std::optional<int> page);   // Optional parameter
    void setFrom(std::optional<QDate> from); // Optional parameter
    void setTo(std::optional<QDate> to);     // Optional parameter

    // Getters for each filter criterion
    QString getSymbol() const;               // Always returns a value
    std::optional<int> getLimit() const;     // May return std::nullopt
    std::optional<int> getPage() const;      // May return std::nullopt
    std::optional<QDate> getFrom() const;    // May return std::nullopt
    std::optional<QDate> getTo() const;      // May return std::nullopt

    // Helper function to generate URL query
    QUrlQuery toUrlQuery() const;

    // Implicit conversion operator
    operator QUrlQuery() const { return toUrlQuery(); }

private:
    // Required parameter
    QString symbol;

    // Optional parameters using std::optional
    std::optional<int> limit;
    std::optional<int> page;
    std::optional<QDate> from;
    std::optional<QDate> to;
};

// New class for API result
class StockNewsResult {
public:
    // Constructor taking a QJsonObject
    StockNewsResult(const QJsonObject& jsonObj);

    // Getters for each member
    QString getSymbol() const;
    QString getTitle() const;
    QString getDate() const;
    QString getText() const;
    QString getUrl() const;
    QString getSite() const;
    QString getImage() const;
    QString getSource() const;

    QString toJsonString() const;
private:
    // Member variables corresponding to JSON fields
    QString symbol;
    QString title;
    QString date;
    QString text;
    QString url;
    QString site;
    QString image;
    QString source;
};

#endif // STOCK_NEWS_FILTER_H 