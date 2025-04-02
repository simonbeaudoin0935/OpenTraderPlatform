#pragma once

#include <QObject>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTimer>
#include <QNetworkReply>

Q_DECLARE_LOGGING_CATEGORY(StreamLog)

class Stream : public QObject {
    Q_OBJECT

public:
    virtual ~Stream();

    // Calling this function arms the timeout timer!
    void setNetworkReply(QNetworkReply *networkReply);

    bool isFinished() const {return streamIsFinished;}
    bool isInError()  const {return streamIsInError;}

    enum class StreamError {
        Timeout,
        BadRequest,
        DualLogon,
        GoAway,
        InternalServerError,
        Unknown
    };

signals:
#warning TODO // Move this from Stream to marketDepth*
    void marketDepthNotAvailable();
    void streamErrorOccurred(StreamError error, QString errorMessage);
    void receivedAmountOfData(qsizetype bytes);

public slots:
    void onReadyRead();
    void onErrorOccurred(QNetworkReply::NetworkError code);
    void onFinished();

private slots:
    void onHeartbeatTimerTimeout();

protected:
    explicit Stream(QObject *parent = nullptr);

    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    virtual bool processJsonObject(const QJsonObject& doc) = 0;
private:
    QTimer *heartbeatTimer = nullptr;
    bool streamIsInError = false;
    bool streamIsFinished = false;

    QNetworkReply *reply = nullptr;
    unsigned int timeoutMS = 7000;
};

