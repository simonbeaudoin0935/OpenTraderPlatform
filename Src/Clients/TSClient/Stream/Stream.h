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

    static size_t getNumberOpenStream() { return s_numberOfStream; }
    
signals:
    void newAmountOfDataReceived(size_t bytes);

public slots:
    void onReplyStreamReadyRead();
    void onReplyStreamFinished();

protected:
    // Each derived class must implement how to process a json object
    virtual void processJsonObject(const QJsonObject& doc) = 0;

    // This is the abstract interface that every QPromise<T> implements
    class PromiseInterface {
    public:
        virtual ~PromiseInterface() = default;
        virtual void finish() = 0;
        virtual void setException(const QException &e) = 0;
        //virtual void start() = 0;
        //virtual void setProgressValue(int v) = 0;
        // add more as needed: start(), setProgressValue(), cancel(), suspend(), etc.
    };

    // The base holds only a pointer to the interface
    std::unique_ptr<PromiseInterface> promiseImpl;

    // Helper so derived classes can install their typed promise
    template<typename T>
    void setPromise(QPromise<T>& p) {
        // Wrap the typed promise into the interface
        struct Wrapper : PromiseInterface {
            QPromise<T>& ref;
            Wrapper(QPromise<T>& r) : ref(r) {}
            void finish() override              { ref.finish(); }
            void setException(const QException &e) override { ref.setException(e); }
        };
        promiseImpl = std::make_unique<Wrapper>(p);
    }

private slots:
    void onHeartbeatTimerTimeout();

private:
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

