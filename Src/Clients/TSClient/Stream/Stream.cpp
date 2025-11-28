#include <QJsonObject>

#include "Stream.h"

Q_LOGGING_CATEGORY(StreamLog, "Stream")


Stream::Stream() :
    QObject(nullptr)
{
}

Stream::~Stream()
{
    qCDebug(StreamLog) << "Destroying Stream " << this->objectName() << " for symbol " << symbol;
}



void Stream::processRawData(const QByteArray& rawData)
{
    const QString streamName = this->objectName();

    m_accumulatedData.append(rawData);

    while(true) {
        int delimiterPos = m_accumulatedData.indexOf('\n');
        if (delimiterPos == -1) {
            // No complete object yet, wait for more data
            //TODO implement metric
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
            continue;
        }

        QJsonObject jsonObj = doc.object();

        if (jsonObj.contains("Heartbeat") && jsonObj.contains("Timestamp")) {
            // TODO implement a container of all the stream timers for eficient timer management
            heartbeatTimer.start(timeoutMS);
            qCDebug(StreamLog) << streamName << "received heartbeat";
        }
        else if (jsonObj.contains("Error") && jsonObj.contains("Message")) {
            streamIsInError = true;
            StreamError error;
            QString errorStr = jsonObj["Error"].toString();

            if (errorStr == "BadRequest"){
                error = StreamError::BadRequest;
            } else if (errorStr == "DualLogon") {
                error = StreamError::DualLogon;
            } else if (errorStr == "GoAway") {
                error = StreamError::GoAway;
            } else if (errorStr == "InternalServerError") {
                error = StreamError::InternalServerError;
            } else if (errorStr == "InvalidSymbol"){
                error = StreamError::InvalidSymbol;
            } else {
                error = StreamError::Unknown;
                qCCritical(StreamLog) << streamName << "received unknown error string: " << errorStr;
            }

            qCCritical(StreamLog) << streamName << " received an error : " << jsonObj["Message"].toString();

            emit streamErrorOccurred(error, jsonObj["Message"].toString());
        }
        else { // Happy path, process the object
            if (processJsonObject(jsonObj)) {
                heartbeatTimer.start(timeoutMS);
            } else {
                qCCritical(StreamLog) << "The stream " << streamName << " failed to process Json object";
            }
        }
    }
}

void Stream::onReplyStreamReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    QByteArray rawData = reply->readAll();

    #warning deal with this shit
    //processNewAmountOfDataReceived(rawData.size());

    processRawData(rawData);
}

void Stream::onReplyStreamFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    Q_CHECK_PTR(reply);

    const QString streamName = this->objectName();

    qCWarning(StreamLog) << streamName << "received the signal finished(). There has been a previous json description of the error received, and this is the remote closing this connection.";

    QByteArray rawData = reply->readAll();

    #warning deal with this shit
    //processNewAmountOfDataReceived(rawData.size());

    streamIsFinished = true;

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

void Stream::onReplyStreamErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply)
{
    Q_CHECK_PTR(reply);

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
    streamIsInError = true;

    const QString streamName = this->objectName();

    qCCritical(StreamLog) << "The stream " << streamName << " did not receive data nor heartbeat in : " << timeoutMS  << "ms";

    emit streamErrorOccurred(StreamError::Timeout, QString("Stream did not receive data nor heartbeat"));
}