#include "StreamMarketDepthAggregate.h"

#include "CONSTANTS.h"
#include "Logging.h"
#include "TSClient.h"

#include <QJsonArray>
#include <QTimer>

#define LOGGING_CATEGORY StreamLog

std::atomic<size_t> StreamMarketDepthAggregate::s_numberOfMarketDepthAggregateStreams{0};

namespace
{
    double parseDoubleValue(const QJsonValue& value)
    {
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

    int parseIntValue(const QJsonValue& value)
    {
        if (value.isDouble())
        {
            return value.toInt();
        }
        if (value.isString())
        {
            bool ok = false;
            const int parsed = value.toString().toInt(&ok);
            return ok ? parsed : 0;
        }
        return 0;
    }

    double parseDouble(const QJsonObject& p_json, const char* p_key)
    {
        return parseDoubleValue(p_json.value(p_key));
    }

    int parseInt(const QJsonObject& p_json, const char* p_key)
    {
        return parseIntValue(p_json.value(p_key));
    }

    QDateTime parseTimestampValue(const QJsonValue& value)
    {
        const QString raw = value.toString();
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

    QDateTime parseTimestamp(const QJsonObject& p_json)
    {
        QDateTime parsed = parseTimestampValue(p_json.value("LatestTime"));
        if (parsed.isValid())
        {
            return parsed;
        }

        parsed = parseTimestampValue(p_json.value("EarliestTime"));
        if (parsed.isValid())
        {
            return parsed;
        }

        return parseTimestampValue(p_json.value("TimeStamp"));
    }

    int parseSize(const QJsonObject& p_json)
    {
        // Aggregate stream rows expose TotalSize/TotalOrderCount.
        // Keep quote-style fallback keys so replay fixtures and older payloads still parse.
        if (p_json.contains("TotalSize"))
        {
            return parseInt(p_json, "TotalSize");
        }
        return parseInt(p_json, "Size");
    }

    int parseOrderCount(const QJsonObject& p_json)
    {
        if (p_json.contains("TotalOrderCount"))
        {
            return parseInt(p_json, "TotalOrderCount");
        }
        return parseInt(p_json, "OrderCount");
    }

    void fillSideRows(const QJsonArray& rows, std::array<Level2Row, 10>& out)
    {
        size_t i = 0;
        for (const QJsonValue& rowValue: rows)
        {
            if (i >= out.size() || !rowValue.isObject())
            {
                break;
            }

            const QJsonObject row = rowValue.toObject();
            out[i].m_price = parseDouble(row, "Price");
            out[i].m_size = parseSize(row);
            out[i].m_orderCount = parseOrderCount(row);
            ++i;
        }

        for (; i < out.size(); ++i)
        {
            out[i] = Level2Row{};
        }
    }
} // namespace

StreamMarketDepthAggregate::StreamMarketDepthAggregate(const QString& symbol, QNetworkReply* reply, QObject* parent)
    : StreamMarketData(reply, parent), m_symbol(symbol)
{
    setObjectName("Stream::Level2::" + symbol);
    ++s_numberOfMarketDepthAggregateStreams;
    DEBUG << "Stream created - Total market depth streams:" << s_numberOfMarketDepthAggregateStreams.load();
}

StreamMarketDepthAggregate::~StreamMarketDepthAggregate()
{
    if (s_numberOfMarketDepthAggregateStreams > 0)
    {
        --s_numberOfMarketDepthAggregateStreams;
    }

    if (!TSClient::getInstance()->isMarketDepthQueueEmpty())
    {
        QTimer::singleShot(MarketDepthConstants::QUEUE_PROCESS_DELAY_MS,
                           TSClient::getInstance(),
                           &TSClient::processMarketDepthQueue);
    }

    DEBUG << "Stream destroyed - Total market depth streams:" << s_numberOfMarketDepthAggregateStreams.load();
}

bool StreamMarketDepthAggregate::canOpenStream()
{
    return s_numberOfMarketDepthAggregateStreams.load() < MarketDepthConstants::MAX_CONCURRENT_STREAMS;
}

void StreamMarketDepthAggregate::processJsonObject(const QJsonObject& jsonObj)
{
    const QJsonArray bids = jsonObj.value("Bids").toArray();
    const QJsonArray asks = jsonObj.value("Asks").toArray();

    if (bids.isEmpty() && asks.isEmpty())
    {
        WARNING << "Ignoring empty market-depth payload for" << m_symbol;
        return;
    }

    Level2 level2;
    level2.m_symbol = m_symbol;

    QDateTime ts;
    if (!bids.isEmpty() && bids.first().isObject())
    {
        ts = parseTimestamp(bids.first().toObject());
    }
    if ((!ts.isValid()) && !asks.isEmpty() && asks.first().isObject())
    {
        ts = parseTimestamp(asks.first().toObject());
    }
    if (!ts.isValid())
    {
        ts = QDateTime::currentDateTime().toTimeZone(TradingHours::MARKET_TIMEZONE);
    }
    level2.m_timeStamp = ts;

    fillSideRows(bids, level2.m_bids);
    fillSideRows(asks, level2.m_asks);

    emit newLevel2Received(level2);
}
