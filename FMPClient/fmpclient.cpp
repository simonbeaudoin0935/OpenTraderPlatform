#include "fmpclient.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>

FMPClient::FMPClient(const QString &apiKey) :
    thread(new QThread()),
    manager(new QNetworkAccessManager(this)),
    apiKey(apiKey)
{
    qDebug() << Q_FUNC_INFO << ": FMPClient created using KEY=" << apiKey;
    Q_ASSERT(!apiKey.isEmpty());

    thread->setObjectName("FPMClientThread");

    //manager->moveToThread(thread);
    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &FMPClient::onThreadStarted);
    connect(manager, &QNetworkAccessManager::finished, this, &FMPClient::onReplyFinished);
    thread->start();
}

FMPClient::~FMPClient() {
    qDebug() << Q_FUNC_INFO << ": FPM Client destroyed with key=" << apiKey;
    thread->quit();
    thread->wait();
}

QString FMPClient::buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const {
    return baseUrl + endpoint + "?" +
           QString("symbol=%1").arg(symbol) + "&" +
           QString("apikey=%1").arg(apiKey);
}

void FMPClient::fetchQuoteAsync(const QString &symbol) {
    QMutexLocker locker(&mutex);
    QString url = buildUrlWithEndpointAndSymbol("quote",symbol);
    QMetaObject::invokeMethod(this, [this, url]() {
        QNetworkRequest request(url);
        QNetworkReply *reply = manager->get(request);
        QMutexLocker locker(&mutex);
        pendingRequests[reply] = {RequestType::QuoteAsync};
    }, Qt::QueuedConnection);

}

bool FMPClient::fetchQuoteSync(const QString &symbol, double &price, double &bid, double &ask) {
    QMutexLocker locker(&mutex);
    QString url = buildUrlWithEndpointAndSymbol("quote",symbol);
    QNetworkReply *reply = nullptr;

    QMetaObject::invokeMethod(this, [this, url, &reply]() {
        QNetworkRequest request(url);
        reply = manager->get(request);
        QMutexLocker locker(&mutex);
        pendingRequests[reply] = {RequestType::QuoteSync};

        qDebug() << Q_FUNC_INFO
                 << " : Thread [" << QThread::currentThread()->objectName()
                 << "] executed the queued method GET and registered the reply " << static_cast<void*>(reply) ;

    }, Qt::QueuedConnection);

    qDebug() << Q_FUNC_INFO
             << " : Thread [" << QThread::currentThread()->objectName()
             << "] invoked the queued method to GET url " << url ;


    if (!waitCondition.wait(&mutex, 5000)) {
        pendingRequests.remove(reply);
        qDebug() << Q_FUNC_INFO << " : The condition wait timed out.";
        return false;
    }

    if (pendingRequests.contains(reply) && pendingRequests[reply].completed) {
        price = pendingRequests[reply].price;
        bid = pendingRequests[reply].bid;
        ask = pendingRequests[reply].ask;

        qDebug() << Q_FUNC_INFO << " : The request " << static_cast<void*>(reply) << " successfuly completed.";

        pendingRequests.remove(reply);

        return true;
    }
    pendingRequests.remove(reply);

    qDebug() << Q_FUNC_INFO << " : The pending requests did not contain the reply, or was not marked as completed.";

    return false;
}

void FMPClient::onThreadStarted() const {
}

void FMPClient::onReplyFinished(QNetworkReply *reply) {
    QMutexLocker locker(&mutex);
    if (!pendingRequests.contains(reply)) {
        reply->deleteLater();
        Q_ASSERT(0);
        return;
    }

    qDebug() << Q_FUNC_INFO
             << " : Thread [" << QThread::currentThread()->objectName()
             << "] working on request " << static_cast<void*>(reply);

    RequestInfo &info = pendingRequests[reply];
    QJsonDocument doc;

    // Those are the default values, just being explicit by resetting them to default
    info.price = 0;
    info.bid = 0;
    info.ask = 0;
    info.completed = false;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << Q_FUNC_INFO << " : Error with the reply";
        goto end;
    }

    doc = QJsonDocument::fromJson(reply->readAll());

    if (doc.isNull() || !doc.isArray()) {
        qDebug() << Q_FUNC_INFO << " : JSON doc is null or not an array";
        goto end;
    }

    if (doc.array().isEmpty()) {
        qDebug() << Q_FUNC_INFO << " : Doc array is empty";
    } else {
        QJsonObject obj = doc.array().first().toObject();
        info.price = obj["price"].toDouble();
        info.bid = obj["bidPrice"].toDouble();
        info.ask = obj["askPrice"].toDouble();
        info.completed = true;
    }

end:
    if (info.type == RequestType::QuoteAsync) {
        emit quoteReceived(info.price, info.bid, info.ask);
        pendingRequests.remove(reply);
    } else if (info.type == RequestType::QuoteSync) {
        waitCondition.wakeOne();
    }

    reply->deleteLater();
}
