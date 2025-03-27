#include "stocknewsfilter.h"
#include <QJsonDocument>

StockNewsFilter::StockNewsFilter() {
    // Default constructor leaves all fields unset (std::nullopt)
}

// Setters
void StockNewsFilter::setSymbol(std::optional<std::string> value) { symbol = value; }
void StockNewsFilter::setLimit(std::optional<int> value) { limit = value; }
void StockNewsFilter::setOffset(std::optional<int> value) { offset = value; }

// Getters
std::optional<std::string> StockNewsFilter::getSymbol() const { return symbol; }
std::optional<int> StockNewsFilter::getLimit() const { return limit; }
std::optional<int> StockNewsFilter::getOffset() const { return offset; }

// Helper function to generate URL parameters
QString StockNewsFilter::getURLParameters() const {
    QString params;
    bool firstParam = true;

    auto appendParam = [&](const QString& key, const QString& value) {
        if (!firstParam) params.append('&');
        params.append(key + '=' + value);
        firstParam = false;
    };

    if (symbol.has_value()) appendParam("symbol", QString::fromStdString(symbol.value()));
    if (limit.has_value()) appendParam("limit", QString::number(limit.value()));
    if (offset.has_value()) appendParam("offset", QString::number(offset.value()));

    return params;
}

// Implementation of StockNewsResult
StockNewsResult::StockNewsResult(const QJsonObject& jsonObj) {
    symbol = jsonObj["symbol"].toString();
    title = jsonObj["title"].toString();
    date = jsonObj["date"].toString();
    text = jsonObj["text"].toString();
    url = jsonObj["url"].toString();
    site = jsonObj["site"].toString();
    image = jsonObj["image"].toString();
    source = jsonObj["source"].toString();
}

// Getters for StockNewsResult
QString StockNewsResult::getSymbol() const { return symbol; }
QString StockNewsResult::getTitle() const { return title; }
QString StockNewsResult::getDate() const { return date; }
QString StockNewsResult::getText() const { return text; }
QString StockNewsResult::getUrl() const { return url; }
QString StockNewsResult::getSite() const { return site; }
QString StockNewsResult::getImage() const { return image; }
QString StockNewsResult::getSource() const { return source; }

QString StockNewsResult::toJsonString() const {
    // Construct a QJsonObject from the member variables
    QJsonObject jsonObj;
    jsonObj["symbol"] = symbol;
    jsonObj["title"] = title;
    jsonObj["date"] = date;
    jsonObj["text"] = text;
    jsonObj["url"] = url;
    jsonObj["site"] = site;
    jsonObj["image"] = image;
    jsonObj["source"] = source;

    // Convert to formatted JSON string
    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented)); // Indented for readability
} 