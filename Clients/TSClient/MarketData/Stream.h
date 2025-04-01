#pragma once

#include <QObject>
#include <QNetworkReply>
#include <QJsonDocument>

class Stream : public QObject {
    Q_OBJECT

public:
    void onReadyRead(QNetworkReply *reply, QByteArray &data);
    void onFinished(QNetworkReply *reply, QByteArray &data);

    bool isFinished() const {return receivedFinishedReply;};
protected:
    explicit Stream();
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

signals:
    void marketDepthNotAvailable();
protected:

    bool receivedFinishedReply = false;
    virtual void processJson(const QJsonDocument& doc) = 0;


    //void onError(QNetworkReply::NetworkError error);

private:
};

