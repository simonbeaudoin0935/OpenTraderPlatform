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

    enum class ErrorStatus {
        NoError,
        Timeout,
        BadRequest,
        DualLogon,
        GoAway,
        InternalServerError,
        InvalidSymbol,
        Unknown
    };
    
    Stream(QObject *parent = nullptr);
    ~Stream();

    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    [[nodiscard]] bool isFinished() const {return m_streamIsFinished; }
    [[nodiscard]] ErrorStatus getErrorStatus() const {return m_errorStatus; }

    void setNetworkReply(QNetworkReply * reply);

signals:
    void newAmountOfDataReceived(size_t bytes);
    void streamErrorOccurred(ErrorStatus error, QString errorMessage);

public slots:
    void onReplyStreamReadyRead();
    void onReplyStreamFinished();
    void onReplyStreamErrorOccurred(QNetworkReply::NetworkError code);

protected:
    virtual bool processJsonObject(const QJsonObject& doc) = 0;

private slots:
    void onHeartbeatTimerTimeout();

private:
    void processRawData(const QByteArray& rawData);
    
    // This is the QNetworkReply that this stream gets attributed to when issuing the http->get request
    QNetworkReply* m_networkReply = nullptr;

    ErrorStatus  m_errorStatus = ErrorStatus::NoError;
    bool         m_streamIsFinished = false;
    const size_t m_heartbeatTimeoutMS = 10000;
    QTimer       m_heartbeatTimer;
    QByteArray   m_accumulatedData;

    size_t m_metricJsonParseError = 0;
    size_t m_metricIncompleteJsonObjectWhenParsing = 0;
};

