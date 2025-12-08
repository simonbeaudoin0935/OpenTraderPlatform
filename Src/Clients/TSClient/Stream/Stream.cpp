#include <QJsonObject>

#include "Stream.h"

Q_LOGGING_CATEGORY(StreamLog, "Stream")

size_t Stream::s_numberOfStream = 0;

Stream::Stream(QObject *parent) :
    QObject(parent)
{
    s_numberOfStream++;
}

Stream::~Stream()
{
    s_numberOfStream--;

    m_networkReply->abort();
    m_networkReply->deleteLater();

    qCDebug(StreamLog) << "Destroying Stream " << this->objectName();
}


void Stream::setNetworkReply(QNetworkReply * reply)
{
    Q_CHECK_PTR(reply);
    m_networkReply = reply; 

    QMetaObject::Connection c;
    c = connect(m_networkReply, &QNetworkReply::readyRead, this, &Stream::onReplyStreamReadyRead);
    Q_ASSERT(c);
    c = connect(m_networkReply, &QNetworkReply::finished, this, &Stream::onReplyStreamReadyRead);
    Q_ASSERT(c);
    c = connect(m_networkReply, &QNetworkReply::errorOccurred, this, &Stream::onReplyStreamErrorOccurred);
    Q_ASSERT(c);
}


void Stream::onReplyStreamReadyRead()
{
    const QByteArray rawData = m_networkReply->readAll();

    Q_ASSERT(rawData.size() > 0);

    emit newAmountOfDataReceived(rawData.size());

    processRawData(rawData);
}

#warning TODO complete
void Stream::onReplyStreamFinished()
{
    const QString streamName = this->objectName();

    qCWarning(StreamLog) << streamName << "received the signal finished(). There has been a previous json description of the error received, and this is the remote closing this connection.";

    QByteArray rawData = m_networkReply->readAll();

    emit newAmountOfDataReceived(rawData.size());

    m_streamIsFinished = true;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCCritical(StreamLog) << streamName << "Failed to parse JSON:" << parseError.errorString();
        qCCritical(StreamLog) << "Bad content : " << rawData << ". Empty quotes are expected when the server closes the connection after an error.";
        return;
    }

    QJsonObject jsonObj = doc.object();

    // Check for error message about missing scope
    if (jsonObj.contains("Message") && jsonObj.contains("StatusCode")) {
        QString message = jsonObj["Message"].toString();
        int statusCode = jsonObj["StatusCode"].toInt();

        qCWarning(StreamLog) << streamName << "Received message : " << message << " with status code : " << statusCode;
    }

    #warning deal with this shit
    Q_ASSERT(false);
}

#warning TODO complete
void Stream::onReplyStreamErrorOccurred(QNetworkReply::NetworkError code)
{
    switch(code)
    {
        case QNetworkReply::RemoteHostClosedError:
            qCCritical(StreamLog) << "Remote host closed the connection unexpectedly.";
            break;
    };

    Q_ASSERT(false);
}


void Stream::onHeartbeatTimerTimeout()
{
    m_errorStatus = ErrorStatus::Timeout;

    const QString streamName = this->objectName();

    qCCritical(StreamLog) << "The stream " << streamName << " did not receive data nor heartbeat in : " << m_heartbeatTimeoutMS  << "ms";

    emit streamErrorOccurred(m_errorStatus, QString("Stream supposed to receive data or heartbeat every 5000ms, received nothing in 10000ms"));
}

void Stream::processRawData(const QByteArray& rawData)
{
    const QString streamName = this->objectName();

    m_accumulatedData.append(rawData);

    while(true) {

        int delimiterPos = m_accumulatedData.indexOf('\n');
        if (delimiterPos == -1) {
            // No complete object yet, wait for more data
            m_metricIncompleteJsonObjectWhenParsing++;
            
            break;
        }

        // TODO optimize to avoid copy
        QByteArray jsonData = m_accumulatedData.left(delimiterPos + 1);
        m_accumulatedData.remove(0, delimiterPos + 1); // Remove extracted data from buffer

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qCCritical(StreamLog) << streamName << "Failed to parse JSON:" << parseError.errorString();
            qCDebug(StreamLog) << streamName << "Raw data : " << jsonData;
            
            m_metricJsonParseError++;
            
            continue;
        }

        QJsonObject jsonObj = doc.object();

        if (jsonObj.contains("Heartbeat") && jsonObj.contains("Timestamp")) {
            // TODO implement a container of all the stream timers for eficient timer management
            // Kick the watchdog
            m_heartbeatTimer.start(m_heartbeatTimeoutMS);
            qCDebug(StreamLog) << streamName << "received heartbeat";

            continue;
        }

        if (jsonObj.contains("Error") && jsonObj.contains("Message")) {

            QString errorStr = jsonObj["Error"].toString();

            if (errorStr == "BadRequest"){
                m_errorStatus = ErrorStatus::BadRequest;
            } else if (errorStr == "DualLogon") {
                m_errorStatus = ErrorStatus::DualLogon;
            } else if (errorStr == "GoAway") {
                m_errorStatus = ErrorStatus::GoAway;
            } else if (errorStr == "InternalServerError") {
                m_errorStatus = ErrorStatus::InternalServerError;
            } else if (errorStr == "InvalidSymbol"){
                m_errorStatus = ErrorStatus::InvalidSymbol;
            } else {
                m_errorStatus = ErrorStatus::Unknown;
                qCCritical(StreamLog) << streamName << "received unknown error string: " << errorStr;
            }

            qCCritical(StreamLog) << streamName << " received an error : " << jsonObj["Message"].toString();

            emit streamErrorOccurred(m_errorStatus, jsonObj["Message"].toString());

            continue;
        }

        // Happy path, process the object
        
        if (processJsonObject(jsonObj)) {
            m_heartbeatTimer.start(m_heartbeatTimeoutMS);
        } else {
            qCCritical(StreamLog) << "The stream " << streamName << " failed to process Json object";
        }
    }
}
