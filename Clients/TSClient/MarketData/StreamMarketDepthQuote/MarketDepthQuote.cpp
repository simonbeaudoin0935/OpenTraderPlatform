#include "MarketDepthQuote.h"
#include <QJsonDocument>
#include <QJsonArray>

// MarketDepthLevel implementation
MarketDepthLevel::MarketDepthLevel(const QJsonObject& jsonObj) {
    timeStamp = QDateTime::fromString(jsonObj["TimeStamp"].toString(), Qt::ISODate);
    side = jsonObj["Side"].toString();
    price = jsonObj["Price"].toString();
    size = jsonObj["Size"].toString();
    orderCount = jsonObj["OrderCount"].toInt();
    name = jsonObj["Name"].toString();
}

QString MarketDepthLevel::toJsonString() const {
    QJsonObject jsonObj;
    jsonObj["TimeStamp"] = timeStamp.toString(Qt::ISODate);
    jsonObj["Side"] = side;
    jsonObj["Price"] = price;
    jsonObj["Size"] = size;
    jsonObj["OrderCount"] = orderCount;
    jsonObj["Name"] = name;
    
    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}

// MarketDepthQuote implementation
MarketDepthQuote::MarketDepthQuote(const QJsonObject& jsonObj) {
    // Parse Bids array
    if (jsonObj.contains("Bids")) {
        const QJsonArray& bidsArray = jsonObj["Bids"].toArray();
        bids.reserve(bidsArray.count());
        for (const auto& bidJson : bidsArray) {
            bids.append(MarketDepthLevel(bidJson.toObject()));
        }
    }
    
    // Parse Asks array
    if (jsonObj.contains("Asks")) {
        const QJsonArray& asksArray = jsonObj["Asks"].toArray();
        asks.reserve(asksArray.count());
        for (const auto& askJson : asksArray) {
            asks.append(MarketDepthLevel(askJson.toObject()));
        }
    }
}

QString MarketDepthQuote::toJsonString() const {
    QJsonObject jsonObj;
    
    // Build Bids array
    QJsonArray bidsArray;
    for (const auto& bid : bids) {
        bidsArray.append(QJsonDocument::fromJson(bid.toJsonString().toUtf8()).object());
    }
    jsonObj["Bids"] = bidsArray;
    
    // Build Asks array
    QJsonArray asksArray;
    for (const auto& ask : asks) {
        asksArray.append(QJsonDocument::fromJson(ask.toJsonString().toUtf8()).object());
    }
    jsonObj["Asks"] = asksArray;
    
    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}
