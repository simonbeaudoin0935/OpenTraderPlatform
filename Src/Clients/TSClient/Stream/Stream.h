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
    Stream();
    ~Stream();

    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    bool isFinished() const {return streamIsFinished;}
    bool isInError()  const {return streamIsInError;}


    enum class StreamError {
        Timeout,
        BadRequest,
        DualLogon,
        GoAway,
        InternalServerError,
        InvalidSymbol,
        Unknown
    };

signals:

    void streamErrorOccurred(StreamError error, QString errorMessage);

public slots:
    void onReplyStreamReadyRead();
    void onReplyStreamFinished();
    void onReplyStreamErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply);

private slots:
    void onHeartbeatTimerTimeout();

protected:

    virtual bool processJsonObject(const QJsonObject& doc) = 0;

private:
    void processRawData(const QByteArray& rawData);

    QTimer heartbeatTimer;
    bool streamIsInError = false;
    bool streamIsFinished = false;

    unsigned int timeoutMS = 10000;

    QByteArray m_accumulatedData;
};

