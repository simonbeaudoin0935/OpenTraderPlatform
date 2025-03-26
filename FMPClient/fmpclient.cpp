#include "fmpclient.h"
#include <QNetworkAccessManager>
#include <QThread>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>
#ifdef UNIT_TESTING
#include <QtTest/QtTest>
#endif
// Define the logging category
Q_LOGGING_CATEGORY(FMPClientLog, "FMPClient")

// Initialize static member outside class
FMPClient* FMPClient::instance = nullptr;

QString FMPClient::apiKey = "";

FMPClient& FMPClient::getInstance() {

    if (instance == nullptr) {
        qCDebug(FMPClientLog) << "Singleton instance created";

        instance = new FMPClient();  // Create on first call
    }
    return *instance;
}

void FMPClient::setAPIKey(const QString &apiKey)
{
    FMPClient::apiKey = apiKey;
    qCDebug(FMPClientLog) << "API key set to " << apiKey;
}

FMPClient::FMPClient() :
    thread(new QThread()),
    manager(new QNetworkAccessManager(this))
{
    qCDebug(FMPClientLog) << Q_FUNC_INFO << ": FMPClient created using KEY=" << apiKey;

    Q_ASSERT(!apiKey.isEmpty());

    thread->setObjectName("FPMClientThread");

    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &FMPClient::onThreadStarted);
    connect(manager, &QNetworkAccessManager::finished, this, &FMPClient::onReplyFinished);
    thread->start();
}

FMPClient::~FMPClient() {
    qCDebug(FMPClientLog) << "Singleton instance destroyed";

    thread->quit();
    thread->wait();
}


QString FMPClient::buildUrlWithEndpoint(const QString &endpoint) const {
    return baseUrl + endpoint + "?" +
           QString("apikey=%1").arg(apiKey);
}

QString FMPClient::buildUrlWithEndpointAndSymbol(const QString &endpoint, const QString &symbol) const {
    return baseUrl + endpoint + "?" +
           QString("symbol=%1").arg(symbol) + "&" +
           QString("apikey=%1").arg(apiKey);
}

void FMPClient::fetchQuoteAsync(const QString &symbol) {
    QString url = buildUrlWithEndpointAndSymbol("quote",symbol);

    QMutexLocker locker(&mutex);

    QMetaObject::invokeMethod(this, [this, url]() {
        QNetworkRequest request(url);
        QNetworkReply *reply = manager->get(request);
        QMutexLocker locker(&mutex);
        pendingRequests[reply] = {RequestType::QuoteAsync};
    }, Qt::QueuedConnection);

}

/*
 * Underlying function doing the fetching of the JSON common to all fetchXSync methods
 */
bool FMPClient::fetchSync(const QString &url, QJsonArray *&jsonArrayFromReplyToDelete) {
    QMutexLocker locker(&mutex);
    QNetworkReply *reply = nullptr;

    Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

    QMetaObject::invokeMethod(this, [this, url, &reply]() {
        QNetworkRequest request(url);
        reply = manager->get(request);
        QMutexLocker locker(&mutex);
        pendingRequests[reply] = {RequestType::QuoteSync};

        qCDebug(FMPClientLog) << Q_FUNC_INFO <<
            " : Thread [" << QThread::currentThread()->objectName() <<
            "] executed the queued method GET and registered the reply " << static_cast<void*>(reply) ;

    }, Qt::QueuedConnection);

    qCDebug(FMPClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] invoked the queued method to GET url " << url ;


    if (!waitCondition.wait(&mutex, 5000)) {
        pendingRequests.remove(reply);
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : The condition wait timed out.";
        return false;
    }


    if (pendingRequests.contains(reply) && pendingRequests[reply].completed) {
        jsonArrayFromReplyToDelete = pendingRequests[reply].jsonArray;

        qCDebug(FMPClientLog) << Q_FUNC_INFO << " : The request " << static_cast<void*>(reply) << " successfuly completed.";

        pendingRequests.remove(reply);

        return true;
    }

    jsonArrayFromReplyToDelete = nullptr;

    pendingRequests.remove(reply);

    qWarning() << Q_FUNC_INFO << " : The pending requests did not contain the reply, or was not marked as completed.";

    return false;
}

bool FMPClient::fetchQuoteSync(const QString &symbol, double &price, double &bid, double &ask) {    
    QString url = buildUrlWithEndpointAndSymbol("quote",symbol);
    QJsonArray *jsonArrayFromReplyToDelete;

    bool ret = fetchSync(url, jsonArrayFromReplyToDelete);

    if (ret) {
        // The positive return value implies jsonArrayFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

        QJsonObject obj = jsonArrayFromReplyToDelete->first().toObject();

        Q_ASSERT(symbol == obj["symbol"].toString());

        price = obj["price"].toDouble();
        bid = obj["bidPrice"].toDouble();
        ask = obj["askPrice"].toDouble();

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        delete jsonArrayFromReplyToDelete;
    }

    return ret;
}

bool FMPClient::fetchSharesFloatSync(const QString &symbol, QString &date, double &freeFloat, double &floatShares, double &outstandingShares)
{
    QString url = buildUrlWithEndpointAndSymbol("shares-float",symbol);
    QJsonArray *jsonArrayFromReplyToDelete;

    bool ret = fetchSync(url, jsonArrayFromReplyToDelete);

    if (ret) {
        // The positive return value implies jsonArrayFromReplyToDelete has been allocated to something
        Q_ASSERT(jsonArrayFromReplyToDelete != nullptr);

        QJsonObject obj = jsonArrayFromReplyToDelete->first().toObject();

        Q_ASSERT(symbol == obj["symbol"].toString());

        date = obj["date"].toString();
        freeFloat = obj["freeFloat"].toDouble();
        floatShares = obj["floatShares"].toDouble();
        outstandingShares = obj["outstandingShares"].toDouble();

        // This pointer to a JSON array was allocated in the fetchSync and needs to be deleted after use
        delete jsonArrayFromReplyToDelete;
    }

    return ret;
}

void FMPClient::onThreadStarted() const {
}

void FMPClient::onReplyFinished(QNetworkReply *reply) {
    QMutexLocker locker(&mutex);

#ifdef UNIT_TESTING
    if (introduce_6s_network_latency) {
        QTest::qWait(6000);
    }
#endif

    if (!pendingRequests.contains(reply)) {
        reply->deleteLater();
        Q_ASSERT(0);
        return;
    }

    qCDebug(FMPClientLog) << Q_FUNC_INFO <<
        " : Thread [" << QThread::currentThread()->objectName() <<
        "] working on request " << static_cast<void*>(reply);

    RequestInfo &info = pendingRequests[reply];
    QJsonDocument doc;

    // Those are the default values, just being explicit by resetting them to default
    info.jsonArray = nullptr;
    info.completed = false;

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : Error with the reply";
        goto end;
    }

    doc = QJsonDocument::fromJson(reply->readAll());

    if (doc.isNull() || !doc.isArray()) {
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : JSON doc is null or not an array";
        goto end;
    }

    if (doc.array().isEmpty()) {
        qCWarning(FMPClientLog) << Q_FUNC_INFO << " : Doc array is empty";
    } else {        
        info.jsonArray = new QJsonArray(doc.array());
        info.completed = true;
    }

end:
    if (info.type == RequestType::QuoteAsync) {
        //emit quoteReceived(info.price, info.bid, info.ask);
        emit quoteReceived(-1, -1, -1);
        pendingRequests.remove(reply);
    } else if (info.type == RequestType::QuoteSync) {
        waitCondition.wakeOne();
    }

    reply->deleteLater();
}

