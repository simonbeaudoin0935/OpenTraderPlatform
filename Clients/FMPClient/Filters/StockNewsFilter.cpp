#include <QJsonDocument>

#include "StockNewsFilter.h"

StockNewsFilter::StockNewsFilter() {
    // Default constructor leaves all optional fields unset (std::nullopt)
}

// Setters
void StockNewsFilter::setSymbol(const QString &symbol) { 
    this->symbol = symbol; 
}

void StockNewsFilter::setLimit(std::optional<int> limit) { 
    this->limit = limit; 
}

void StockNewsFilter::setPage(std::optional<int> page) { 
    this->page = page; 
}

void StockNewsFilter::setFrom(std::optional<QDate> from) {
    this->from = from;
}

void StockNewsFilter::setTo(std::optional<QDate> to) {
    this->to = to;
}

// Getters
QString StockNewsFilter::getSymbol() const { 
    return symbol; 
}

std::optional<int> StockNewsFilter::getLimit() const { 
    return limit; 
}

std::optional<int> StockNewsFilter::getPage() const { 
    return page; 
}

std::optional<QDate> StockNewsFilter::getFrom() const {
    return from;
}

std::optional<QDate> StockNewsFilter::getTo() const {
    return to;
}

// Helper function to generate URL query
QUrlQuery StockNewsFilter::toUrlQuery() const {
    QUrlQuery query;

    // Required parameter
    if (!symbol.isEmpty()) {
        query.addQueryItem("symbols", symbol);
    }

    // Optional parameters
    if (limit.has_value()) {
        query.addQueryItem("limit", QString::number(limit.value()));
    }

    if (page.has_value()) {
        query.addQueryItem("page", QString::number(page.value()));
    }

    if (from.has_value()) {
        query.addQueryItem("from", from.value().toString("yyyy-MM-dd"));
    }

    if (to.has_value()) {
        query.addQueryItem("to", to.value().toString("yyyy-MM-dd"));
    }

    return query;
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
