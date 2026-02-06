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

    m_stream->future().then(this,
                            [this](std::optional<std::pair<Stream::StreamError, QString>> error)
                            {
                                if (error.has_value())
                                {
                                    auto [errorType, message] = error.value();
                                    CRITICAL << "Bars stream for" << m_symbol << "finished with error:" << message;
                                    m_stream = nullptr;
                                    QTimer::singleShot(300, this, &BarReceiver::createBarStream);
                                }
                                else
                                {
                                    DEBUG << "Bars stream for" << m_symbol << "intentionally closed";
                                }
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
