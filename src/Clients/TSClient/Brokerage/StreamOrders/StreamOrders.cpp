
#include "StreamOrders.h"
#include "TSClient.h"

StreamOrders::StreamOrders(QString &accountID, QObject *parent) :
    Stream(parent),
    accountID(accountID)
{
    qFatal();
}

StreamOrders::~StreamOrders()
{
    qFatal();
}

bool StreamOrders::processJsonObject(const QJsonObject &jsonObj)
{
    qFatal();
}


StreamOrders* TSClient::openStreamOrders(QString &accountID) {
    Q_ASSERT(accountID.length() >= 8); // normal account numbers have 8 digits, sim have additional letters

    const QString endpoint = QString(ENDPOINT_STREAM_ORDERS).arg(accountID);

    QUrlQuery query;

    StreamOrders * stream = new StreamOrders(accountID);
    stream->moveToThread(thread);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Opening StreamOrders " << static_cast<void*>(stream);

    TSClient::openStream("NOSYMBOL", endpoint, query, stream);


    return stream;
}

void TSClient::closeStreamOrders(StreamOrders* stream) {
    Q_ASSERT(stream != nullptr);

    qCDebug(TSClientLog) << Q_FUNC_INFO << "Closing StreamOrders " << static_cast<void*>(stream);

    TSClient::closeStream(stream);
}
