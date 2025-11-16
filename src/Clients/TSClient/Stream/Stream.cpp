#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QDir>
#include <QFileInfo>

#include "Stream.h"

Q_LOGGING_CATEGORY(StreamLog, "Stream")


Stream::Stream(QString symbol, QObject *parent) :
    QObject(parent),
    symbol(symbol)
{
}

Stream::~Stream()
{
    qCDebug(StreamLog) << "Destroying Stream " << this->objectName() << " for symbol " << symbol;
}

void Stream::setNetworkReply(QNetworkReply *networkReply)
{
    Q_ASSERT(networkReply != nullptr);

    reply = networkReply;

    connect(reply, &QNetworkReply::readyRead, this, &Stream::onReadyRead);
    connect(reply, &QNetworkReply::errorOccurred, this, &Stream::onErrorOccurred);
    connect(reply, &QNetworkReply::finished, this, &Stream::onFinished);

    connect(&heartbeatTimer, &QTimer::timeout, this, &Stream::onHeartbeatTimerTimeout);
}

void Stream::onReadyRead()
{
    Q_ASSERT(reply != nullptr);

    QString streamName = this->objectName();

    QByteArray rawData = reply->readAll();
    qsizetype bytesReceived = rawData.size();

    emit receivedAmountOfData(bytesReceived);

    // Start by emitting the raw json for recording purposes if anyone binds to this signal

    // TODO tomorrow: bring the symbol in this stream class instead of children classes so 
    // that we can emit the raw json here
    emit receivedNewRawData(symbol, rawData);

    accumulatedData.append(rawData);

    qCDebug(StreamLog) << streamName << "is ready read";

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

        // Write raw data to recording file if recording is active

        if (parseError.error != QJsonParseError::NoError) {
            qCCritical(StreamLog) << streamName << "Failed to parse JSON:" << parseError.errorString();
            qCDebug(StreamLog) << streamName << "Raw data : " << jsonData;
            continue;
        }

        QJsonObject jsonObj = doc.object();

        if (jsonObj.contains("Heartbeat") && jsonObj.contains("Timestamp")) {
            heartbeatTimer.start(timeoutMS);
            qCInfo(StreamLog) << streamName << "received heartbeat";
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

            qCInfo(StreamLog) << streamName << " received an error : " << jsonObj["Message"].toString();

            emit streamErrorOccurred(error, jsonObj["Message"].toString());
        }
        else {
            if (processJsonObject(jsonObj)) {
                heartbeatTimer.start(timeoutMS);
            } else {
                qCCritical(StreamLog) << "The stream " << streamName << " failed to process Json object";
            }
        }
    }
}


void Stream::onFinished()
{
    QString streamName = this->objectName();

    qCWarning(StreamLog) << streamName << "received the signal finished()";

    QByteArray rawData = reply->readAll();

    streamIsFinished = true;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCCritical(StreamLog) << streamName << "Failed to parse JSON:" << parseError.errorString();
        qCCritical(StreamLog) << "Bad content : " << rawData;
        return;
    }

    QJsonObject jsonObj = doc.object();

    // Check for error message about missing scope
    if (jsonObj.contains("Message") && jsonObj.contains("StatusCode")) {
        QString message = jsonObj["Message"].toString();
        int statusCode = jsonObj["StatusCode"].toInt();

        qCWarning(StreamLog) << streamName << "Received message : " << message << " with status code : " << statusCode;
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

    QString streamName;

    if (this->objectName().isEmpty()) {
        streamName = QString::asprintf("Name not set, ptr = %p", this);
    } else {
        streamName = this->objectName();
    }

    qCCritical(StreamLog) << "The stream <" << streamName << "> did not receive data nor heartbeat in : " << timeoutMS  << "ms"
                          << Q_FUNC_INFO;

    emit streamErrorOccurred(StreamError::Timeout, QString("Stream did not receive data nor heartbeat"));
}