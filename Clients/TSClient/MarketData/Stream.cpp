#include <QJsonDocument>
#include <QJsonObject>

#include "Stream.h"

Q_LOGGING_CATEGORY(StreamLog, "Stream")


Stream::Stream()
{
}

Stream::~Stream()
{
}

void Stream::setNetworkReply(QNetworkReply *networkReply)
{
    reply = networkReply;

    connect(reply, &QNetworkReply::readyRead, this, &Stream::onReadyRead);
    connect(reply, &QNetworkReply::errorOccurred, this, &Stream::onErrorOccurred);
    connect(reply, &QNetworkReply::finished, this, &Stream::onFinished);

    connect(&heartbeatTimer, &QTimer::timeout, this, &Stream::onHeartbeatTimerTimeout);
}

#warning TODO dechunking
void Stream::onReadyRead()
{
    Q_ASSERT(reply != nullptr);

    QByteArray rawData = reply->readAll();
    qsizetype bytesReceived = rawData.size();

    emit receivedAmountOfData(bytesReceived);

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(StreamLog) << Q_FUNC_INFO << "Failed to parse JSON:" << parseError.errorString();
        return;
    }

    QJsonObject jsonObj = doc.object();
    
    if (jsonObj.contains("Heartbeat") && jsonObj.contains("Timestamp")) {
        heartbeatTimer.start(timeoutMS);
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
        } else {
            error = StreamError::Unknown;
        }

        emit streamErrorOccurred(error, jsonObj["Message"].toString());
    }
    else {
        if (processJsonObject(jsonObj)) {
            heartbeatTimer.start(timeoutMS);
        } else {
            qCWarning(StreamLog) << Q_FUNC_INFO << "Failed to process Json object";
        }
    }
}


void Stream::onFinished()
{
    qCWarning(StreamLog) << Q_FUNC_INFO << "The stream " << static_cast<void*>(this) << " received the signal finished()";

    QByteArray rawData = reply->readAll();

    streamIsFinished = true;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << Q_FUNC_INFO << "Failed to parse JSON:" << parseError.errorString();
        qWarning() << Q_FUNC_INFO << "Bad content : " << rawData;
        return;
    }

    QJsonObject jsonObj = doc.object();

    // Check for error message about missing scope
    if (jsonObj.contains("Message") && jsonObj.contains("StatusCode")) {
        QString message = jsonObj["Message"].toString();
        int statusCode = jsonObj["StatusCode"].toInt();

        if (message == "Missing required scope." && statusCode == 403) {
            qWarning() << Q_FUNC_INFO << "Missing required scope error detected";

            emit marketDepthNotAvailable();
            return;
        }
    }
}

void Stream::onErrorOccurred(QNetworkReply::NetworkError code) {

    Q_ASSERT(reply->error() != QNetworkReply::NoError);

    streamIsInError = true;

    qCWarning(StreamLog) << Q_FUNC_INFO <<
        "The stream " << static_cast<void*>(this) << " received the error : " << code << " : " << reply->errorString();

    if (code == QNetworkReply::ContentAccessDenied) {
        qWarning() << Q_FUNC_INFO <<
            "Level2 data is not activated on the account";

        //if (!isMarketDepthNotAvailableAlreadyEmitted) { // This is a hack to avoid emitting the signal multiple times
            emit marketDepthNotAvailable();
        //    isMarketDepthNotAvailableAlreadyEmitted = true;
        //}
    }
}

void Stream::onHeartbeatTimerTimeout()
{
    streamIsInError = true;

    qCCritical(StreamLog) << Q_FUNC_INFO <<
        "The stream " << static_cast<void*>(this) << " did not receive data nor heartbeat in : " << timeoutMS  << "ms";

    emit streamErrorOccurred(StreamError::Timeout, QString("Stream did not receive data nor heartbeat"));
}
