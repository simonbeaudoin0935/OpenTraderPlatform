#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include "Stream.h"

Q_LOGGING_CATEGORY(StreamLog, "Stream")


Stream::Stream(QObject *parent) :
    QObject(parent)
{
}

Stream::~Stream()
{
}

void Stream::setNetworkReply(QNetworkReply *networkReply)
{
    Q_ASSERT(networkReply != nullptr);

    reply = networkReply;

    connect(reply, &QNetworkReply::readyRead, this, &Stream::onReadyRead);
    connect(reply, &QNetworkReply::errorOccurred, this, &Stream::onErrorOccurred);
    connect(reply, &QNetworkReply::finished, this, &Stream::onFinished);

    heartbeatTimer = new QTimer(this);
    Q_CHECK_PTR(heartbeatTimer);

    connect(heartbeatTimer, &QTimer::timeout, this, &Stream::onHeartbeatTimerTimeout);
}

void Stream::onReadyRead()
{
    Q_ASSERT(reply != nullptr);

    QByteArray rawData = reply->readAll();
    qsizetype bytesReceived = rawData.size();

    emit receivedAmountOfData(bytesReceived);

    accumulatedData.append(rawData);

    while(true) {
        int delimiterPos = accumulatedData.indexOf('\n');
        if (delimiterPos == -1) {
            // No complete object yet, wait for more data
            break;
        }

        QByteArray jsonData = accumulatedData.left(delimiterPos + 1);
        accumulatedData.remove(0, delimiterPos + 1); // Remove extracted data from buffer

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qCWarning(StreamLog) << Q_FUNC_INFO << "Failed to parse JSON:" << parseError.errorString();
            qCWarning(StreamLog) << "Raw data : " << jsonData;
            continue;
        }

        QJsonObject jsonObj = doc.object();

        if (jsonObj.contains("Heartbeat") && jsonObj.contains("Timestamp")) {
            heartbeatTimer->start(timeoutMS);
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
                heartbeatTimer->start(timeoutMS);
            } else {
                qCWarning(StreamLog) << Q_FUNC_INFO << "Failed to process Json object";
            }
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

        qWarning() << Q_FUNC_INFO << "Received message : " << message << " with status code : " << statusCode;
    }
}

void Stream::onErrorOccurred(QNetworkReply::NetworkError code) {

    Q_ASSERT(reply->error() != QNetworkReply::NoError);

    streamIsInError = true;

    qCWarning(StreamLog) << Q_FUNC_INFO <<
        "The stream " << static_cast<void*>(this) << " received the error : " << code << " : " << reply->errorString();
}

void Stream::onHeartbeatTimerTimeout()
{
    streamIsInError = true;

    qCCritical(StreamLog) << Q_FUNC_INFO <<
        "The stream " << static_cast<void*>(this) << " did not receive data nor heartbeat in : " << timeoutMS  << "ms";

    emit streamErrorOccurred(StreamError::Timeout, QString("Stream did not receive data nor heartbeat"));
}
