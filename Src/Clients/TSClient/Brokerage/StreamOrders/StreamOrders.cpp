#include "StreamOrders.h"
#include "TSClient.h"
#include "Logging.h"

#define LOGGING_CATEGORY StreamLog

StreamOrders::StreamOrders(const QString& accountID, QNetworkReply* reply, QObject* parent)
    : Stream(reply, parent), m_accountID(accountID)
{
    this->setObjectName("Stream::Orders::" + accountID);

    INFO << "Stream created";
}

void StreamOrders::processJsonObject(const QJsonObject& jsonObj)
{
    // First check if this is a status message
    if (jsonObj.contains("StreamStatus")) [[unlikely]]
    {
        QString statusStr = jsonObj["StreamStatus"].toString();
        if (statusStr == "EndSnapshot")
        {
            emit endSnapshotReceived();
        }
        else if (statusStr == "GoAway")
        {
            WARNING << "Received GoAway status for account" << m_accountID;
            //          Q_ASSERT(false); // TODO handle this properly
        }
        else
        {
            WARNING << "Stream status object invalid : "
                    << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        }

        return;
    }

    if (jsonObj.contains("ErrorResponse")) [[unlikely]]
    {
        QString errorStr = jsonObj["Error"].toString();
        QString message = jsonObj["Message"].toString();
        QString accountID = jsonObj["AccountID"].toString();

        m_jsonErrorString = errorStr + ": " + message + " (AccountID: " + accountID + ")";

        CRITICAL << "Received error string '" << errorStr << "' and message: " << message << " for account "
                 << accountID;

        return;
    }

    // If not a status message, try to process as an order update
    qDebug() << "StreamOrders: Processing order JSON:"
             << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));

    Order order(jsonObj,
                m_receivedEndSnapshot); // Pass the update flag based on EndSnapshot status

    if (!order.isValid()) [[unlikely]]
    {
        qCWarning(StreamLog) << "Order update object invalid : "
                             << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        return;
    }

    // Debug: Log the parsed order details
    qDebug() << "StreamOrders: Parsed order - OrderID:" << order.getOrderID() << "Symbol:" << order.getSymbol()
             << "Quantity:" << order.getQuantity() << "TradeAction:" << order.getTradeAction();

    emit newOrderReceived(order);
}