#include <QJsonDocument>
#include <QJsonArray>

#include "MarketDepthQuote.h"

// MarketDepthLevel implementation
MarketDepthLevel::MarketDepthLevel(const QJsonObject& jsonObj)
{
    timeStamp = QDateTime::fromString(jsonObj["TimeStamp"].toString(), Qt::ISODate);
    side = jsonObj["Side"].toString();
    price = jsonObj["Price"].toString();
    size = jsonObj["Size"].toString();
    orderCount = jsonObj["OrderCount"].toInt();
    name = jsonObj["Name"].toString();
}

bool MarketDepthLevel::isValid() const
{
    // Check that all required fields are present and have valid values
    if (!timeStamp.isValid() || side.isEmpty() || price.isEmpty() || size.isEmpty() || name.isEmpty() || orderCount < 0)
    {
        return false;
    }

    // Validate side is either "Bid" or "Ask"
    if (side != "Bid" && side != "Ask")
    {
        return false;
    }

    // Validate price and size are positive numbers
    bool ok;
    double priceValue = price.toDouble(&ok);
    if (!ok || priceValue <= 0)
    {
        return false;
    }

    double sizeValue = size.toDouble(&ok);
    if (!ok || sizeValue <= 0)
    {
        return false;
    }

    return true;
}

QString MarketDepthLevel::toJsonString() const
{
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
MarketDepthQuote::MarketDepthQuote(const QJsonObject& jsonObj)
{
    // Parse Bids array
    if (jsonObj.contains("Bids"))
    {
        const QJsonArray& bidsArray = jsonObj["Bids"].toArray();
        bids.reserve(bidsArray.count());
        for (const auto& bidJson: bidsArray)
        {
            bids.append(MarketDepthLevel(bidJson.toObject()));
        }
    }

    // Parse Asks array
    if (jsonObj.contains("Asks"))
    {
        const QJsonArray& asksArray = jsonObj["Asks"].toArray();
        asks.reserve(asksArray.count());
        for (const auto& askJson: asksArray)
        {
            asks.append(MarketDepthLevel(askJson.toObject()));
        }
    }
}

bool MarketDepthQuote::isValid() const
{
    // Check that we have at least one bid or ask
    if (isEmpty())
    {
        return false;
    }

    // Validate all bid levels
    for (const auto& bid: bids)
    {
        if (!bid.isValid())
        {
            return false;
        }
    }

    // Validate all ask levels
    for (const auto& ask: asks)
    {
        if (!ask.isValid())
        {
            return false;
        }
    }

    return true;
}

// Market is locked when best bid equals best ask
bool MarketDepthQuote::isLocked() const
{
    // If we don't have both bids and asks, it can't be locked
    if (bids.isEmpty() || asks.isEmpty())
    {
        return false;
    }

    // Get best bid and ask prices
    double bestBid = bids.first().getPrice().toDouble();
    double bestAsk = asks.first().getPrice().toDouble();

    return qFuzzyCompare(bestBid, bestAsk);
}

// Market is crossed when best bid is higher than best ask
bool MarketDepthQuote::isCrossed() const
{
    // If we don't have both bids and asks, it can't be crossed
    if (bids.isEmpty() || asks.isEmpty())
    {
        return false;
    }

    // Get best bid and ask prices
    double bestBid = bids.first().getPrice().toDouble();
    double bestAsk = asks.first().getPrice().toDouble();

    return bestBid > bestAsk;
}

QString MarketDepthQuote::toJsonString() const
{
    QJsonObject jsonObj;

    // Build Bids array
    QJsonArray bidsArray;
    for (const auto& bid: bids)
    {
        bidsArray.append(QJsonDocument::fromJson(bid.toJsonString().toUtf8()).object());
    }
    jsonObj["Bids"] = bidsArray;

    // Build Asks array
    QJsonArray asksArray;
    for (const auto& ask: asks)
    {
        asksArray.append(QJsonDocument::fromJson(ask.toJsonString().toUtf8()).object());
    }
    jsonObj["Asks"] = asksArray;

    QJsonDocument doc(jsonObj);
    return QString(doc.toJson(QJsonDocument::Indented));
}
