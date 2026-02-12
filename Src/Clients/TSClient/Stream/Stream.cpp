#include <QJsonObject>
#include <QMetaEnum>
#include <QNetworkReply>
#include <QCoreApplication>

#include "Stream.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY StreamLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "Stream")

size_t Stream::s_numberOfStream = 0;
bool Stream::s_isShuttingDown = false;


Stream::Stream(QNetworkReply* reply, QObject* parent) : QObject(parent), m_networkReply(reply)
{
    Q_CHECK_PTR(reply);
    Q_CHECK_PTR(parent);

    s_numberOfStream++;

    m_networkReply->setParent(this);

    QMetaObject::Connection c;
    c = connect(m_networkReply, &QNetworkReply::readyRead, this, &Stream::onReplyReadyRead);
    OBJ_ASSUME_TRUE(c);
    c = connect(m_networkReply, &QNetworkReply::finished, this, &Stream::onReplyFinished);
    OBJ_ASSUME_TRUE(c);

    m_heartbeatTimer.setSingleShot(true);
    c = connect(&m_heartbeatTimer, &QTimer::timeout, this, &Stream::onHeartbeatTimerTimeout);
    OBJ_ASSUME_TRUE(c);

    // Creating a Stream implies that we expect data to start flowing in because the QNetworkReply
    // that was passed has been obtained after the HTTP x request, so we start the heartbeat timer now
    m_heartbeatTimer.start(m_heartbeatTimeoutMS);
}

Stream::~Stream()
{
    DEBUG << "Stream destroyed ";

    // Stream must be destroyed in the same thread where it was created
    OBJ_ASSUME_EQUAL(QThread::currentThread(), this->thread());

    s_numberOfStream--;

    // Disconnect finished signal BEFORE aborting to prevent onReplyFinished() from firing.
    // The intentional close path emits streamClosed(Closed) below instead.
    disconnect(m_networkReply, &QNetworkReply::finished, this, &Stream::onReplyFinished);
    m_networkReply->abort();
    m_networkReply->deleteLater();

    m_heartbeatTimer.stop();

    if (!s_isShuttingDown)
    {
        emit streamClosed(StreamError::Closed, QStringLiteral("Stream intentionally closed"));
    }
}

void Stream::onReplyFinished()
{
    WARNING << "received the signal finished()";

    Q_CHECK_PTR(m_networkReply);

    // Determine the error type based on what triggered this:
    // 1) Heartbeat timeout (m_receivedTimeoutError == true)
    // 2) JSON error received from server (m_streamError has been set by intermediate class)
    // 3) Network error reported by QNetworkReply (timeout, disconnection, etc)
    StreamError errorType = StreamError::NoError; // Default to no error
    if (m_streamError.has_value())
    {
        errorType = m_streamError.value();
    }

    QNetworkReply::NetworkError error = m_networkReply->error();
    QMetaEnum metaEnum = QMetaEnum::fromType<QNetworkReply::NetworkError>();
    QString errorName = metaEnum.valueToKey(error);

    QString description = "Timeout: " + (m_receivedTimeoutError ? QString("true") : QString("false")) +
                          " JSON Error: " + m_jsonErrorString + ". Network error: " + errorName + " (" +
                          m_networkReply->errorString() + ")";

    DEBUG << "Stream finished with error:" << description;

    m_heartbeatTimer.stop();

    emit streamClosed(errorType, description);

    // The destructor will be called later, but we disconnect finished() so it won't emit again
    disconnect(m_networkReply, &QNetworkReply::finished, this, &Stream::onReplyFinished);
    this->deleteLater();
}

void Stream::onHeartbeatTimerTimeout()
{
    CRITICAL << "Heartbeat timer timeout occurred";

    m_receivedTimeoutError = true;
    m_heartbeatTimer.stop();

    // The fact of aborting the network reply will then trigger the finished() slot
    // which will take care of further destruction and promise finalization
    m_networkReply->abort();
}


void Stream::onReplyReadyRead()
{
    const QByteArray rawData = m_networkReply->readAll();

    // Something is very wrong if we get a readyRead signal but no data
    OBJ_ASSUME_GT(rawData.size(), 0);

    emit newAmountOfDataReceived(rawData.size());

    processRawData(rawData);
}

/*
 * Process raw data received from the network reply.
 * Data is expected to be newline delimited JSON objects.
 * Process as many complete JSON objects as there are in the accumulation buffer possible.
 */
void Stream::processRawData(const QByteArray& rawData)
{
    // Kick the heartbeat watchdog unless intentionally paused (e.g., replay paused)
    if (!m_heartbeatPaused)
    {
        m_heartbeatTimer.start(m_heartbeatTimeoutMS);
    }

    m_accumulatedData.append(rawData);

    emit receivedNewRawData(rawData);

    // Decode and parse one JSON document at a time, as long as we have complete objects (newline delimited)
    for (int delimiterPos = m_accumulatedData.indexOf('\n'); delimiterPos != -1;
         delimiterPos = m_accumulatedData.indexOf('\n'))
    {
        // TODO optimize to avoid copy
        const QByteArray jsonData = m_accumulatedData.left(delimiterPos + 1);
        m_accumulatedData.remove(0,
                                 delimiterPos + 1); // Remove extracted data from buffer

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            CRITICAL << "Failed to parse JSON:" << parseError.errorString();
            DEBUG << "Raw data : " << jsonData;

            // Skip this malformed object and continue processing further data, hopefully we can resync on good objects
            continue;
        }

        processJsonDoc(doc);
    }
}

/*
 * Process a JSON document extracted from the stream.
 * Handles heartbeat objects centrally, delegates error/status handling to intermediate classes
 * (StreamMarketData, StreamBrokerage), and delegates normal data to concrete derived classes.
 */
void Stream::processJsonDoc(const QJsonDocument& doc)
{
    const QJsonObject jsonObj = doc.object();

    // All types of stream have in common this heartbeat object
    if (jsonObj.contains("Heartbeat"))
    {

        // Here, we just test that the heartbeat object is well formed, we don't actually care about the timestamp
        // We will kick the watchdog timer anyway
        if (jsonObj.contains("Timestamp") == false) [[unlikely]]
        {
            CRITICAL << " received malformed heartbeat object without Timestamp field";
        }

        if (!m_heartbeatPaused)
        {
            m_heartbeatTimer.start(m_heartbeatTimeoutMS);
        }
        return;
    }

    // Delegate error/status detection to intermediate classes (StreamMarketData or StreamBrokerage)
    if (handleErrorOrStatus(jsonObj))
    {
        return;
    }

    // Happy path: delegate normal data processing to concrete derived class
    processJsonObject(jsonObj);
}
