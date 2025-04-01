#pragma once

#include <QObject>
#include <QNetworkReply>

class Stream : public QObject {
    Q_OBJECT
protected:
    explicit Stream(QNetworkReply *reply);
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

signals:
    void marketDepthNotAvailable();
protected:
    QNetworkReply *reply;

    virtual void processJson(QByteArray &json) = 0; // Just to make sure Stream is an abstract class

    friend class TSClient; // Friended only to be able to connect to those signals. Perhaps a bit of a hack
    void onReadyRead();
    void onFinished();
    void onError(QNetworkReply::NetworkError error);
};

