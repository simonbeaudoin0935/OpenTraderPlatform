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

    Stream(QNetworkReply * reply, QObject *parent);
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    QFuture<void> future() const { return m_future; }

    static size_t getNumberOpenStream() { return s_numberOfStream; }

signals:
    void newAmountOfDataReceived(size_t bytes);
    void receivedNewRawData(const QByteArray& rawData);

public slots:
    void onReplyReadyRead();
    void onReplyFinished();

protected:
    // Each derived class must implement how to process a json object
    virtual void processJsonObject(const QJsonObject& doc) = 0;

    bool m_receivedTimeoutError = false;

    QString m_jsonErrorString;
    
private slots:
    void onHeartbeatTimerTimeout();

private:

    QPromise<void> m_promise;
    QFuture<void> m_future;



    QByteArray   m_accumulatedData;
    void processRawData(const QByteArray& rawData);
    void processJsonDoc(const QJsonDocument& doc);    
    
    // This is the QNetworkReply that this stream gets attributed to when issuing the http->get request
    QNetworkReply* m_networkReply = nullptr;

    const size_t m_heartbeatTimeoutMS = 10000;
    QTimer       m_heartbeatTimer;

    static size_t s_numberOfStream; 
};

