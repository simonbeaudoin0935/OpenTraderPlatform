#include "StreamOrders.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

#define LOGGING_CATEGORY StreamLog

// Initialize static counter
size_t StreamOrders::s_numberOfOrderStreams = 0;

StreamOrders::StreamOrders(const QString& accountID, QNetworkReply* reply, QObject* parent)
    : StreamBrokerage(reply, parent), m_accountID(accountID)
{
    this->setObjectName("Stream::Orders::" + accountID);

    // Assert that we're not creating a second orders stream
    OBJ_ASSUME_EQUAL(s_numberOfOrderStreams, 0u);

    s_numberOfOrderStreams++;

    DEBUG << "Stream created - Total order streams:" << s_numberOfOrderStreams;
}

StreamOrders::~StreamOrders()
{
    s_numberOfOrderStreams--;

    DEBUG << "Stream destroyed - Total order streams:" << s_numberOfOrderStreams;
}

void StreamOrders::processJsonObject(const QJsonObject& jsonObj)
{
    // If not a status message, try to process as an order update
    DEBUG << "StreamOrders: Processing order JSON:" << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));

    Order order(jsonObj,
                m_receivedEndSnapshot); // Pass the update flag based on EndSnapshot status

    if (!order.isValid()) [[unlikely]]
    {
        CRITICAL << "Order update object invalid : " << QString(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented));
        return;
    }

    // Debug: Log the parsed order details
    DEBUG << "StreamOrders: Parsed order - OrderID:" << order.getOrderID() << "Symbol:" << order.getSymbol()
          << "Quantity:" << order.getQuantity() << "TradeAction:" << order.getTradeAction();

    emit newOrderReceived(order);
}