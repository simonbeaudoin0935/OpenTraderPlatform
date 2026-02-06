#include "BarReceiver.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY BarReceiverLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "BarReceiver")

BarReceiver::BarReceiver(const QString& symbol, QObject* parent) : QObject(parent), m_symbol(symbol)
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

    m_stream->future().then(
        this,
        [this](std::optional<QString> error)
        {
            if (error.has_value())
            {
                CRITICAL << "Bars stream for" << m_symbol << "finished with error:" << error.value();
            }
            else
            {
                DEBUG << "Bars stream for" << m_symbol << "finished without error";
            }
            WARNING << "Bars stream future finished. This is ok if TSClient::closeStream() is called";
        });
}

BarReceiver::~BarReceiver()
{
    // Note: Stream cleanup is handled by TSClient. Calling closeStream() from destructor
    // can cause race conditions with pending .then() callbacks when using deleteLater().
}

void BarReceiver::onReceivedNewBar(Bar newBar)
{
    emit receivedNewBar(m_symbol, newBar);
}
