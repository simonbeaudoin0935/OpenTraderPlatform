#pragma once

#include <QObject>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTimer>
#include <QNetworkReply>
#include <QFile>
#include <QTextStream>

Q_DECLARE_LOGGING_CATEGORY(StreamLog)

class Stream : public QObject {
    Q_OBJECT

public:
    virtual ~Stream();

    // Calling this function arms the timeout timer!
    void setNetworkReply(QNetworkReply *networkReply);
    QNetworkReply* getNetworkReply() const { return reply; }

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
    // This exist so that RecorderLogic can record the received json to replay later
    void receivedNewRawData(QString symbol, const QByteArray jsonObj);

    void streamErrorOccurred(StreamError error, QString errorMessage);
    void receivedAmountOfData(qsizetype bytes);

public slots:
    void onReadyRead();
    void onErrorOccurred(QNetworkReply::NetworkError code);
    void onFinished();

private slots:
    void onHeartbeatTimerTimeout();

protected:

    explicit Stream(QString symbol = "", QObject *parent = nullptr);

    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    virtual bool processJsonObject(const QJsonObject& doc) = 0;

    QString symbol;

private:
    QTimer heartbeatTimer;
    bool streamIsInError = false;
    bool streamIsFinished = false;

    QNetworkReply *reply = nullptr;
    unsigned int timeoutMS = 10000;

    QByteArray accumulatedData;
};

