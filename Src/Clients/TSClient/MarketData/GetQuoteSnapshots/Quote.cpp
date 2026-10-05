#include "Quote.h"

#include "CONSTANTS.h"

namespace
{
    double parseDoubleField(const QJsonObject& p_json, const char* p_key)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isDouble())
        {
            return value.toDouble();
        }
        if (value.isString())
        {
            bool ok = false;
            const double parsed = value.toString().toDouble(&ok);
            return ok ? parsed : 0.0;
        }
        return 0.0;
    }

    unsigned int parseUIntField(const QJsonObject& p_json, const char* p_key)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isDouble())
        {
            return static_cast<unsigned int>(qMax(0, value.toInt()));
        }
        if (value.isString())
        {
            bool ok = false;
            const unsigned int parsed = value.toString().toUInt(&ok);
            return ok ? parsed : 0u;
        }
        return 0u;
    }

    QDateTime parseDateTime(const QJsonObject& p_json, const char* p_key)
    {
        const QString raw = p_json.value(p_key).toString();
        if (raw.isEmpty())
        {
            return {};
        }

        QDateTime parsed = QDateTime::fromString(raw, Qt::ISODate);
        if (!parsed.isValid())
        {
            return {};
        }

        if (parsed.timeZone().isValid())
        {
            return parsed.toTimeZone(TradingHours::MARKET_TIMEZONE);
        }

        parsed.setTimeZone(TradingHours::MARKET_TIMEZONE);
        return parsed;
    }
} // namespace

void Quote::fromJson(const QJsonObject& json)
{
    m_symbol = json.value("Symbol").toString().trimmed().toUpper();
    m_error = json.value("Error").toString();

    m_last = parseDoubleField(json, "Last");
    m_lastSize = parseUIntField(json, "LastSize");

    m_bid = parseDoubleField(json, "Bid");
    m_bidSize = parseUIntField(json, "BidSize");
    m_ask = parseDoubleField(json, "Ask");
    m_askSize = parseUIntField(json, "AskSize");

    m_tradeTime = parseDateTime(json, "TradeTime");

    if (json.contains("MarketFlags") && json.value("MarketFlags").isObject())
    {
        m_marketFlags = MarketFlags(json.value("MarketFlags").toObject());
    }
    else
    {
        m_marketFlags = MarketFlags(json);
    }
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

    if ((m_bid > 0.0 && m_ask > 0.0) && m_bid > m_ask)
    {
        return false;
    }

    return true;
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
