#include "MockNetworkAccessManager.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include "Assume.h"
#include "Logging.h"
#include "MockNetworkReply.h"
#include "OrderEmulator.h"
#include "PlaceOrder.h"

#define LOGGING_CATEGORY MockNetworkAccessManagerLog
Q_LOGGING_CATEGORY(MockNetworkAccessManagerLog, "MockNetworkAccessManager");

MockNetworkAccessManager::MockNetworkAccessManager(OrderEmulator* p_emulator, QObject* p_parent)
    : QNetworkAccessManager(p_parent), m_emulator(p_emulator)
{
    OBJ_ASSUME_DIFF(p_emulator, nullptr);
    setObjectName("MockNetworkAccessManager");
    INFO << "MockNetworkAccessManager created";
}

MockNetworkAccessManager::~MockNetworkAccessManager()
{
    DEBUG << "MockNetworkAccessManager destroyed";
}

QNetworkReply*
MockNetworkAccessManager::createRequest(Operation p_op, const QNetworkRequest& p_request, QIODevice* p_outgoingData)
{
    QString path = p_request.url().path();
    DEBUG << "Intercepting request:" << p_op << path;

    switch (p_op)
    {
    case PostOperation:
        return handlePost(p_request, p_outgoingData);

    case DeleteOperation:
        return handleDelete(p_request);

    case GetOperation:
        return handleGet(p_request);

    default:
        WARNING << "Unhandled operation type:" << p_op << "for path:" << path;
        // Fall through to base implementation for unhandled operations
        return QNetworkAccessManager::createRequest(p_op, p_request, p_outgoingData);
    }
}

QNetworkReply* MockNetworkAccessManager::handlePost(const QNetworkRequest& p_request, QIODevice* p_outgoingData)
{
    QString path = p_request.url().path();

    // Handle placeOrder
    if (path.contains("/orderexecution/orders"))
    {
        // Read request body
        QByteArray body;
        if (p_outgoingData)
        {
            body = p_outgoingData->readAll();
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject())
        {
            WARNING << "Invalid JSON in placeOrder request";
            QJsonObject errorResp;
            errorResp["Error"] = "Invalid request body";
            return createMockReply(QJsonDocument(errorResp).toJson());
        }

        QJsonObject requestObj = doc.object();

        // Parse PlaceOrderRequest from JSON
        PlaceOrderRequest orderRequest;
        orderRequest.setAccountID(requestObj["AccountID"].toString());
        orderRequest.setSymbol(requestObj["Symbol"].toString());
        orderRequest.setQuantity(requestObj["Quantity"].toString().toInt());

        // Parse trade action
        QString tradeActionStr = requestObj["TradeAction"].toString();
        if (tradeActionStr == "BUY")
        {
            orderRequest.setTradeAction(TradeAction::Buy);
        }
        else if (tradeActionStr == "SELL")
        {
            orderRequest.setTradeAction(TradeAction::Sell);
        }
        else if (tradeActionStr == "BUYTOCOVER")
        {
            orderRequest.setTradeAction(TradeAction::BuyToCover);
        }
        else if (tradeActionStr == "SELLSHORT")
        {
            orderRequest.setTradeAction(TradeAction::SellShort);
        }

        // Parse order type
        QString orderTypeStr = requestObj["OrderType"].toString();
        DEBUG << "Parsing order type:" << orderTypeStr;

        if (orderTypeStr == "Limit")
        {
            orderRequest.setOrderType(OrderType::Type::Limit);
            if (requestObj.contains("LimitPrice"))
            {
                orderRequest.setLimitPrice(requestObj["LimitPrice"].toString().toDouble());
            }
        }
        else if (orderTypeStr == "StopMarket")
        {
            orderRequest.setOrderType(OrderType::Type::StopMarket);
            if (requestObj.contains("StopPrice"))
            {
                orderRequest.setStopPrice(requestObj["StopPrice"].toString().toDouble());
            }
        }
        else if (orderTypeStr == "StopLimit")
        {
            orderRequest.setOrderType(OrderType::Type::StopLimit);
            if (requestObj.contains("LimitPrice"))
            {
                orderRequest.setLimitPrice(requestObj["LimitPrice"].toString().toDouble());
            }
            if (requestObj.contains("StopPrice"))
            {
                orderRequest.setStopPrice(requestObj["StopPrice"].toString().toDouble());
            }
        }
        else if (orderTypeStr == "Market")
        {
            orderRequest.setOrderType(OrderType::Type::Market);
        }
        else
        {
            WARNING << "Unknown order type:" << orderTypeStr << "- defaulting to Market";
            orderRequest.setOrderType(OrderType::Type::Market);
        }

        // Generate request ID
        QString requestID = QString::number(QDateTime::currentMSecsSinceEpoch());

        // Route to emulator
        m_emulator->placeOrder(orderRequest, requestID);

        // Return immediate acknowledgment
        QJsonObject response;
        QJsonArray orders;
        QJsonObject orderResult;
        orderResult["OrderID"] = requestID;
        orderResult["Message"] = "Order received for processing";
        orders.append(orderResult);
        response["Orders"] = orders;

        INFO << "Order routed to emulator:" << orderRequest.getSymbol() << "qty:" << orderRequest.getQuantity();

        return createMockReply(QJsonDocument(response).toJson());
    }

    WARNING << "Unhandled POST path:" << path;
    return createMockReply("{}");
}

