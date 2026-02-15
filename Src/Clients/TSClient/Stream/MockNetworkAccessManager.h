#pragma once

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QObject>

Q_DECLARE_LOGGING_CATEGORY(MockNetworkAccessManagerLog)

class OrderEmulator;

/**
 * @class MockNetworkAccessManager
 * @brief Mock QNetworkAccessManager for intercepting API calls in replay mode
 *
 * This class intercepts HTTP requests that would normally go to the TradeStation API
 * and routes them to the OrderEmulator for local processing. This allows replay mode
 * to simulate order execution without making real network requests.
 *
 * Intercepted endpoints:
 * - POST /v3/orderexecution/orders → placeOrder
 * - DELETE /v3/orderexecution/orders/{orderID} → cancelOrder
 * - GET /v3/brokerage/accounts → getAccounts (returns mock account)
 * - GET /v3/brokerage/accounts/{accountID}/balances → getBalances
 *
 * Threading: Created and used in TSClient thread
 */
class MockNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

  public:
    /**
     * @brief Construct MockNetworkAccessManager
     * @param p_emulator Reference to OrderEmulator for routing orders
     * @param p_parent Parent object (typically TSClient)
     */
    explicit MockNetworkAccessManager(OrderEmulator* p_emulator, QObject* p_parent = nullptr);
    ~MockNetworkAccessManager() override;

    Q_DISABLE_COPY_MOVE(MockNetworkAccessManager)

  protected:
    /**
     * @brief Override createRequest to intercept network requests
     */
    QNetworkReply* createRequest(Operation p_op,
                                  const QNetworkRequest& p_request,
                                  QIODevice* p_outgoingData) override;

  private:
    /**
     * @brief Handle POST requests (placeOrder)
     */
    QNetworkReply* handlePost(const QNetworkRequest& p_request, QIODevice* p_outgoingData);

    /**
     * @brief Handle DELETE requests (cancelOrder)
     */
    QNetworkReply* handleDelete(const QNetworkRequest& p_request);

    /**
     * @brief Handle GET requests (getAccounts, getBalances)
     */
    QNetworkReply* handleGet(const QNetworkRequest& p_request);

    /**
     * @brief Create a mock reply with given JSON response
     */
    QNetworkReply* createMockReply(const QByteArray& p_data);

    /**
     * @brief Extract account ID from URL path
     * @param p_path URL path (e.g., /v3/brokerage/accounts/SIM123456/balances)
     * @return Account ID or empty string if not found
     */
    QString extractAccountIdFromPath(const QString& p_path);

    /**
     * @brief Create a mock error reply
     * @param p_error Error code
     * @param p_message Human-readable error message
     */
    QNetworkReply* createErrorReply(const QString& p_error, const QString& p_message);

    OrderEmulator* m_emulator; // Not owned
};
