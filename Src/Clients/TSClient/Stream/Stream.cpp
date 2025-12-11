#include <QJsonObject>

#include "Stream.h"

Q_LOGGING_CATEGORY(StreamLog, "Stream")

template<typename T> size_t Stream<T>::s_numberOfStream = 0;

template<typename T> Stream<T>::Stream(QNetworkReply * reply, QPromise<T> movedDataPromise, QObject *parent) :
    QObject(parent),
    m_dataPromise(std::move(movedDataPromise)),
    m_networkReply(reply)
{
    Q_CHECK_PTR(reply);

    s_numberOfStream++;

    m_networkReply->setParent(this);

    QMetaObject::Connection c;
    c = connect(m_networkReply, &QNetworkReply::readyRead, this, &Stream::onReplyStreamReadyRead);
    Q_ASSERT(c);
    c = connect(m_networkReply, &QNetworkReply::finished, this, &Stream::onReplyStreamFinished);
    Q_ASSERT(c);

    m_heartbeatTimer.setSingleShot(true);
    c = connect(&m_heartbeatTimer, &QTimer::timeout, this, &Stream::onHeartbeatTimerTimeout);
    Q_ASSERT(c);

    // Creating a Stream implies that we expect data to start flowing in because the QNetworkReply
    // that was passed has been obtained after the HTTP x request, so we start the heartbeat timer now
    m_heartbeatTimer.start(m_heartbeatTimeoutMS);
}

template<typename T> Stream<T>::~Stream()
{
    qCDebug(StreamLog) << "Destroying Stream " << this->objectName();

    s_numberOfStream--;

    m_networkReply->abort();
    m_networkReply->deleteLater();

    m_dataPromise.finish();
    m_heartbeatTimer.stop();
}

template<typename T> void Stream<T>::onReplyStreamReadyRead()
{
    const QByteArray rawData = m_networkReply->readAll();

    Q_ASSERT(rawData.size() > 0);

    emit newAmountOfDataReceived(rawData.size());

    processRawData(rawData);
}

template<typename T> void Stream<T>::onReplyStreamFinished()
{
    const QString streamName = this->objectName();

    qCError(StreamLog) << streamName << "received the signal finished(). There has been a previous json description of the error received, and this is the remote closing this connection.";

    // Because we listen to readyRead(), it WILL have been called becofore and therefore finishing there should be no more data
    Q_ASSERT(m_networkReply->readAll().size() == 0);

    // Logically, in a stream, the reason why we would get finished is because there has been an error
    // received prior that was decoded in the onReadyRead() method.
    Q_ASSERT(m_receivedError == true);

    m_heartbeatTimer.stop();

    m_isFinished = true;

    m_dataPromise.finish();
}

template<typename T> void Stream<T>::onHeartbeatTimerTimeout()
{
    m_isFinished = true;
    m_heartbeatTimer.stop();
    m_networkReply->abort();  
    m_dataPromise.setException(TimeoutException());
    m_dataPromise.finish();

    qCCritical(StreamLog) << "The stream " << this->objectName() << " did not receive data nor heartbeat in : " << m_heartbeatTimeoutMS  << "ms";
}

/*
 * Process raw data received from the network reply.
 * Data is expected to be newline delimited JSON objects.
 * Process as many complete JSON objects as there are in the accumulation buffer possible.
 */
template<typename T> void Stream<T>::processRawData(const QByteArray& rawData)
{
    m_accumulatedData.append(rawData);

    // Doesnt matter what it, heartbeat or data or error, we kick the heartbeat watchdog
    m_heartbeatTimer.start(m_heartbeatTimeoutMS);

    for(int delimiterPos = m_accumulatedData.indexOf('\n'); delimiterPos != -1; delimiterPos = m_accumulatedData.indexOf('\n'))
    {
        // TODO optimize to avoid copy
        const QByteArray jsonData = m_accumulatedData.left(delimiterPos + 1);
        m_accumulatedData.remove(0, delimiterPos + 1); // Remove extracted data from buffer

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qCCritical(StreamLog) << this->objectName() << "Failed to parse JSON:" << parseError.errorString();
            qCDebug(StreamLog) << this->objectName() << "Raw data : " << jsonData;
            
            m_metricJsonParseError++;
            
            // Skip this malformed object and continue processing further data
            continue;
        }

        processJsonDoc(doc);
    }
}

/*
 * Process a JSON document extracted from the stream.
 * Handles error and heartbeat objects, and delegates normal objects to derived classes.
 */
template<typename T> void Stream<T>::processJsonDoc(const QJsonDocument& doc)
{
    const QJsonObject jsonObj = doc.object();

    if (jsonObj.contains("Error")) [[unlikely]] {
            
        QString errorStr = jsonObj["Error"].toString();
        QString message = "no error message";
            
        if ( jsonObj.contains("Message")) [[likely]]{
            message = jsonObj["Message"].toString();
        } else [[unlikely]] {
            qCCritical(StreamLog) << streamName << " received malformed error object without Message field";
        }

        if (errorStr == "BadRequest"){
            m_dataPromise.setException(BadRequestException());
        } else if (errorStr == "DualLogon") {
            m_dataPromise.setException(DualLogonException());
        } else if (errorStr == "GoAway") {
            m_dataPromise.setException(GoAwayException());
        } else if (errorStr == "InternalServerError") {
            m_dataPromise.setException(InternalServerErrorException());
        } else if (errorStr == "InvalidSymbol"){
            m_dataPromise.setException(InvalidSymbolException());
        } else {
            m_dataPromise.setException(std::runtime_error(message.toStdString()));
        }

        qCCritical(StreamLog) << this->objectName() << " received error string '" << errorStr << "' and message: " << jsonObj["Message"].toString();
        
        m_receivedError = true;

        return;
    }

    if (jsonObj.contains("Heartbeat")) {

        // Here, we just test that the heartbeat object is well formed, we don't actually care about the timestamp
        // We will kick the watchdog timer anyway
        if (jsonObj.contains("Timestamp") == false) [[unlikely]]{
            qCCritical(StreamLog) << this->objectName() << " received malformed heartbeat object without Timestamp field";
        }

        m_heartbeatTimer.start(m_heartbeatTimeoutMS);
        qCDebug(StreamLog) << this->objectName() << "received heartbeat";

        return;;
    }

    // Happy path, process the object
    // Delegate to derived class for processing
    processJsonObject(jsonObj);
}
