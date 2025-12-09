#include <QJsonObject>
#include <QJsonValue>
#include <QDateTime>
#include <QJsonDocument>

#include "Quote.h"

Quote::Quote()
{
}

Quote::Quote(const QJsonObject& json)
{
    fromJson(json);
}

bool Quote::isValid() const
{
    // Check if symbol is not empty
    if (m_symbol.isEmpty()) {
        return false;
    }

    // Check if prices are non-negative
    if (m_open < 0 || m_high < 0 || m_low < 0 || m_previousClose < 0 || 
        m_last < 0 || m_ask < 0 || m_bid < 0 || m_close < 0) {
        return false;
    }

    // Check if high is greater than or equal to low
    if (m_high < m_low) {
        return false;
    }

    // Check if timestamps are valid
    if (!m_tradeTime.isValid() || !m_high52WeekTimestamp.isValid() || !m_low52WeekTimestamp.isValid()) {
        return false;
    }

    // Check if 52-week high is greater than or equal to 52-week low
    if (m_high52Week < m_low52Week) {
        return false;
    }

    return true;
}

QJsonObject Quote::toJson() const
{
    QJsonObject json;
    json["Symbol"] = m_symbol;
    json["Open"] = QString::number(m_open, 'f', 2);
    json["High"] = QString::number(m_high, 'f', 2);
    json["Low"] = QString::number(m_low, 'f', 2);
    json["PreviousClose"] = QString::number(m_previousClose, 'f', 2);
    json["Last"] = QString::number(m_last, 'f', 2);
    json["Ask"] = QString::number(m_ask, 'f', 2);
    json["AskSize"] = QString::number(m_askSize);
    json["Bid"] = QString::number(m_bid, 'f', 2);
    json["BidSize"] = QString::number(m_bidSize);
    json["NetChange"] = QString::number(m_netChange, 'f', 2);
    json["NetChangePct"] = QString::number(m_netChangePct, 'f', 2);
    json["High52Week"] = QString::number(m_high52Week, 'f', 2);
    json["High52WeekTimestamp"] = m_high52WeekTimestamp.toString(Qt::ISODate);
    json["Low52Week"] = QString::number(m_low52Week, 'f', 2);
    json["Low52WeekTimestamp"] = m_low52WeekTimestamp.toString(Qt::ISODate);
    json["Volume"] = QString::number(m_volume);
    json["PreviousVolume"] = QString::number(m_previousVolume);
    json["Close"] = QString::number(m_close, 'f', 2);
    json["DailyOpenInterest"] = QString::number(m_dailyOpenInterest);
    json["TradeTime"] = m_tradeTime.toString(Qt::ISODate);
    json["TickSizeTier"] = QString::number(m_tickSizeTier);
    
    QJsonObject marketFlags;
    marketFlags["IsDelayed"] = m_isDelayed;
    marketFlags["IsHardToBorrow"] = m_isHardToBorrow;
    marketFlags["IsBats"] = m_isBats;
    marketFlags["IsHalted"] = m_isHalted;
    json["MarketFlags"] = marketFlags;
    
    json["LastSize"] = QString::number(m_lastSize);
    json["LastVenue"] = m_lastVenue;
    json["VWAP"] = QString::number(m_vwap, 'f', 2);
    
    return json;
}

QString Quote::toJsonString() const
{
    QJsonDocument doc(toJson());
    return doc.toJson(QJsonDocument::Indented);
}

void Quote::fromJson(const QJsonObject& json)
{
    m_symbol = json["Symbol"].toString();
    m_open = json["Open"].toString().toDouble();
    m_high = json["High"].toString().toDouble();
    m_low = json["Low"].toString().toDouble();
    m_previousClose = json["PreviousClose"].toString().toDouble();
    m_last = json["Last"].toString().toDouble();
    m_ask = json["Ask"].toString().toDouble();
    m_askSize = json["AskSize"].toString().toUInt();
    m_bid = json["Bid"].toString().toDouble();
    m_bidSize = json["BidSize"].toString().toUInt();
    m_netChange = json["NetChange"].toString().toDouble();
    m_netChangePct = json["NetChangePct"].toString().toDouble();
    m_high52Week = json["High52Week"].toString().toDouble();
    m_high52WeekTimestamp = QDateTime::fromString(json["High52WeekTimestamp"].toString(), Qt::ISODate);
    m_low52Week = json["Low52Week"].toString().toDouble();
    m_low52WeekTimestamp = QDateTime::fromString(json["Low52WeekTimestamp"].toString(), Qt::ISODate);
    m_volume = json["Volume"].toString().toULongLong();
    m_previousVolume = json["PreviousVolume"].toString().toULongLong();
    m_close = json["Close"].toString().toDouble();
    m_dailyOpenInterest = json["DailyOpenInterest"].toString().toUInt();
    m_tradeTime = QDateTime::fromString(json["TradeTime"].toString(), Qt::ISODate);
    m_tickSizeTier = json["TickSizeTier"].toString().toUInt();
    
    QJsonObject marketFlags = json["MarketFlags"].toObject();
    m_isDelayed = marketFlags["IsDelayed"].toBool();
    m_isHardToBorrow = marketFlags["IsHardToBorrow"].toBool();
    m_isBats = marketFlags["IsBats"].toBool();
    m_isHalted = marketFlags["IsHalted"].toBool();
    
    m_lastSize = json["LastSize"].toString().toUInt();
    m_lastVenue = json["LastVenue"].toString();
    m_vwap = json["VWAP"].toString().toDouble();
}
