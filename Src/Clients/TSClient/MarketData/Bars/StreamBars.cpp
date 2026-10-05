#include "StreamBars.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "Logging.h"
#include "MarketData/Bars/TSBarTimestampNormalizer.h"
#include "Stream/MockNetworkReply.h"

#define LOGGING_CATEGORY StreamLog

size_t StreamBars::s_numberOfBarsStreams = 0;

namespace
{
    double parseDouble(const QJsonObject& p_json, const char* p_key)
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

    quint64 parseUInt64(const QJsonObject& p_json, const char* p_key)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isDouble())
        {
            return static_cast<quint64>(qMax<qint64>(0, value.toVariant().toLongLong()));
        }
        if (value.isString())
        {
            bool ok = false;
            const quint64 parsed = value.toString().toULongLong(&ok);
            return ok ? parsed : 0ULL;
        }
        return 0ULL;
    }

    QDateTime parseTimestamp(const QJsonObject& p_json, const int p_minuteIntervalSeconds)
    {
        return TSBarTimestampNormalizer::normalizeToCanonicalBarTimestamp(p_json.value("TimeStamp").toString(),
                                                                          p_minuteIntervalSeconds);
    }

    Bar::BarStatus parseStatus(const QJsonObject& p_json)
    {
        const QString status = p_json.value("BarStatus").toString();
        if (status.isEmpty())
        {
            return Bar::BarStatus::Closed;
        }

        if (status == "Open")
        {
            return Bar::BarStatus::Open;
        }

        if (status == "Closed")
        {
            return Bar::BarStatus::Closed;
        }

        if (status == "Null")
        {
            return Bar::BarStatus::Null;
        }

        return Bar::BarStatus::Closed;
    }
} // namespace

StreamBars::StreamBars(const QString& symbol, QNetworkReply* reply, QObject* parent, const int p_minuteIntervalSeconds)
    : StreamMarketData(reply, parent), m_symbol(symbol), m_minuteIntervalSeconds(p_minuteIntervalSeconds)
{
    const QString suffix = qobject_cast<MockNetworkReply*>(reply) ? QStringLiteral("::mock") : QStringLiteral("::live");
    setObjectName("Stream::Bars::" + symbol + suffix);

    ++s_numberOfBarsStreams;
    DEBUG << "Stream created - Total bar streams:" << s_numberOfBarsStreams;
}

StreamBars::~StreamBars()
{
    if (s_numberOfBarsStreams > 0)
    {
        --s_numberOfBarsStreams;
    }

    DEBUG << "Stream destroyed - Total bar streams:" << s_numberOfBarsStreams;
}

void StreamBars::processJsonObject(const QJsonObject& jsonObj)
{
    const QDateTime ts = parseTimestamp(jsonObj, m_minuteIntervalSeconds);
    if (!ts.isValid())
    {
        WARNING << "Ignoring bar with invalid timestamp:"
                << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
        return;
    }

    Bar bar(ts,
            static_cast<float>(parseDouble(jsonObj, "Open")),
            static_cast<float>(parseDouble(jsonObj, "High")),
            static_cast<float>(parseDouble(jsonObj, "Low")),
            static_cast<float>(parseDouble(jsonObj, "Close")),
            parseUInt64(jsonObj, "TotalVolume"),
            parseStatus(jsonObj));

    if (!bar.isValid())
    {
        WARNING << "Malformed bar payload:" << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
        return;
    }

    emit newBarReceived(bar);
}
