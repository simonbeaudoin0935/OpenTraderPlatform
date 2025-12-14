#include <QJsonObject>
#include <QMetaEnum>
#include <QNetworkReply>

#include "Stream.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog
Q_LOGGING_CATEGORY(LOGGING_CATEGORY, "Stream")

size_t Stream::s_numberOfStream = 0;


Stream::Stream(QNetworkReply * reply, QObject *parent) :
    QObject(parent),
    m_networkReply(reply)
{
    Q_CHECK_PTR(reply);

    INFO << "Stream created ";

    s_numberOfStream++;

    m_networkReply->setParent(this);

    QMetaObject::Connection c;
    c = connect(m_networkReply, &QNetworkReply::readyRead, this, &Stream::onReplyReadyRead);
    Q_ASSERT(c);
    c = connect(m_networkReply, &QNetworkReply::finished, this, &Stream::onReplyFinished);
    Q_ASSERT(c);

    m_heartbeatTimer.setSingleShot(true);
    c = connect(&m_heartbeatTimer, &QTimer::timeout, this, &Stream::onHeartbeatTimerTimeout);
    Q_ASSERT(c);

    // Creating a Stream implies that we expect data to start flowing in because the QNetworkReply
    // that was passed has been obtained after the HTTP x request, so we start the heartbeat timer now
    m_heartbeatTimer.start(m_heartbeatTimeoutMS);

    m_future = m_promise.future();
    m_promise.start();
}

Stream::~Stream()
{
    WARNING << "Stream destroyed ";

    Q_ASSERT_X(QThread::currentThread() == this->thread(),
               "Stream::~Stream",
               "Stream must be destroyed in the same thread where it was created");

    s_numberOfStream--;

    m_networkReply->abort();
    m_networkReply->deleteLater();

    // Here, maybe its this stream that asked to destroy itself because of an error detected in the stream,
    // or maybe the user of the stream is destroying it because he wants to stop it.
    // In any case, we must finish the promise to mark the stream as finished, and if there was an error, we
    // must have already set the exception in the promise.
    m_promise.finish();
    m_heartbeatTimer.stop();
}

void Stream::onReplyFinished()
{
    CRITICAL << "received the signal finished()";

    Q_CHECK_PTR(m_networkReply);

    // Because we listen to readyRead(), it WILL have been called before and therefore finishing there should be no more data
    Q_ASSERT(m_networkReply->readAll().size() == 0);

    // Theres 3 ways to get here:
    // 1) In the previous readyRead() call, we detected an error object in the stream and marked m_isInError = true
    // 2) The heartbeat timer timed out and aborted the network reply
    // 3) There is a network error reported by QNetworkReply (timeout, disconnection, etc )
    QString exceptionString = "Timeout: " + (m_receivedTimeoutError ? QString("true"):QString("false")) + " JSON Error: " + m_jsonErrorString + ". Network error: " + m_networkReply->errorString();

    CRITICAL << "exceptionString: " << exceptionString;

    m_heartbeatTimer.stop();

    m_promise.setException(std::make_exception_ptr(
        std::runtime_error(exceptionString.toStdString())    
    ));

    // m_promise.finish() will be called in the destructor
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
    Q_ASSERT(rawData.size() > 0);

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
    // Doesnt matter what it, heartbeat or data or error, we kick the heartbeat watchdog
    m_heartbeatTimer.start(m_heartbeatTimeoutMS);

    m_accumulatedData.append(rawData);

    emit receivedNewRawData(rawData);
    
    // Decode and parse one JSON document at a time, as long as we have complete objects (newline delimited)
    for(int delimiterPos = m_accumulatedData.indexOf('\n'); delimiterPos != -1; delimiterPos = m_accumulatedData.indexOf('\n'))
    {
        // TODO optimize to avoid copy
        const QByteArray jsonData = m_accumulatedData.left(delimiterPos + 1);
        m_accumulatedData.remove(0, delimiterPos + 1); // Remove extracted data from buffer

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
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
 * Handles error and heartbeat objects, and delegates normal objects to derived classes.
 */
void Stream::processJsonDoc(const QJsonDocument& doc)
{
    const QJsonObject jsonObj = doc.object();

    // All types of stream have in common this heartbeat object
    if (jsonObj.contains("Heartbeat")) {

        // Here, we just test that the heartbeat object is well formed, we don't actually care about the timestamp
        // We will kick the watchdog timer anyway
        if (jsonObj.contains("Timestamp") == false) [[unlikely]]{
            CRITICAL << " received malformed heartbeat object without Timestamp field";
        }

        m_heartbeatTimer.start(m_heartbeatTimeoutMS);
        //DEBUG << "received heartbeat";
        return;;
    }

    // Each type of stream may have its own error object, so we delegate to derived classes
    // to handle error objects as they see fit.

    // Happy path, process the object
    // Delegate to derived class for processing    
    processJsonObject(jsonObj);
}
