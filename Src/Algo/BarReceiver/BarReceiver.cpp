#include "BarReceiver.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY BarReceiverLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "BarReceiver")

BarReceiver::BarReceiver(const QString& symbol, QObject* parent) : StreamReceiver(parent), m_symbol(symbol)
{
    setObjectName("BarReceiver::" + symbol);

    createBarStream();
}

void BarReceiver::createBarStream()
{
    DEBUG << "Starting bars stream for" << m_symbol;

    OBJ_ASSUME_EQUAL(m_stream, nullptr);

    m_stream = TSClient::getInstance()->openStreamBars(m_symbol,
                                                       1, /* Interval: 1 bar */
                                                       Bar::BarUnit::Minute,
                                                       1, /* Barsback: 1 bars */
                                                       Bar::BarSessionTemplate::USEQ24Hour);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamBars::newBarReceived, this, &BarReceiver::onReceivedNewBar);

    // Capture symbol by value - no context object needed since this is just logging
    m_stream->future().then(
        [symbol = m_symbol](std::optional<std::pair<Stream::StreamError, QString>> error)
        {
            if (error.has_value())
            {
                auto [errorType, message] = error.value();
                qCCritical(BarReceiverLog) << "Bars stream for" << symbol << "finished with error:" << message;
            }
            else
            {
                qCDebug(BarReceiverLog) << "Bars stream for" << symbol << "finished without error";
            }
            qCWarning(BarReceiverLog) << "Bars stream future finished. This is ok if TSClient::closeStream() is called";
        });
}

BarReceiver::~BarReceiver()
{
    if (m_stream != nullptr)
    {
        TSClient::getInstance()->closeStream(m_stream);
    }
}

void BarReceiver::onReceivedNewBar(Bar newBar)
{
    emit receivedNewBar(m_symbol, newBar);
}
