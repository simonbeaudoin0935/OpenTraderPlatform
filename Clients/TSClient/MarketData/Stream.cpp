#include "Stream.h"

Stream::Stream(QNetworkReply *reply) :
    reply(reply)
{
}

Stream::~Stream()
{
    reply->abort();
    reply->deleteLater();
}

void Stream::onReadyRead()
{
    QByteArray json;

    qDebug() << "**********************************************yeeeeeeeee";

    QByteArray data = reply->readAll();
    qDebug() << "Data received:" << data;

    processJson(json);
}

void Stream::onFinished()
{
    qWarning() << Q_FUNC_INFO << "The stream finished, which should not happen";
}

void Stream::onError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);
    Q_ASSERT(reply->error() != QNetworkReply::NoError);

    if (reply->error() == QNetworkReply::ContentAccessDenied) {
        qWarning() << Q_FUNC_INFO <<
            "Level2 data is not activated on the account";

        emit marketDepthNotAvailable();
    }
    qWarning() << Q_FUNC_INFO <<
        "The stream received an error : " << reply->errorString() << reply->error();
}
