#include <QJsonDocument>
#include <QJsonObject>

#include "Stream.h"

Stream::Stream()
{
}

Stream::~Stream()
{
}

void Stream::onReadyRead(QNetworkReply *reply, QByteArray &data)
{
    qDebug() << "Data received:" << data;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << Q_FUNC_INFO << "Failed to parse JSON:" << parseError.errorString();
        return;
    }

    QJsonObject jsonObj = doc.object();
    
    processJson(doc);
}

void Stream::onFinished(QNetworkReply *reply, QByteArray &data)
{
    qWarning() << Q_FUNC_INFO << "The stream finished, which should not happen";

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << Q_FUNC_INFO << "Failed to parse JSON:" << parseError.errorString();
        return;
    }

    QJsonObject jsonObj = doc.object();

    // Check for error message about missing scope
    if (jsonObj.contains("Message") && jsonObj.contains("StatusCode")) {
        QString message = jsonObj["Message"].toString();
        int statusCode = jsonObj["StatusCode"].toInt();

        if (message == "Missing required scope." && statusCode == 403) {
            qWarning() << Q_FUNC_INFO << "Missing required scope error detected";

            emit Stream::marketDepthNotAvailable();
            return;
        }
    }
}

/*
void Stream::onError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);
    Q_ASSERT(reply->error() != QNetworkReply::NoError);

    if (reply->error() == QNetworkReply::ContentAccessDenied) {
        qWarning() << Q_FUNC_INFO <<
            "Level2 data is not activated on the account";

        if (!isMarketDepthNotAvailableAlreadyEmitted) { // This is a hack to avoid emitting the signal multiple times
            emit marketDepthNotAvailable();
            isMarketDepthNotAvailableAlreadyEmitted = true;
        }
    }
    qWarning() << Q_FUNC_INFO <<
        "The stream received an error : " << reply->errorString() << reply->error();
}
*/
