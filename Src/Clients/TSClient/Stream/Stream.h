#pragma once

#include <QObject>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTimer>
#include <QNetworkReply>
#include <QPromise>
#include <QFuture>

Q_DECLARE_LOGGING_CATEGORY(StreamLog)




class Stream : public QObject
{
    Q_OBJECT

public:
    enum StreamError {
        NoError,
        Timeout,
        BadRequest,
        DualLogon,
        GoAway,
        InternalServerError,
        InvalidSymbol,
        Unknown
    };
    Q_ENUM(StreamError);

    StreamError m_streamError = StreamError::NoError;
    QString errorToString() const;

    Stream(QNetworkReply * reply, QObject *parent = nullptr);
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    QFuture<void> future() const { return m_future; }

    static size_t getNumberOpenStream() { return s_numberOfStream; }
    
    bool isFinished() const { return m_isFinished; }
    bool isInError() const { return m_receivedError; }
signals:
    void newAmountOfDataReceived(size_t bytes);
    void receivedNewRawData(const QByteArray& rawData);

public slots:
    void onReplyReadyRead();
    void onReplyFinished();

protected:
    // Each derived class must implement how to process a json object
    virtual void processJsonObject(const QJsonObject& doc) = 0;

private slots:
    void onHeartbeatTimerTimeout();

private:

    QPromise<void> m_promise;
    QFuture<void> m_future;

    bool m_receivedError = false;
    bool m_isFinished = false;

    QByteArray   m_accumulatedData;
    void processRawData(const QByteArray& rawData);
    void processJsonDoc(const QJsonDocument& doc);    
    
    // This is the QNetworkReply that this stream gets attributed to when issuing the http->get request
    QNetworkReply* m_networkReply = nullptr;

    const size_t m_heartbeatTimeoutMS = 10000;
    QTimer       m_heartbeatTimer;

    // Metrics
    size_t m_metricJsonParseError = 0;
    static size_t s_numberOfStream; 
};

