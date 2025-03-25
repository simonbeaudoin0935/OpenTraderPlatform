#ifndef FMPCLIENT_H
#define FMPCLIENT_H

#include <QObject>
#include <QThread>
#include <QNetworkAccessManager>
#include <QMutex>
#include <QWaitCondition>
#include <QHash>

class FMPClient : public QObject {
    Q_OBJECT
public:
    explicit FMPClient(const QString &apiKey);
    ~FMPClient();

    void fetchQuoteAsync(const QString &symbol);
    bool fetchQuoteSync(const QString &symbol, double &price, double &bid, double &ask);

signals:
    void quoteReceived(double price, double bid, double ask) const;

private slots:
    void onThreadStarted() const;
    void onReplyFinished(QNetworkReply *reply);

private:
    enum class RequestType { QuoteAsync, QuoteSync };

    struct RequestInfo {
        RequestType type;
        bool completed = false;
        double price = 0.0;
        double bid = 0.0;
        double ask = 0.0;
    };

    QString buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const;

    QThread *thread;
    QNetworkAccessManager *manager;
    mutable QMutex mutex;
    QWaitCondition waitCondition;
    QHash<QNetworkReply*, RequestInfo> pendingRequests;
    QString apiKey;
    const QString baseUrl = "https://financialmodelingprep.com/stable/";
};

#endif // FMPCLIENT_H
