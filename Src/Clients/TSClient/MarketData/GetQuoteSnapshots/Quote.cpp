#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "Quote.h"

namespace
{
    double parseDoubleField(const QJsonObject& p_json, const char* p_key, double p_default = 0.0)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isString())
        {
            bool ok = false;
            const double parsed = value.toString().toDouble(&ok);
            return ok ? parsed : p_default;
        }
        if (value.isDouble())
        {
            return value.toDouble(p_default);
        }
        return p_default;
    }

    unsigned int parseUIntField(const QJsonObject& p_json, const char* p_key, unsigned int p_default = 0)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isString())
        {
            bool ok = false;
            const unsigned int parsed = value.toString().toUInt(&ok);
            return ok ? parsed : p_default;
        }
        if (value.isDouble())
        {
            return static_cast<unsigned int>(value.toInt(static_cast<int>(p_default)));
        }
        return p_default;
    }

    unsigned long long
    parseULongLongField(const QJsonObject& p_json, const char* p_key, unsigned long long p_default = 0ULL)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isString())
        {
            bool ok = false;
            const unsigned long long parsed = value.toString().toULongLong(&ok);
            return ok ? parsed : p_default;
        }
        if (value.isDouble())
        {
            return static_cast<unsigned long long>(value.toVariant().toULongLong());
        }
        return p_default;
    }

    QDateTime parseDateTimeField(const QJsonObject& p_json, const char* p_key)
    {
        if (!p_json.contains(p_key))
        {
            return {};
        }

        const QString timestamp = p_json.value(p_key).toString();
        if (timestamp.isEmpty())
        {
            return {};
        }

        return QDateTime::fromString(timestamp, Qt::ISODate);
    }

    std::optional<QDateTime> parseOptionalDateTimeField(const QJsonObject& p_json, const char* p_key)
    {
        const QDateTime parsed = parseDateTimeField(p_json, p_key);
        if (!parsed.isValid())
        {
            return std::nullopt;
        }
        return parsed;
    }

    std::optional<double> parseOptionalDoubleField(const QJsonObject& p_json, const char* p_key)
    {
        if (!p_json.contains(p_key))
        {
            return std::nullopt;
        }

        bool ok = false;
        const QJsonValue value = p_json.value(p_key);
        if (value.isString())
        {
            const double parsed = value.toString().toDouble(&ok);
            return ok ? std::optional<double>(parsed) : std::nullopt;
        }
        if (value.isDouble())
        {
            return value.toDouble();
        }

        return std::nullopt;
    }
} // namespace

Quote::Quote(const QJsonObject& json)
{
    fromJson(json);
}

bool Quote::isValid() const
{
    if (m_symbol.isEmpty())
    {
        return false;
    }

    if (!m_error.isEmpty())
    {
        return false;
    }

    if (m_open < 0.0 || m_high < 0.0 || m_low < 0.0 || m_previousClose < 0.0 || m_last < 0.0 || m_ask < 0.0 ||
        m_bid < 0.0 || m_close < 0.0 || m_vwap < 0.0)
    {
        return false;
    }

    if (m_high < m_low)
    {
        return false;
    }

    if (m_high52Week < m_low52Week)
    {
        return false;
    }

    if (!m_marketFlags.isValid())
    {
        return false;
    }

    if (m_minPrice.has_value() && m_minPrice.value() < 0.0)
    {
        return false;
    }

    if (m_maxPrice.has_value() && m_maxPrice.value() < 0.0)
    {
        return false;
    }

    return true;
}

bool Quote::isEmpty() const
{
    return m_symbol.isEmpty() && m_error.isEmpty();
}

std::optional<double> Quote::getBestBid() const
{
    if (m_bid <= 0.0)
    {
        return std::nullopt;
    }

    return m_bid;
}

std::optional<double> Quote::getBestAsk() const
{
    if (m_ask <= 0.0)
    {
        return std::nullopt;
    }

    return m_ask;
}

