#include "StreamMarketDepthQuote.h"
#include "TSClient.h"
#include "Logging.h"
#include "CONSTANTS.h"
#include "Assume.h"
#include "MockNetworkReply.h"

#include <QTimer>

#define LOGGING_CATEGORY StreamLog

// Initialize static atomic counter
std::atomic<size_t> StreamMarketDepthQuote::s_numberOfMarketDepthStreams{0};

StreamMarketDepthQuote::StreamMarketDepthQuote(const QString& symbol, QNetworkReply* reply, QObject* parent)
    : StreamMarketData(reply, parent), m_symbol(symbol)
{
    const QString suffix = qobject_cast<MockNetworkReply*>(reply) ? QStringLiteral("::mock") : QStringLiteral("::live");
    this->setObjectName("Stream::MarketDepthQuote::" + symbol + suffix);

    // Increment atomic counter
    s_numberOfMarketDepthStreams++;

    DEBUG << "Stream created - Total market depth streams:" << s_numberOfMarketDepthStreams.load();
}

StreamMarketDepthQuote::~StreamMarketDepthQuote()
{
    DEBUG << "Stream destroyed for" << m_symbol;

    // Decrement atomic counter
    s_numberOfMarketDepthStreams--;

    // If there are queued requests, schedule processing after TCP close delay
    if (!TSClient::getInstance()->isMarketDepthQueueEmpty())
    {
        DEBUG << "Queue not empty - scheduling processing after" << MarketDepthConstants::QUEUE_PROCESS_DELAY_MS
              << "ms delay";

        QTimer::singleShot(MarketDepthConstants::QUEUE_PROCESS_DELAY_MS,
                           TSClient::getInstance(),
                           &TSClient::processMarketDepthQueue);
    }

    DEBUG << "Total market depth streams after destruction:" << s_numberOfMarketDepthStreams.load();
}

bool StreamMarketDepthQuote::canOpenStream()
{
    return s_numberOfMarketDepthStreams.load() < MarketDepthConstants::MAX_CONCURRENT_STREAMS;
}

void StreamMarketDepthQuote::processJsonObject(const QJsonObject& jsonObj)
{
    MarketDepthQuote quote(jsonObj);

    if (!quote.isValid()) [[unlikely]]
    {

        WARNING << "Market Depth Quote invalid. Received data : "
                << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));

        return;
    }

    if (quote.isLocked())
    {
        DEBUG << " Quote is locked";
    }
    else if (quote.isCrossed())
    {
        DEBUG << " Quote is crossed";
    }

    emit newMarketDepthQuoteReceived(quote);
}