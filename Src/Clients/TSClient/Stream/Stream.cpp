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


void Stream::onFinished()
{
    QString streamName = this->objectName();

    qCWarning(StreamLog) << streamName << "received the signal finished(). There has been a previous json description of the error received, and this is the remote closing this connection.";

    QByteArray rawData = reply->readAll();

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
}

void Stream::onErrorOccurred(QNetworkReply::NetworkError code) {

    Q_ASSERT(reply->error() != QNetworkReply::NoError);

    streamIsInError = true;

    qCWarning(StreamLog) << Q_FUNC_INFO << "The stream " << static_cast<void*>(this) << " received the error : " << code << " : " << reply->errorString();


    switch(code)
    {
        case QNetworkReply::RemoteHostClosedError:
            qCCritical(StreamLog) << "Remote host closed the connection unexpectedly.";
            break;
    };
}

void Stream::onHeartbeatTimerTimeout()
{
    streamIsInError = true;

    const QString streamName = this->objectName();

    qCCritical(StreamLog) << "The stream " << streamName << " did not receive data nor heartbeat in : " << timeoutMS  << "ms";

    emit streamErrorOccurred(StreamError::Timeout, QString("Stream did not receive data nor heartbeat"));
}