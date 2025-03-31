#ifndef STOCK_NEWS_FILTER_H
#define STOCK_NEWS_FILTER_H

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
    StockNewsResult() = default;
    StockNewsResult(const QJsonObject& jsonObj);

    // Getters for each member
    QString getSymbol() const;
    QString getPublishedDate() const;
    QString getPublisher() const;
    QString getTitle() const;
    QString getImage() const;
    QString getSite() const;
    QString getText() const;
    QString getUrl() const;

    QString toJsonString() const;

    void TESTsetPublishedDateToNow() {publishedDate = QDateTime::currentDateTime().toString();};

private:
    // Member variables corresponding to JSON fields
    QString symbol;
    QString publishedDate;
    QString publisher;
    QString title;
    QString image;
    QString site;
    QString text;
    QString url;
};

#endif // STOCK_NEWS_FILTER_H 