QNetworkReply* MockNetworkAccessManager::handleDelete(const QNetworkRequest& p_request)
{
    QString path = p_request.url().path();

    // Handle cancelOrder: /v3/orderexecution/orders/{orderID}
    if (path.contains("/orderexecution/orders/"))
    {
        // Extract order ID from path
        QStringList parts = path.split('/');
        QString orderID = parts.last();

        if (orderID.isEmpty())
        {
            WARNING << "Empty order ID in cancel request";
            QJsonObject errorResp;
            errorResp["Error"] = "Order ID is required";
            return createMockReply(QJsonDocument(errorResp).toJson());
        }

        QString requestID = QString::number(QDateTime::currentMSecsSinceEpoch());

        // Route to emulator
        m_emulator->cancelOrder(orderID, requestID);

        QJsonObject response;
        response["OrderID"] = orderID;
        response["Message"] = "Cancel request received";

        INFO << "Cancel routed to emulator:" << orderID;

        return createMockReply(QJsonDocument(response).toJson());
    }

    WARNING << "Unhandled DELETE path:" << path;
    return createMockReply("{}");
}

QNetworkReply* MockNetworkAccessManager::handleGet(const QNetworkRequest& p_request)
{
    QString path = p_request.url().path();

    // Handle getAccounts: /v3/brokerage/accounts
    if (path.contains("/brokerage/accounts") && !path.contains("/balances"))
    {
        QJsonArray accounts;
        QJsonObject account;
        account["AccountID"] = OrderEmulator::getSimulatedAccountID();
        account["AccountType"] = "Margin";
        account["Name"] = "Simulated Account";
        account["Status"] = "Active";
        accounts.append(account);

        QJsonObject response;
        response["Accounts"] = accounts;

        DEBUG << "Returning mock account:" << OrderEmulator::getSimulatedAccountID();

        return createMockReply(QJsonDocument(response).toJson());
    }

    // Handle getBalances: /v3/brokerage/accounts/{accounts}/balances
    if (path.contains("/balances"))
    {
        // Validate account ID matches simulated account
        QString accountId = extractAccountIdFromPath(path);
        if (accountId != OrderEmulator::getSimulatedAccountID())
        {
            return createErrorReply("INVALID_ACCOUNT",
                                    QString("Account '%1' not found in replay mode. Use '%2'.")
                                        .arg(accountId, OrderEmulator::getSimulatedAccountID()));
        }

        double cashBalance = m_emulator->getBalance();
        double closedPnL = m_emulator->getClosedPositionsPnL();

        // Calculate total market value and unrealized P&L from ACTIVE positions
        double totalMarketValue = 0.0;
        double totalUnrealizedPnL = 0.0;

        QVector<Position> positions = m_emulator->getPositions();
        for (const Position& position: positions)
        {
            totalMarketValue += position.getMarketValue().toDouble();
            totalUnrealizedPnL += position.getUnrealizedProfitLoss().toDouble();
        }

        // Calculate equity (cash + market value)
        double equity = cashBalance + totalMarketValue;

        // Calculate buying power (2x equity for margin account)
        double buyingPower = equity * 2.0;

        // Total today's P&L = closed positions P&L + open positions unrealized P&L
        double todaysPnL = closedPnL + totalUnrealizedPnL;

        QJsonArray balances;
        QJsonObject balanceObj;
        balanceObj["AccountID"] = OrderEmulator::getSimulatedAccountID();
        balanceObj["AccountType"] = "Margin";
        balanceObj["CashBalance"] = QString::number(cashBalance, 'f', 2);
        balanceObj["BuyingPower"] = QString::number(buyingPower, 'f', 2);
        balanceObj["Equity"] = QString::number(equity, 'f', 2);
        balanceObj["MarketValue"] = QString::number(totalMarketValue, 'f', 2);
        balanceObj["RealizedProfitLoss"] = QString::number(closedPnL, 'f', 2);
        balanceObj["UnrealizedProfitLoss"] = QString::number(totalUnrealizedPnL, 'f', 2);
        balanceObj["TodaysProfitLoss"] = QString::number(todaysPnL, 'f', 2);
        balanceObj["Comission"] = "0.00";
        balanceObj["UnclearedDeposit"] = "0.00";
        balances.append(balanceObj);

        QJsonObject response;
        response["Balances"] = balances;

        return createMockReply(QJsonDocument(response).toJson());
    }

    WARNING << "Unhandled GET path:" << path;
    return createMockReply("{}");
}

QNetworkReply* MockNetworkAccessManager::createMockReply(const QByteArray& p_data)
{
    auto* reply = new MockNetworkReply(this);
    reply->setProperty("mockData", p_data);

    // Schedule data injection after reply is connected
    QMetaObject::invokeMethod(
        reply,
        [reply, p_data]()
        {
            reply->injectData(p_data);
            emit reply->finished();
        },
        Qt::QueuedConnection);

    return reply;
}

QString MockNetworkAccessManager::extractAccountIdFromPath(const QString& p_path)
{
    // Pattern: /v3/brokerage/accounts/{accountId}/...
    QRegularExpression re("/accounts/([^/]+)");
    QRegularExpressionMatch match = re.match(p_path);
    if (match.hasMatch())
    {
        return match.captured(1);
    }
    return QString();
}

QNetworkReply* MockNetworkAccessManager::createErrorReply(const QString& p_error, const QString& p_message)
{
    QJsonObject errorObj;
    errorObj["Error"] = p_error;
    errorObj["Message"] = p_message;

    WARNING << "Returning error response:" << p_error << "-" << p_message;

    return createMockReply(QJsonDocument(errorObj).toJson());
}
