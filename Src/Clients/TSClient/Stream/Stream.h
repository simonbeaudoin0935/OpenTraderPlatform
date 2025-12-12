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

    class TimeoutException : public QException {
    public:
        void raise() const override { throw *this; }
        TimeoutException *clone() const override { return new TimeoutException(*this); }
    };
    class BadRequestException : public QException {
    public:
        void raise() const override { throw *this; }
        BadRequestException *clone() const override { return new BadRequestException(*this); }
    };    
    class DualLogonException : public QException {
    public:
        void raise() const override { throw *this; }
        DualLogonException *clone() const override { return new DualLogonException(*this); }
    };
    class GoAwayException : public QException {
    public:
        void raise() const override { throw *this; }
        GoAwayException *clone() const override { return new GoAwayException(*this); }
    };
    class InternalServerErrorException : public QException {
    public:
        void raise() const override { throw *this; }
        InternalServerErrorException *clone() const override { return new InternalServerErrorException(*this); }
    };           
    class InvalidSymbolException : public QException {
    public:
        void raise() const override { throw *this; }
        InvalidSymbolException *clone() const override { return new InvalidSymbolException(*this); }
    };        
    
    
    Stream(QNetworkReply * reply, QObject *parent = nullptr);
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    QFuture<void> future() const { return m_future; }

    static size_t getNumberOpenStream() { return s_numberOfStream; }
    
signals:
    void newAmountOfDataReceived(size_t bytes);

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

