#ifndef STOCK_NEWS_FILTER_H
#define STOCK_NEWS_FILTER_H

#include <string>
#include <optional>
#include <QString>
#include <QJsonObject>

class StockNewsFilter {
public:
    // Constructor
    StockNewsFilter();

    // Setters for each filter criterion
    void setSymbol(std::optional<std::string> value);
    void setLimit(std::optional<int> value);
    void setOffset(std::optional<int> value);

    // Getters for each filter criterion
    std::optional<std::string> getSymbol() const;
    std::optional<int> getLimit() const;
    std::optional<int> getOffset() const;

    // Helper function to generate URL parameters
    QString getURLParameters() const;

private:
    // Member variables using std::optional for optional values
    std::optional<std::string> symbol;
    std::optional<int> limit;
    std::optional<int> offset;
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