QJsonObject Quote::toJson() const
{
    QJsonObject json;

    json["Symbol"] = m_symbol;
    json["Error"] = m_error;
    json["Open"] = QString::number(m_open, 'f', 8);
    json["High"] = QString::number(m_high, 'f', 8);
    json["Low"] = QString::number(m_low, 'f', 8);
    json["PreviousClose"] = QString::number(m_previousClose, 'f', 8);
    json["Last"] = QString::number(m_last, 'f', 8);
    json["Ask"] = QString::number(m_ask, 'f', 8);
    json["AskSize"] = QString::number(m_askSize);
    json["Bid"] = QString::number(m_bid, 'f', 8);
    json["BidSize"] = QString::number(m_bidSize);
    json["NetChange"] = QString::number(m_netChange, 'f', 8);
    json["NetChangePct"] = QString::number(m_netChangePct, 'f', 8);
    json["High52Week"] = QString::number(m_high52Week, 'f', 8);
    json["High52WeekTimestamp"] = m_high52WeekTimestamp.toString(Qt::ISODate);
    json["Low52Week"] = QString::number(m_low52Week, 'f', 8);
    json["Low52WeekTimestamp"] = m_low52WeekTimestamp.toString(Qt::ISODate);
    json["Volume"] = QString::number(m_volume);
    json["PreviousVolume"] = QString::number(m_previousVolume);
    json["Close"] = QString::number(m_close, 'f', 8);
    json["DailyOpenInterest"] = QString::number(m_dailyOpenInterest);
    json["TradeTime"] = m_tradeTime.toString(Qt::ISODate);
    json["TickSizeTier"] = QString::number(m_tickSizeTier);
    json["MarketFlags"] = m_marketFlags.toJson();
    json["LastSize"] = QString::number(m_lastSize);
    json["LastVenue"] = m_lastVenue;
    json["VWAP"] = QString::number(m_vwap, 'f', 8);

    if (m_minPrice.has_value())
    {
        json["MinPrice"] = QString::number(m_minPrice.value(), 'f', 8);
    }
    if (m_maxPrice.has_value())
    {
        json["MaxPrice"] = QString::number(m_maxPrice.value(), 'f', 8);
    }
    if (m_firstNoticeDate.has_value())
    {
        json["FirstNoticeDate"] = m_firstNoticeDate.value().toString(Qt::ISODate);
    }
    if (m_lastTradingDate.has_value())
    {
        json["LastTradingDate"] = m_lastTradingDate.value().toString(Qt::ISODate);
    }

    QJsonArray restrictionsArray;
    for (const QString& restriction: m_restrictions)
    {
        restrictionsArray.append(restriction);
    }
    json["Restrictions"] = restrictionsArray;

    return json;
}

QString Quote::toJsonString() const
{
    const QJsonDocument doc(toJson());
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

void Quote::fromJson(const QJsonObject& json)
{
    m_symbol = json.value("Symbol").toString();
    m_error = json.value("Error").toString();

    m_open = parseDoubleField(json, "Open");
    m_high = parseDoubleField(json, "High");
    m_low = parseDoubleField(json, "Low");
    m_previousClose = parseDoubleField(json, "PreviousClose");
    m_last = parseDoubleField(json, "Last");
    m_ask = parseDoubleField(json, "Ask");
    m_bid = parseDoubleField(json, "Bid");
    m_netChange = parseDoubleField(json, "NetChange");
    m_netChangePct = parseDoubleField(json, "NetChangePct");
    m_high52Week = parseDoubleField(json, "High52Week");
    m_low52Week = parseDoubleField(json, "Low52Week");
    m_close = parseDoubleField(json, "Close");
    m_vwap = parseDoubleField(json, "VWAP");

    m_askSize = parseUIntField(json, "AskSize");
    m_bidSize = parseUIntField(json, "BidSize");
    m_dailyOpenInterest = parseUIntField(json, "DailyOpenInterest");
    m_tickSizeTier = parseUIntField(json, "TickSizeTier");
    m_lastSize = parseUIntField(json, "LastSize");

    m_volume = parseULongLongField(json, "Volume");
    m_previousVolume = parseULongLongField(json, "PreviousVolume");

    m_high52WeekTimestamp = parseDateTimeField(json, "High52WeekTimestamp");
    m_low52WeekTimestamp = parseDateTimeField(json, "Low52WeekTimestamp");
    m_tradeTime = parseDateTimeField(json, "TradeTime");

    m_minPrice = parseOptionalDoubleField(json, "MinPrice");
    m_maxPrice = parseOptionalDoubleField(json, "MaxPrice");
    m_firstNoticeDate = parseOptionalDateTimeField(json, "FirstNoticeDate");
    m_lastTradingDate = parseOptionalDateTimeField(json, "LastTradingDate");

    m_lastVenue = json.value("LastVenue").toString();

    m_restrictions.clear();
    if (json.contains("Restrictions") && json.value("Restrictions").isArray())
    {
        const QJsonArray restrictions = json.value("Restrictions").toArray();
        m_restrictions.reserve(restrictions.size());
        for (const QJsonValue& value: restrictions)
        {
            if (value.isString())
            {
                m_restrictions.push_back(value.toString());
            }
        }
    }

    if (json.contains("MarketFlags") && json.value("MarketFlags").isObject())
    {
        m_marketFlags = MarketFlags(json.value("MarketFlags").toObject());
    }
    else
    {
        m_marketFlags = MarketFlags();
    }
}
